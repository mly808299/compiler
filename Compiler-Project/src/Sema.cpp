#include "Sema.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringExtras.h"
#include <string>

namespace {

    class InputCheck : public ASTVisitor {
        llvm::StringMap<llvm::StringRef> SymbolTable;
        llvm::StringMap<std::string> ValueTable;

        bool HasError;
        llvm::StringRef CurrentExprType;
        std::string CurrentExprValue;

        llvm::StringRef SourceCode;

        bool InRestrictedContext = false;
        bool InControlFlow = false;

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

        void errorOnNode(AST *Node, const llvm::Twine &Msg) {
            if (Node) error(*Node, Msg);
        }

    public:
        InputCheck(llvm::StringRef Code) : HasError(false), CurrentExprType("void"), SourceCode(Code) {}

        bool hasError() const { return HasError; }

        virtual void visit(AST &Node) override {}

        virtual void visit(Block &Node) override {
            for (auto *Stmt: Node.getStatements()) if (Stmt) Stmt->accept(*this);
        }

        virtual void visit(Declaration &Node) override {
            if (InControlFlow) {
                error(Node, "Semantic Error: Variable declaration is not allowed inside control flow bodies (if, loop).");
            }

            llvm::StringRef Name = Node.getName();
            llvm::StringRef Type = Node.getType();

            if (SymbolTable.count(Name)) {
                error(Node, "Semantic Error: Variable '" + Name + "' is already declared.");
                return;
            }

            if (Node.getInit()) {
                Node.getInit()->accept(*this);
                if (CurrentExprType != "unknown" && CurrentExprType != Type) {
                    if (!(Type == "float" && CurrentExprType == "int")) {
                        error(Node, "Type Error: Type mismatch in declaration of '" + Name + "'. Expected " + Type + ", got " + CurrentExprType);
                    }
                }
                if (!CurrentExprValue.empty()) ValueTable[Name] = CurrentExprValue;
            }
            SymbolTable[Name] = Type;
            CurrentExprValue = "";
        }

        virtual void visit(Assignment &Node) override {
            llvm::StringRef Name = Node.getName();
            if (SymbolTable.find(Name) == SymbolTable.end()) {
                error(Node, "Semantic Error: Variable '" + Name + "' is used but not declared.");
                return;
            }
            llvm::StringRef ExpectedType = SymbolTable[Name];

            if (Node.getIndex()) {
                if (ExpectedType != "array") {
                    error(Node, "Semantic Error: Variable '" + Name + "' is not an array.");
                    return;
                }
                Node.getIndex()->accept(*this);
                if (CurrentExprType != "int") errorOnNode(Node.getIndex(), "Type Error: Array index must be an integer.");

                if (Node.getValue()) {
                    Node.getValue()->accept(*this);
                    if (CurrentExprType != "int" && CurrentExprType != "float") errorOnNode(Node.getValue(), "Type Error: Array elements must be numeric.");
                }
                return;
            }

            if (Node.getValue()) {
                Node.getValue()->accept(*this);
                if (CurrentExprType != "unknown" && CurrentExprType != ExpectedType) {
                    if (!(ExpectedType == "float" && CurrentExprType == "int")) {
                        if (CurrentExprType == "bool") error(Node, "Type Error: Type mismatch in assignment to '" + Name + "'. Expected bool, got " + ExpectedType);
                        else error(Node, "Type Error: Type mismatch in assignment to '" + Name + "'. Expected " + ExpectedType + ", got " + CurrentExprType);
                    }
                }
                if (!CurrentExprValue.empty()) ValueTable[Name] = CurrentExprValue;
            }
            CurrentExprValue = "";
        }

        virtual void visit(CompoundStmt &Node) override {
            llvm::StringRef Name = Node.getName();
            if (SymbolTable.find(Name) == SymbolTable.end()) { error(Node, "Semantic Error: Variable '" + Name + "' is used but not declared."); return; }
            llvm::StringRef Type = SymbolTable[Name];

            if (Node.getIndex()) {
                if (Type != "array") { error(Node, "Semantic Error: Variable '" + Name + "' is not an array."); return; }
                Node.getIndex()->accept(*this);
            } else {
                if (Type == "bool") { error(Node, "Semantic Error: Invalid compound assignment: " + Name + " is of type bool."); return; }
                if (Type != "int" && Type != "float") { error(Node, "Semantic Error: Compound assignment only works on int or float."); return; }
            }

            Node.getValue()->accept(*this);
            if (CurrentExprType != "int" && CurrentExprType != "float") errorOnNode(Node.getValue(), "Type Error: Operand must be numeric.");
            CurrentExprValue = "";
        }

        virtual void visit(UnaryStmt &Node) override {
            llvm::StringRef Name = Node.getName();
            if (SymbolTable.find(Name) == SymbolTable.end()) { error(Node, "Semantic Error: Variable '" + Name + "' is used but not declared."); return; }
            llvm::StringRef Type = SymbolTable[Name];

            if (Node.getIndex()) {
                if (Type != "array") { error(Node, "Semantic Error: Variable '" + Name + "' is not an array and cannot be indexed."); return; }
                Node.getIndex()->accept(*this);
                return;
            }
            if (Type == "bool") { error(Node, "Semantic Error: Invalid unary operator: " + Name + " is of type bool."); return; }
            if (Type != "int" && Type != "float") { error(Node, "Semantic Error: Increment/Decrement only works on int or float variables."); return; }
            CurrentExprValue = "";
        }

        virtual void visit(ArrayAccess &Node) override {
            if (SymbolTable.find(Node.getName()) == SymbolTable.end()) error(Node, "Semantic Error: Variable '" + Node.getName() + "' is used but not declared.");
            Node.getIndex()->accept(*this);
            CurrentExprType = "int";
            CurrentExprValue = "";
        }

        virtual void visit(MatchStmt &Node) override {
            Node.getTarget()->accept(*this);
            bool PrevState = InControlFlow;
            InControlFlow = true;
            for (auto &Case: Node.getCases()) {
                Case.first->accept(*this);
                Case.second->accept(*this);
            }
            InControlFlow = PrevState;
            CurrentExprValue = "";
        }

        virtual void visit(RangeExpr &Node) override {
            if (SymbolTable.find(Node.getList()) == SymbolTable.end()) error(Node, "Semantic Error: List '" + Node.getList() + "' not declared.");
            SymbolTable[Node.getIterator()] = "int";

            bool PrevState = InRestrictedContext;
            InRestrictedContext = true;
            if (Node.getCondition()) Node.getCondition()->accept(*this);
            InRestrictedContext = PrevState;

            Node.getTargetExpr()->accept(*this);
            SymbolTable.erase(Node.getIterator());
            CurrentExprType = "array";
            CurrentExprValue = "";
        }

        virtual void visit(BinaryOp &Node) override {
            BinaryOp::Operator Op = Node.getOperator();

            if ((Op == BinaryOp::And || Op == BinaryOp::Or) && InRestrictedContext) {
                error(Node, "Semantic Error: Restricted Usage: AND/OR operators are NOT allowed in this context.");
                CurrentExprType = "unknown";
                return;
            }

            Node.getLeft()->accept(*this);
            std::string LeftType = CurrentExprType.str();

            Node.getRight()->accept(*this);
            std::string RightType = CurrentExprType.str();
            std::string RightVal = CurrentExprValue;

            if (Op == BinaryOp::And || Op == BinaryOp::Or) {
                if (LeftType != "bool" || RightType != "bool") {
                    error(Node, "Type Error: Logical operators AND/OR require 'bool' operands. Got '" + LeftType + "' and '" + RightType + "'.");
                    CurrentExprType = "unknown";
                    return;
                }
                CurrentExprType = "bool";
            }
            else if (Op == BinaryOp::Eq || Op == BinaryOp::Neq || Op == BinaryOp::Lt ||
                     Op == BinaryOp::Gt || Op == BinaryOp::Lte || Op == BinaryOp::Gte) {
                bool isNumericLeft = (LeftType == "int" || LeftType == "float");
                bool isNumericRight = (RightType == "int" || RightType == "float");
                if (LeftType != RightType) {
                    if (!(isNumericLeft && isNumericRight)) {
                        error(Node, "Type Error: Comparison between incompatible types '" + LeftType + "' and '" + RightType + "'.");
                        CurrentExprType = "unknown";
                        return;
                    }
                }
                CurrentExprType = "bool";
            }
            else {
                if (LeftType == "bool" || RightType == "bool") {
                    error(Node, "Type Error: Arithmetic operators do not work on 'bool'.");
                    CurrentExprType = "unknown";
                    return;
                }
                if (LeftType == "array" || RightType == "array") {
                    error(Node, "Type Error: Arithmetic operators do not work on 'array'.");
                    CurrentExprType = "unknown";
                    return;
                }
                if (LeftType == "float" || RightType == "float") CurrentExprType = "float";
                else CurrentExprType = "int";
            }

            if (Op == BinaryOp::Div || Op == BinaryOp::Mod) {
                bool isZero = false;
                if (RightVal == "0" || RightVal == "0.0") isZero = true;
                if (isZero) {
                    std::string varName = "";
                    if (auto *F = dynamic_cast<Final*>(Node.getRight())) {
                        if (F->getKind() == Final::Ident) varName = F->getValue().str();
                    }
                    if (!varName.empty()) error(Node, "Semantic Error: Division by zero (denominator '" + varName + "' is 0)");
                    else error(Node, "Semantic Error: Division by zero");
                }
            }
            CurrentExprValue = "";
        }

        virtual void visit(Final &Node) override {
            if (Node.getKind() == Final::Ident) {
                if (SymbolTable.find(Node.getValue()) == SymbolTable.end()) {
                    // اگر متغیر پیدا نشد، چک کنیم شاید 'x' در find باشد که هنوز اضافه نشده
                    // (البته این حالت نباید پیش بیاید چون قبل از ویزیت کردن شرط find، متغیر x را اضافه می‌کنیم)
                    error(Node, "Semantic Error: Undefined variable '" + Node.getValue() + "'");
                    CurrentExprType = "unknown";
                    CurrentExprValue = "";
                } else {
                    CurrentExprType = SymbolTable[Node.getValue()];
                    if (ValueTable.count(Node.getValue())) CurrentExprValue = ValueTable[Node.getValue()];
                    else CurrentExprValue = "";
                }
            } else {
                CurrentExprValue = Node.getValue().str();
                switch (Node.getKind()) {
                    case Final::Number: CurrentExprType = "int"; break;
                    case Final::Float:  CurrentExprType = "float"; break;
                    case Final::Bool:   CurrentExprType = "bool"; break;
                    default: CurrentExprType = "string"; break;
                }
            }
        }

        virtual void visit(IfStmt &Node) override {
            if (Node.getCond()) {
                Node.getCond()->accept(*this);
                if (CurrentExprType != "bool" && CurrentExprType != "int" && CurrentExprType != "unknown") {
                    errorOnNode(Node.getCond(), "Type Error: Condition must evaluate to bool or int.");
                }
            }
            bool PrevState = InControlFlow;
            InControlFlow = true;
            if (Node.getThen()) Node.getThen()->accept(*this);
            if (Node.getElse()) Node.getElse()->accept(*this);
            for (auto &Elif: Node.getElifs()) {
                InControlFlow = PrevState;
                if (Elif.first) Elif.first->accept(*this);
                InControlFlow = true;
                if (Elif.second) Elif.second->accept(*this);
            }
            InControlFlow = PrevState;
            CurrentExprValue = "";
        }

        virtual void visit(ForStmt &Node) override {
            if (Node.getInit()) Node.getInit()->accept(*this);
            if (Node.getCond()) {
                Node.getCond()->accept(*this);
                if (CurrentExprType != "bool" && CurrentExprType != "int" && CurrentExprType != "unknown") {
                    errorOnNode(Node.getCond(), "Type Error: Loop condition must be bool or int.");
                }
            }
            if (Node.getStep()) Node.getStep()->accept(*this);
            bool PrevState = InControlFlow;
            InControlFlow = true;
            if (Node.getBody()) Node.getBody()->accept(*this);
            InControlFlow = PrevState;
            CurrentExprValue = "";
        }

        virtual void visit(ForEachStmt &Node) override {
            if (SymbolTable.find(Node.getCollection()) == SymbolTable.end()) {
                error(Node, "Semantic Error: Collection '" + Node.getCollection() + "' not declared.");
            }
            SymbolTable[Node.getIterator()] = "int";
            bool PrevState = InControlFlow;
            InControlFlow = true;
            if (Node.getBody()) Node.getBody()->accept(*this);
            InControlFlow = PrevState;
            SymbolTable.erase(Node.getIterator());
            CurrentExprValue = "";
        }

        virtual void visit(PrintStmt &Node) override {
            Node.getArg()->accept(*this);
            CurrentExprValue = "";
        }

        virtual void visit(ArrayLiteral &Node) override {
            for (auto *E: Node.getValues()) E->accept(*this);
            CurrentExprType = "array";
            CurrentExprValue = "";
        }

        virtual void visit(BuiltinCall &Node) override {
            // >>> مدیریت ویژه برای find <<<
            // >>> مدیریت ویژه برای find <<<
            if (Node.getName() == "find") {
                if (Node.getArgs().size() != 2) {
                    error(Node, "Semantic Error: 'find' requires 2 arguments.");
                    return;
                }

                Node.getArgs()[0]->accept(*this);
                if (CurrentExprType != "array") {
                    errorOnNode(Node.getArgs()[0], "Semantic Error: First argument of 'find' must be an array.");
                }

                // تابع کمکی ۱: بررسی وجود x
                std::function<bool(AST*)> containsX = [&](AST *N) -> bool {
                    if (!N) return false;
                    if (auto *B = dynamic_cast<BinaryOp*>(N))
                        return containsX(B->getLeft()) || containsX(B->getRight());
                    if (auto *F = dynamic_cast<Final*>(N))
                        return (F->getKind() == Final::Ident && F->getValue() == "x");
                    return false;
                };

                // تابع کمکی ۲: پیدا کردن متغیر مزاحم (مثلا x2) برای اشاره دقیق خطا
                std::function<AST*(AST*)> findBadIdent = [&](AST *N) -> AST* {
                    if (!N) return nullptr;
                    if (auto *B = dynamic_cast<BinaryOp*>(N)) {
                        AST* LeftBad = findBadIdent(B->getLeft());
                        if (LeftBad) return LeftBad;
                        return findBadIdent(B->getRight());
                    }
                    // اگر متغیری دیدیم که x نیست، همان را برگردان
                    if (auto *F = dynamic_cast<Final*>(N)) {
                        if (F->getKind() == Final::Ident && F->getValue() != "x") return N;
                    }
                    return nullptr;
                };

                // اگر x در شرط نبود
                if (!containsX(Node.getArgs()[1])) {
                    // بگرد ببین چه چیزی به جای x نوشته شده
                    AST* BadNode = findBadIdent(Node.getArgs()[1]);

                    if (BadNode) {
                        // اگر متغیر غلط (مثل x2) پیدا شد، خطا را دقیقا روی آن بده
                        errorOnNode(BadNode, "Semantic Error: The condition in 'find' MUST use the implicit variable 'x'. Did you mean 'x'?");
                    } else {
                        // اگر کلا متغیری نبود (مثلا 5 > 2)، خطا را روی کل شرط بده
                        errorOnNode(Node.getArgs()[1], "Semantic Error: The condition in 'find' MUST use the implicit variable 'x'.");
                    }

                    // [نکته مهم] تایپ را int تنظیم می‌کنیم تا خطای دوم (Type mismatch) حذف شود
                    CurrentExprType = "int";
                    CurrentExprValue = "";
                    return;
                }
                // -----------------------------------------------------------

                // 1. اضافه کردن موقت x به جدول نمادها
                bool Shadowing = SymbolTable.count("x");
                llvm::StringRef OldType = Shadowing ? SymbolTable["x"] : "";

                SymbolTable["x"] = "int";

                // 2. بررسی شرط
                Node.getArgs()[1]->accept(*this);
                if (CurrentExprType != "bool" && CurrentExprType != "int" && CurrentExprType != "unknown") {
                    errorOnNode(Node.getArgs()[1], "Type Error: Condition in 'find' must evaluate to bool or int.");
                }

                // 3. پاک کردن x
                if (Shadowing) SymbolTable["x"] = OldType;
                else SymbolTable.erase("x");

                CurrentExprType = "int";
                CurrentExprValue = "";
                return;
            }
            // >>> پایان مدیریت find <<<
            // >>> پایان مدیریت find <<<

            for (auto *Arg: Node.getArgs()) {
                if (Arg) Arg->accept(*this);
            }

            if (Node.getName() == "to_float") CurrentExprType = "float";
            else if (Node.getName() == "to_bool") CurrentExprType = "bool";
            else CurrentExprType = "int";
            CurrentExprValue = "";
        }
    };
}

bool Sema::semantic(AST *Tree, llvm::StringRef InputCode) {
    if (!Tree) return false;
    InputCheck Checker(InputCode);
    Tree->accept(Checker);
    return Checker.hasError();
}