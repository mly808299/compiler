#include "Sema.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class InputCheck : public ASTVisitor {
    llvm::StringMap<llvm::StringRef> SymbolTable;
    bool HasError;
    llvm::StringRef CurrentExprType;

    // تغییر: اضافه کردن شماره خط به پیام خطا
    void error(AST &Node, const llvm::Twine &Msg) {
        llvm::errs() << "Error at line " << Node.getLine() << ": " << Msg << "\n";
        HasError = true;
    }

    // خطای کلی بدون نود
    void error(const llvm::Twine &Msg) {
        llvm::errs() << "Error: " << Msg << "\n";
        HasError = true;
    }

public:
    InputCheck() : HasError(false), CurrentExprType("void") {}

    bool hasError() const { return HasError; }

    virtual void visit(AST &Node) override {}

    virtual void visit(Block &Node) override {
        for (auto *Stmt : Node.getStatements()) {
            if (Stmt) Stmt->accept(*this);
        }
    }

    virtual void visit(Declaration &Node) override {
        llvm::StringRef Name = Node.getName();
        llvm::StringRef Type = Node.getType();

        if (SymbolTable.count(Name)) {
            error(Node, "Variable '" + Name + "' is already declared.");
            return;
        }

        if (Node.getInit()) {
            Node.getInit()->accept(*this);
            if (CurrentExprType != "unknown" && CurrentExprType != Type) {
                // استثنا برای آرایه، چون نوع عناصر آرایه فعلا دقیق چک نمیشود
                if (Type != "array" && !(Type == "float" && CurrentExprType == "int")) {
                     error(Node, "Type mismatch in declaration of '" + Name + "'. Expected " + Type + ", got " + CurrentExprType);
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
                    error(Node, "Type mismatch in assignment to '" + Name + "'. Expected " + ExpectedType + ", got " + CurrentExprType);
                }
            }
        }
    }

    // --- توابع جدید برای AST های اضافه شده ---

    virtual void visit(CompoundStmt &Node) override {
        llvm::StringRef Name = Node.getName();
        if (SymbolTable.find(Name) == SymbolTable.end()) {
            error(Node, "Variable '" + Name + "' is used but not declared.");
            return;
        }
        // چک کردن نوع داده برای PLE/MIE (باید عددی باشد)
        llvm::StringRef Type = SymbolTable[Name];
        if (Type != "int" && Type != "float") {
             error(Node, "Compound assignment (+= / -=) only works on int or float.");
        }

        Node.getValue()->accept(*this);
        if (CurrentExprType != "int" && CurrentExprType != "float") {
             error(Node, "Operand of compound assignment must be numeric.");
        }
    }

    virtual void visit(UnaryStmt &Node) override {
        llvm::StringRef Name = Node.getName();
        if (SymbolTable.find(Name) == SymbolTable.end()) {
            error(Node, "Variable '" + Name + "' is used but not declared.");
            return;
        }
        llvm::StringRef Type = SymbolTable[Name];
        if (Type != "int" && Type != "float") {
             error(Node, "Increment/Decrement only works on int or float.");
        }
    }

    virtual void visit(MatchStmt &Node) override {
        Node.getTarget()->accept(*this);
        // نوع متغیر هدف
        llvm::StringRef TargetType = CurrentExprType;

        for (auto &Case : Node.getCases()) {
            // بررسی پترن
            Case.first->accept(*this);
            // اگر پترن _ نبود، باید تایپش با متغیر هدف یکی باشد
            if (CurrentExprType != "underscore" && CurrentExprType != "unknown") {
                if (CurrentExprType != TargetType) {
                   // error(Node, "Match pattern type mismatch."); // فعلا سختگیری نمیکنیم
                }
            }
            // بررسی بدنه کیس
            Case.second->accept(*this);
        }
    }

    virtual void visit(RangeExpr &Node) override {
        // 1. بررسی لیست منبع
        if (SymbolTable.find(Node.getList()) == SymbolTable.end()) {
            error(Node, "List '" + Node.getList() + "' not declared.");
        }

        // 2. اضافه کردن متغیر موقت iterator به جدول نمادها
        llvm::StringRef IterName = Node.getIterator();
        SymbolTable[IterName] = "int"; // فرض میکنیم آرایه ها int هستند فعلا

        // 3. بررسی شرط (اگر وجود دارد)
        if (Node.getCondition()) {
            Node.getCondition()->accept(*this);
            if (CurrentExprType != "bool") {
                error(Node, "Condition in list comprehension must evaluate to boolean.");
            }
        }

        // 4. بررسی عبارت خروجی
        Node.getTargetExpr()->accept(*this);

        // 5. حذف متغیر موقت (چون فقط داخل براکت معتبر است)
        SymbolTable.erase(IterName);

        CurrentExprType = "array";
    }

    // -------------------------------------------

    virtual void visit(BinaryOp &Node) override {
        Node.getLeft()->accept(*this);
        llvm::StringRef LeftType = CurrentExprType;

        Node.getRight()->accept(*this);
        llvm::StringRef RightType = CurrentExprType;

        BinaryOp::Operator Op = Node.getOperator();

        // تقسیم بر صفر
        if (Op == BinaryOp::Div || Op == BinaryOp::Mod) {
            if (auto *Num = dynamic_cast<Final*>(Node.getRight())) {
                if (Num->getKind() == Final::Number && Num->getValue() == "0") {
                    error(Node, "Division by zero detected.");
                }
            }
        }

        // عملگرهای منطقی فقط برای bool
        if (Op == BinaryOp::And || Op == BinaryOp::Or) {
            if (LeftType != "bool" || RightType != "bool") {
                error(Node, "Logical operations (AND/OR) require boolean operands.");
            }
            CurrentExprType = "bool";
            return;
        }

        // مقایسه ها
        if (Op >= BinaryOp::Eq && Op <= BinaryOp::Gte) {
            CurrentExprType = "bool";
            return;
        }

        // عملیات ریاضی
        if (LeftType != RightType) {
             if ((LeftType == "int" && RightType == "float") || (LeftType == "float" && RightType == "int")) {
                 CurrentExprType = "float";
             } else {
                 error(Node, "Type mismatch in arithmetic operation: " + LeftType + " vs " + RightType);
                 CurrentExprType = "unknown";
             }
        } else {
            CurrentExprType = LeftType;
        }
    }

    virtual void visit(Final &Node) override {
        switch (Node.getKind()) {
            case Final::Number: CurrentExprType = "int"; break;
            case Final::Float:  CurrentExprType = "float"; break;
            case Final::Bool:   CurrentExprType = "bool"; break;
            case Final::String: CurrentExprType = "string"; break;
            case Final::Underscore: CurrentExprType = "underscore"; break;
            case Final::Ident: {
                llvm::StringRef Name = Node.getValue();
                if (SymbolTable.find(Name) == SymbolTable.end()) {
                    error(Node, "Undefined variable '" + Name + "'");
                    CurrentExprType = "unknown";
                } else {
                    CurrentExprType = SymbolTable[Name];
                }
                break;
            }
        }
    }

    virtual void visit(IfStmt &Node) override {
        Node.getCond()->accept(*this);
        if (CurrentExprType != "bool") error(Node, "If condition must evaluate to boolean.");

        Node.getThen()->accept(*this);

        if (Node.getElse()) Node.getElse()->accept(*this);

        for (auto &Elif : Node.getElifs()) {
            Elif.first->accept(*this); // شرط elif
            if (CurrentExprType != "bool") error(Node, "Elif condition must evaluate to boolean.");
            Elif.second->accept(*this); // بدنه elif
        }
    }

    virtual void visit(ForStmt &Node) override {
        // اسکوپ جدید برای حلقه (ساده سازی شده)
        if (Node.getInit()) Node.getInit()->accept(*this);
        if (Node.getCond()) {
            Node.getCond()->accept(*this);
            if (CurrentExprType != "bool") error(Node, "Loop condition must be boolean.");
        }
        if (Node.getStep()) Node.getStep()->accept(*this);
        Node.getBody()->accept(*this);
    }

    virtual void visit(ForEachStmt &Node) override {
        if (SymbolTable.find(Node.getCollection()) == SymbolTable.end()) {
            error(Node, "Collection '" + Node.getCollection() + "' not declared.");
        }

        // اضافه کردن متغیر حلقه به جدول
        SymbolTable[Node.getIterator()] = "int";

        // پردازش بدنه حلقه
        Node.getBody()->accept(*this);

        // --- تغییر مهم: پاک کردن متغیر بعد از خروج از حلقه (Scope) ---
        SymbolTable.erase(Node.getIterator());
    }

    virtual void visit(PrintStmt &Node) override {
        Node.getArg()->accept(*this);
    }

    virtual void visit(ArrayLiteral &Node) override {
        for (auto *E : Node.getValues()) E->accept(*this);
        CurrentExprType = "array";
    }

    virtual void visit(BuiltinCall &Node) override {
        for(auto *Arg : Node.getArgs()) Arg->accept(*this);

        if (Node.getName() == "to_float") CurrentExprType = "float";
        else if (Node.getName() == "to_bool") CurrentExprType = "bool";
        else if (Node.getName() == "to_int") CurrentExprType = "int";
        else CurrentExprType = "int"; // length, max, index, find
    }
};
}

bool Sema::semantic(AST *Tree) {
    if (!Tree) return false;
    InputCheck Checker;
    Tree->accept(Checker);
    return Checker.hasError();
}