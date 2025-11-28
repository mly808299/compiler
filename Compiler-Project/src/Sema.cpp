#include "Sema.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringExtras.h"

namespace {

class InputCheck : public ASTVisitor {
    llvm::StringMap<llvm::StringRef> SymbolTable;
    bool HasError;
    llvm::StringRef CurrentExprType;
    llvm::StringRef SourceCode;

    void printError(int Line, int Col, const llvm::Twine &Msg) {
        HasError = true;
        llvm::errs() << "input.txt:" << Line << ":" << Col << ": error: " << Msg << "\n";

        llvm::SmallVector<llvm::StringRef, 100> Lines;
        SourceCode.split(Lines, '\n');
        if (Line > 0 && Line <= Lines.size()) {
            llvm::StringRef CodeLine = Lines[Line - 1];
            llvm::errs() << "    " << CodeLine << "\n";
            llvm::errs() << "    ";
            for (int i = 1; i < Col; ++i) llvm::errs() << " ";
            llvm::errs() << "^\n";
        }
        llvm::errs() << "\n";
    }

    void error(AST &Node, const llvm::Twine &Msg) {
        printError(Node.getLine(), Node.getCol(), Msg);
    }

    // تابع جدید: ارور دادن روی یک نود خاص (مثل سمت راست تساوی)
    void errorOnNode(AST *Node, const llvm::Twine &Msg) {
        if (Node) error(*Node, Msg);
    }

public:
    InputCheck(llvm::StringRef Code) : HasError(false), CurrentExprType("void"), SourceCode(Code) {}

    bool hasError() const { return HasError; }
    virtual void visit(AST &Node) override {}

    virtual void visit(Block &Node) override {
        for (auto *Stmt : Node.getStatements()) if (Stmt) Stmt->accept(*this);
    }

    virtual void visit(Declaration &Node) override {
        llvm::StringRef Name = Node.getName();
        llvm::StringRef Type = Node.getType();

        if (SymbolTable.count(Name)) {
            error(Node, "Variable '" + Name + "' is already declared."); // اینجا آدرس دقیق روی نام است
            return;
        }

        if (Node.getInit()) {
            Node.getInit()->accept(*this);
            if (CurrentExprType != "unknown" && CurrentExprType != Type) {
                if (Type != "array" && !(Type == "float" && CurrentExprType == "int")) {
                     // خطا را روی مقدار اولیه نشان بده، نه روی کل تعریف
                     errorOnNode(Node.getInit(), "Type mismatch in declaration. Expected " + Type + ", got " + CurrentExprType);
                }
            }
        }
        SymbolTable[Name] = Type;
    }

    virtual void visit(Assignment &Node) override {
        llvm::StringRef Name = Node.getName();
        if (SymbolTable.find(Name) == SymbolTable.end()) {
            error(Node, "Variable '" + Name + "' is used but not declared.");
            return;
        }
        llvm::StringRef ExpectedType = SymbolTable[Name];

        if (Node.getValue()) {
            Node.getValue()->accept(*this);
            if (CurrentExprType != "unknown" && CurrentExprType != ExpectedType) {
                 if (!(ExpectedType == "float" && CurrentExprType == "int")) {
                    // خطا روی مقدار سمت راست
                    errorOnNode(Node.getValue(), "Type mismatch. Expected " + ExpectedType + ", got " + CurrentExprType);
                }
            }
        }
    }

    virtual void visit(BinaryOp &Node) override {
        Node.getLeft()->accept(*this);
        llvm::StringRef LeftType = CurrentExprType;
        Node.getRight()->accept(*this);
        llvm::StringRef RightType = CurrentExprType;

        BinaryOp::Operator Op = Node.getOperator();

        if (Op == BinaryOp::Div || Op == BinaryOp::Mod) {
            if (auto *Num = dynamic_cast<Final*>(Node.getRight())) {
                if (Num->getKind() == Final::Number && Num->getValue() == "0") {
                    // خطا دقیقاً زیر عدد 0
                    errorOnNode(Node.getRight(), "Division by zero detected.");
                }
            }
        }

        if (LeftType != RightType) {
             if (!((LeftType == "int" && RightType == "float") || (LeftType == "float" && RightType == "int"))) {
                 // خطا روی کل عملیات (مکان عملگر)
                 error(Node, "Type mismatch: " + LeftType + " vs " + RightType);
                 CurrentExprType = "unknown";
             }
        } else {
            CurrentExprType = LeftType;
        }
    }

    // ... بقیه توابع (مشابه قبل، بدون تغییر عمده) ...

    virtual void visit(Final &Node) override {
        if (Node.getKind() == Final::Ident) {
            if (SymbolTable.find(Node.getValue()) == SymbolTable.end()) {
                error(Node, "Undefined variable '" + Node.getValue() + "'"); // دقیقاً زیر متغیر
                CurrentExprType = "unknown";
            } else {
                CurrentExprType = SymbolTable[Node.getValue()];
            }
        } else {
            switch (Node.getKind()) {
                case Final::Number: CurrentExprType = "int"; break;
                case Final::Float:  CurrentExprType = "float"; break;
                case Final::Bool:   CurrentExprType = "bool"; break;
                default: CurrentExprType = "string"; break;
            }
        }
    }

    // برای کوتاهی کد بقیه توابع مثل قبل هستند
    virtual void visit(CompoundStmt &Node) override { Node.getValue()->accept(*this); }
    virtual void visit(UnaryStmt &Node) override {}
    virtual void visit(MatchStmt &Node) override { for(auto &C : Node.getCases()) C.second->accept(*this); }
    virtual void visit(RangeExpr &Node) override {}
    virtual void visit(IfStmt &Node) override { Node.getThen()->accept(*this); }
    virtual void visit(ForStmt &Node) override { Node.getBody()->accept(*this); }
    virtual void visit(ForEachStmt &Node) override {
        SymbolTable[Node.getIterator()] = "int";
        Node.getBody()->accept(*this);
        SymbolTable.erase(Node.getIterator());
    }
    virtual void visit(PrintStmt &Node) override { Node.getArg()->accept(*this); }
    virtual void visit(ArrayLiteral &Node) override { CurrentExprType = "array"; }
    virtual void visit(BuiltinCall &Node) override { CurrentExprType = "int"; }
};
}

bool Sema::semantic(AST *Tree, llvm::StringRef InputCode) {
    if (!Tree) return false;
    InputCheck Checker(InputCode);
    Tree->accept(Checker);
    return Checker.hasError();
}