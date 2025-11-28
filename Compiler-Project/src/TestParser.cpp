#include "Parser.h"
#include "llvm/Support/raw_ostream.h"

// یک ویزیتور ساده برای چاپ درخت
class ASTPrinter : public ASTVisitor {
public:
    void visit(AST &) override {}
    void visit(Block &B) override {
        llvm::outs() << "Block {\n";
        for (auto *S : B.getStatements()) S->accept(*this);
        llvm::outs() << "}\n";
    }
    void visit(Declaration &D) override {
        llvm::outs() << "  Declaration: " << D.getName() << " type=" << D.getType() << "\n";
    }
    void visit(Assignment &A) override {
        llvm::outs() << "  Assignment: " << A.getName() << "\n";
    }
    void visit(PrintStmt &P) override {
        llvm::outs() << "  Print Statement\n";
    }
    void visit(BinaryOp &) override {}
    void visit(Final &) override {}
    void visit(IfStmt &) override {}
    void visit(ForStmt &) override {}
    void visit(ForEachStmt &) override {
        llvm::outs() << "  ForEach Loop\n";
    }
};

int main() {
    // کد نمونه برای تست پارسر
    llvm::StringRef Input =
        "var a int = 5;\n"
        "ADD a 10 20;\n"
        "print(a);";

    Lexer L(Input);
    Parser P(L);

    llvm::outs() << "--- Start Parser Test ---\n";
    Block *Program = P.parse();

    if (P.hasError()) {
        llvm::errs() << "Parsing Failed!\n";
        return 1;
    }

    llvm::outs() << "Parsing Successful! AST Structure:\n";
    ASTPrinter Printer;
    Program->accept(Printer);

    return 0;
}