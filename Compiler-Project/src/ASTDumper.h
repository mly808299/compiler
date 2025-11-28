#ifndef ASTDUMPER_H
#define ASTDUMPER_H

#include "AST.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

class ASTDumper : public ASTVisitor {
    llvm::raw_ostream &OS;
    int Indent;

    void indent() {
        for (int i = 0; i < Indent; ++i) OS << "  ";
    }

    void printKey(llvm::StringRef Key) {
        indent();
        OS << "\"" << Key << "\": ";
    }

public:
    ASTDumper(llvm::raw_ostream &OS) : OS(OS), Indent(0) {}

    void dump(AST *Node) {
        if (Node) Node->accept(*this);
    }

    // Helper to wrap object
    void visit(AST &Node) override {}

    void visit(Block &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"Block\",\n";
        printKey("body"); OS << "[\n";

        for (size_t i = 0; i < Node.getStatements().size(); ++i) {
            Indent++; indent();
            Node.getStatements()[i]->accept(*this);
            if (i < Node.getStatements().size() - 1) OS << ",";
            OS << "\n";
            Indent--;
        }

        indent(); OS << "]\n";
        Indent--; indent(); OS << "}";
    }

    void visit(Declaration &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"Declaration\",\n";
        printKey("name"); OS << "\"" << Node.getName() << "\",\n";
        printKey("type"); OS << "\"" << Node.getType() << "\"";

        if (Node.getInit()) {
            OS << ",\n";
            printKey("init");
            Node.getInit()->accept(*this);
        }
        OS << "\n";
        Indent--; indent(); OS << "}";
    }

    void visit(Assignment &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"Assignment\",\n";
        printKey("target"); OS << "\"" << Node.getName() << "\",\n";
        printKey("value");
        Node.getValue()->accept(*this);
        OS << "\n";
        Indent--; indent(); OS << "}";
    }

    void visit(BinaryOp &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"BinaryOp\",\n";

        std::string opStr;
        switch(Node.getOperator()) {
            case BinaryOp::Plus: opStr = "+"; break;
            case BinaryOp::Minus: opStr = "-"; break;
            case BinaryOp::Mul: opStr = "*"; break;
            case BinaryOp::Div: opStr = "/"; break;
            default: opStr = "Op"; break;
        }
        printKey("operator"); OS << "\"" << opStr << "\",\n";

        printKey("left"); Node.getLeft()->accept(*this); OS << ",\n";
        printKey("right"); Node.getRight()->accept(*this);
        OS << "\n";
        Indent--; indent(); OS << "}";
    }

    void visit(Final &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"Final\",\n";
        printKey("value"); OS << "\"" << Node.getValue() << "\"";
        OS << "\n";
        Indent--; indent(); OS << "}";
    }

    // برای سایر نودها پیاده‌سازی ساده (برای کوتاه شدن کد، بقیه را هم مشابه بالا می‌توان تکمیل کرد)
    void visit(PrintStmt &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"PrintStmt\",\n";
        printKey("arg");
        Node.getArg()->accept(*this);
        OS << "\n";
        Indent--; indent(); OS << "}";
    }

    // پیاده‌سازی‌های خالی یا ساده برای جلوگیری از ارور کامپایل
    void visit(IfStmt &Node) { OS << "{\"kind\": \"IfStmt\"}"; }
    void visit(ForStmt &Node) { OS << "{\"kind\": \"ForStmt\"}"; }
    void visit(ForEachStmt &Node) { OS << "{\"kind\": \"ForEachStmt\"}"; }
    void visit(ArrayLiteral &Node) { OS << "{\"kind\": \"ArrayLiteral\"}"; }
    void visit(BuiltinCall &Node) { OS << "{\"kind\": \"BuiltinCall\"}"; }
    void visit(MatchStmt &Node) { OS << "{\"kind\": \"MatchStmt\"}"; }
    void visit(CompoundStmt &Node) { OS << "{\"kind\": \"CompoundStmt\"}"; }
    void visit(UnaryStmt &Node) { OS << "{\"kind\": \"UnaryStmt\"}"; }
    void visit(RangeExpr &Node) { OS << "{\"kind\": \"RangeExpr\"}"; }
};

#endif