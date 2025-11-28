#include "Lexer.h"
#include "Parser.h"
#include "AST.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <string>

// --- Helper class to print AST structure ---
class ASTPrinter : public ASTVisitor {
    int IndentLevel = 0;

    void printIndent() {
        for (int i = 0; i < IndentLevel; ++i) std::cout << "  | ";
    }

    void printLocation(AST &Node) {
        std::cout << " [Line: " << Node.getLine() << ", Col: " << Node.getCol() << "]";
    }

    std::string getOpString(BinaryOp::Operator Op) {
        switch (Op) {
            case BinaryOp::Plus: return "+ (ADD)";
            case BinaryOp::Minus: return "- (SUB)";
            case BinaryOp::Mul: return "* (MUL)";
            case BinaryOp::Div: return "/ (DIV)";
            case BinaryOp::Mod: return "% (MOD)";
            case BinaryOp::And: return "&& (AND)";
            case BinaryOp::Or: return "|| (OR)";
            case BinaryOp::Eq: return "==";
            case BinaryOp::Neq: return "!=";
            case BinaryOp::Lt: return "<";
            case BinaryOp::Gt: return ">";
            case BinaryOp::Lte: return "<=";
            case BinaryOp::Gte: return ">=";
            default: return "Unknown";
        }
    }

public:
    void visit(AST &Node) override {
        // Fallback
    }

    void visit(Block &Node) override {
        printIndent();
        std::cout << "Block";
        printLocation(Node);
        std::cout << std::endl;

        IndentLevel++;
        for (auto *Stmt : Node.getStatements()) {
            Stmt->accept(*this);
        }
        IndentLevel--;
    }

    void visit(Declaration &Node) override {
        printIndent();
        std::cout << "Declaration: " << Node.getName().str() << " (" << Node.getType().str() << ")";
        printLocation(Node);
        std::cout << std::endl;

        if (Node.getInit()) {
            IndentLevel++;
            printIndent(); std::cout << "init:" << std::endl;
            Node.getInit()->accept(*this);
            IndentLevel--;
        }
    }

    void visit(Assignment &Node) override {
        printIndent();
        std::cout << "Assignment: " << Node.getName().str() << " =";
        printLocation(Node);
        std::cout << std::endl;

        IndentLevel++;
        Node.getValue()->accept(*this);
        IndentLevel--;
    }

    void visit(BinaryOp &Node) override {
        printIndent();
        std::cout << "BinaryOp: " << getOpString(Node.getOperator());
        printLocation(Node);
        std::cout << std::endl;

        IndentLevel++;
        Node.getLeft()->accept(*this);
        Node.getRight()->accept(*this);
        IndentLevel--;
    }

    void visit(Final &Node) override {
        printIndent();
        std::cout << "Final: " << Node.getValue().str();
        switch (Node.getKind()) {
            case Final::Number: std::cout << " (Number)"; break;
			case Final::Float:  std::cout << " (Float)"; break;
            case Final::Ident: std::cout << " (Ident)"; break;
            case Final::String: std::cout << " (String)"; break;
            case Final::Bool: std::cout << " (Bool)"; break;
            case Final::Underscore: std::cout << " (_)"; break;
        }
        printLocation(Node);
        std::cout << std::endl;
    }

    void visit(IfStmt &Node) override {
        printIndent();
        std::cout << "If Statement";
        printLocation(Node);
        std::cout << std::endl;

        IndentLevel++;
        printIndent(); std::cout << "Condition:" << std::endl;
        Node.getCond()->accept(*this);
        printIndent(); std::cout << "Then Block:" << std::endl;
        Node.getThen()->accept(*this);
        
        for (auto &Elif : Node.getElifs()) {
            printIndent(); std::cout << "Elif:" << std::endl;
            Elif.first->accept(*this);
            Elif.second->accept(*this);
        }

        if (Node.getElse()) {
            printIndent(); std::cout << "Else Block:" << std::endl;
            Node.getElse()->accept(*this);
        }
        IndentLevel--;
    }

    void visit(ForStmt &Node) override {
        printIndent();
        std::cout << "For Loop";
        printLocation(Node);
        std::cout << std::endl;
        // Simplified print for brevity
        IndentLevel++;
        Node.getBody()->accept(*this);
        IndentLevel--;
    }

    void visit(ForEachStmt &Node) override {
        printIndent();
        std::cout << "ForEach Loop: " << Node.getIterator().str() << " inside " << Node.getCollection().str();
        printLocation(Node);
        std::cout << std::endl;
        
        IndentLevel++;
        Node.getBody()->accept(*this);
        IndentLevel--;
    }

    void visit(PrintStmt &Node) override {
        printIndent();
        std::cout << "Print";
        printLocation(Node);
        std::cout << std::endl;

        IndentLevel++;
        Node.getArg()->accept(*this);
        IndentLevel--;
    }

    void visit(ArrayLiteral &Node) override {
        printIndent();
        std::cout << "Array Literal []";
        printLocation(Node);
        std::cout << std::endl;
        
        IndentLevel++;
        for(auto *Val : Node.getValues()) {
            Val->accept(*this);
        }
        IndentLevel--;
    }

    void visit(BuiltinCall &Node) override {
        printIndent();
        std::cout << "Builtin Call: " << Node.getName();
        printLocation(Node);
        std::cout << std::endl;
        
        IndentLevel++;
        for(auto *Arg : Node.getArgs()) {
            Arg->accept(*this);
        }
        IndentLevel--;
    }

    void visit(MatchStmt &Node) override {
        printIndent();
        std::cout << "Match Statement";
        printLocation(Node);
        std::cout << std::endl;

        IndentLevel++;
        printIndent(); std::cout << "Target:" << std::endl;
        Node.getTarget()->accept(*this);
        
        for (auto &Case : Node.getCases()) {
            printIndent(); std::cout << "Case Pattern:" << std::endl;
            Case.first->accept(*this);
            printIndent(); std::cout << "Case Body:" << std::endl;
            Case.second->accept(*this);
        }
        IndentLevel--;
    }

    void visit(CompoundStmt &Node) override {
        printIndent();
        std::string op = (Node.getOperator() == CompoundStmt::PLE) ? "+= (PLE)" : "-= (MIE)";
        std::cout << "Compound: " << Node.getName().str() << " " << op;
        printLocation(Node);
        std::cout << std::endl;
        
        IndentLevel++;
        Node.getValue()->accept(*this);
        IndentLevel--;
    }

    void visit(UnaryStmt &Node) override {
        printIndent();
        std::string op = (Node.getOperator() == UnaryStmt::INC) ? "++ (INC)" : "-- (DEC)";
        std::cout << "Unary: " << Node.getName().str() << " " << op;
        printLocation(Node);
        std::cout << std::endl;
    }

    void visit(RangeExpr &Node) override {
        printIndent();
        std::cout << "List Comprehension: [ ... for " << Node.getIterator().str() 
                  << " in " << Node.getList().str() << " ]";
        printLocation(Node);
        std::cout << std::endl;
        
        IndentLevel++;
        printIndent(); std::cout << "Expression:" << std::endl;
        Node.getTargetExpr()->accept(*this);
        if (Node.getCondition()) {
            printIndent(); std::cout << "Condition:" << std::endl;
            Node.getCondition()->accept(*this);
        }
        IndentLevel--;
    }
};

int main() {
    // کد تست شامل تمام ویژگی‌های جدید
    llvm::StringRef inputCode =
        "var x int = 10;\n"
        "ADD res x 5;\n"
        "PLE res 2;\n"
        "INC x;\n"
        "array list = [1, 2, 3];\n"
        "\n"
        "foreach (item in list) {\n"
        "    print(item);\n"
        "}\n"
        "\n"
        "match x {\n"
        "    1 -> print(\"One\"),\n"
        "    _ -> print(\"Default\")\n"
        "}\n"
        "\n"
        "array evens = [x * 2 for x in list if x > 5];";

    Lexer Lex(inputCode);
    Parser Par(Lex);
    
    std::cout << "--- STARTING PARSER TEST ---" << std::endl;
    
    Block *Program = Par.parse();

    if (Par.hasError() || !Program) {
        std::cout << "❌ Parser Failed!" << std::endl;
        return 1;
    }

    std::cout << "✅ Parse Successful! Printing AST:\n" << std::endl;

    ASTPrinter Printer;
    Program->accept(Printer);

    return 0;
}