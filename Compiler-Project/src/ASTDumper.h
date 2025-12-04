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

        if (Node.getIndex()) {
            printKey("index");
            Node.getIndex()->accept(*this);
            OS << ",\n";
        }

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

    void visit(PrintStmt &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"PrintStmt\",\n";
        printKey("arg");
        Node.getArg()->accept(*this);
        OS << "\n";
        Indent--; indent(); OS << "}";
    }

    // --- توابع جدید (با override) ---

    void visit(ArrayAccess &Node) override {
        OS << "{\n"; Indent++;
        printKey("kind"); OS << "\"ArrayAccess\",\n";
        printKey("name"); OS << "\"" << Node.getName() << "\",\n";
        printKey("index");
        Node.getIndex()->accept(*this);
        OS << "\n";
        Indent--; indent(); OS << "}";
    }

    void visit(IfStmt &Node) override { OS << "{\"kind\": \"IfStmt\"}"; }
    void visit(ForStmt &Node) override { OS << "{\"kind\": \"ForStmt\"}"; }
    void visit(ForEachStmt &Node) override { OS << "{\"kind\": \"ForEachStmt\"}"; }
    void visit(ArrayLiteral &Node) override { OS << "{\"kind\": \"ArrayLiteral\"}"; }
    void visit(BuiltinCall &Node) override { OS << "{\"kind\": \"BuiltinCall\"}"; }
    void visit(MatchStmt &Node) override { OS << "{\"kind\": \"MatchStmt\"}"; }
    void visit(CompoundStmt &Node) override { OS << "{\"kind\": \"CompoundStmt\"}"; }
    void visit(UnaryStmt &Node) override { OS << "{\"kind\": \"UnaryStmt\"}"; }
    void visit(RangeExpr &Node) override { OS << "{\"kind\": \"RangeExpr\"}"; }
};

#endif