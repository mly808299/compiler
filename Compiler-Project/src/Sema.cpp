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
             } else {
                 // int <-> float combination: result is float
                 CurrentExprType = "float";
             }
        } else {
            CurrentExprType = LeftType;
        }
    }

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
    virtual void visit(MatchStmt &Node) override { for(auto &C : Node.getCases()) { if (C.first) C.first->accept(*this); if (C.second) C.second->accept(*this); } }
    virtual void visit(RangeExpr &Node) override {}
    virtual void visit(IfStmt &Node) override {
        if (Node.getCond()) {
            Node.getCond()->accept(*this);
            if (CurrentExprType != "bool") {
                errorOnNode(Node.getCond(), "Condition in if must be boolean, got " + CurrentExprType);
            }
        }
        if (Node.getThen()) Node.getThen()->accept(*this);
        for (auto &E : Node.getElifs()) {
            if (E.first) {
                E.first->accept(*this);
                if (CurrentExprType != "bool") {
                    errorOnNode(E.first, "Condition in elif must be boolean, got " + CurrentExprType);
                }
            }
            if (E.second) E.second->accept(*this);
        }
        if (Node.getElse()) Node.getElse()->accept(*this);
    }
    virtual void visit(ForStmt &Node) override {
        if (Node.getInit()) Node.getInit()->accept(*this);
        if (Node.getCond()) {
            Node.getCond()->accept(*this);
            if (CurrentExprType != "bool") {
                errorOnNode(Node.getCond(), "Condition in for must be boolean, got " + CurrentExprType);
            }
        }
        if (Node.getBody()) Node.getBody()->accept(*this);
        if (Node.getStep()) {
            // step is a statement; allow it but check inside
            Node.getStep()->accept(*this);
        }
    }

    virtual void visit(ForEachStmt &Node) override {
        // 1) بررسی اینکه collection به صورت شناسه باشد و موجود است
        llvm::StringRef ColName = Node.getCollection();
        if (!ColName.empty() && SymbolTable.find(ColName) == SymbolTable.end()) {
            // اگر collection شناسه نبود این خط اجرا نمی‌شود (در حال حاضر collection همیشه شناسه است چون پارسر آن را به عنوان ident می‌گرفت)
            // اما برای ایمنی:
            errorOnNode(&Node, "Collection '" + ColName + "' is not defined.");
            return;
        }

        // 2) تایپ collection را چک کن
        llvm::StringRef ColType = SymbolTable.lookup(ColName);
        if (ColType != "array") {
            errorOnNode(&Node, "foreach expects an array collection, but '" + ColName + "' is of type " + ColType);
            return;
        }

        // 3) iterator را در جدول سمبل‌ها وارد کن (نوع عنصر آرایه فعلاً int)
        llvm::StringRef IterName = Node.getIterator();
        bool hadOld = false;
        llvm::StringRef OldType;
        if (SymbolTable.count(IterName)) {
            hadOld = true;
            OldType = SymbolTable[IterName];
        }
        SymbolTable[IterName] = "int"; // فرض: آرایه از اعداد int تشکیل شده

        // 4) چک کردن بدنه
        if (Node.getBody()) Node.getBody()->accept(*this);

        // 5) بازگرداندن وضعیت قبلی iterator
        if (hadOld) SymbolTable[IterName] = OldType;
        else SymbolTable.erase(IterName);
    }

    virtual void visit(PrintStmt &Node) override { if (Node.getArg()) Node.getArg()->accept(*this); }
    virtual void visit(ArrayLiteral &Node) override { CurrentExprType = "array"; }
    virtual void visit(BuiltinCall &Node) override {
        // برخی builtinها نوع مشخصی برمی‌گردانند؛ برای سادگی فعلی، بیشتر آن‌ها int برمی‌گردانند
        llvm::StringRef Name(Node.getName());
        if (Name == "to_float") CurrentExprType = "float";
        else if (Name == "to_bool") CurrentExprType = "bool";
        else if (Name == "length") CurrentExprType = "int";
        else CurrentExprType = "int";
    }
};
}

bool Sema::semantic(AST *Tree, llvm::StringRef InputCode) {
    if (!Tree) return false;
    InputCheck Checker(InputCode);
    Tree->accept(Checker);
    return Checker.hasError();
}
