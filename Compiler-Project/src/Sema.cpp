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
            error(Node, "Variable '" + Name + "' is already declared.");
            return;
        }

        if (Node.getInit()) {
            Node.getInit()->accept(*this);
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

        // حالت ۱: مقداردهی به خانه آرایه
        if (Node.getIndex()) {
            if (ExpectedType != "array") {
                error(Node, "Variable '" + Name + "' is not an array.");
                return;
            }
            Node.getIndex()->accept(*this);

            // چک کردن مقدار سمت راست
            if (Node.getValue()) {
                Node.getValue()->accept(*this);
                // فعلاً فرض می‌کنیم آرایه‌ها int هستند
                if (CurrentExprType != "int" && CurrentExprType != "float") {
                    errorOnNode(Node.getValue(), "Array elements must be numeric.");
                }
            }
            return;
        }

        // حالت ۲: مقداردهی معمولی
        if (Node.getValue()) {
            Node.getValue()->accept(*this);
            if (CurrentExprType != "unknown" && CurrentExprType != ExpectedType) {
                if (!(ExpectedType == "float" && CurrentExprType == "int")) {
                    error(Node, "Type mismatch in assignment to '" + Name + "'. Expected " + ExpectedType + ", got " + CurrentExprType);
                }
            }
        }
    }

    virtual void visit(CompoundStmt &Node) override {
        llvm::StringRef Name = Node.getName();
        if (SymbolTable.find(Name) == SymbolTable.end()) {
            error(Node, "Variable '" + Name + "' is used but not declared.");
            return;
        }
        llvm::StringRef Type = SymbolTable[Name];

        // حالت ۱: عملیات روی آرایه
        if (Node.getIndex()) {
            if (Type != "array") {
                error(Node, "Variable '" + Name + "' is not an array.");
                return;
            }
            Node.getIndex()->accept(*this);
        }
        // حالت ۲: عملیات روی متغیر معمولی
        else {
            if (Type == "bool") {
                error(Node, "Invalid compound assignment: " + Name + " is of type bool.");
                return;
            }
            if (Type != "int" && Type != "float") {
                error(Node, "Compound assignment only works on int or float.");
                return;
            }
        }

        // بررسی مقدار سمت راست
        Node.getValue()->accept(*this);
        if (CurrentExprType != "int" && CurrentExprType != "float") {
            errorOnNode(Node.getValue(), "Operand must be numeric.");
        }
    }

    virtual void visit(UnaryStmt &Node) override {
        llvm::StringRef Name = Node.getName();

        if (SymbolTable.find(Name) == SymbolTable.end()) {
            error(Node, "Variable '" + Name + "' is used but not declared.");
            return;
        }
        llvm::StringRef Type = SymbolTable[Name];

        // حالت ۱: اگر روی آرایه اعمال شده (دارای ایندکس است)
        if (Node.getIndex()) {
            if (Type != "array") {
                error(Node, "Variable '" + Name + "' is not an array and cannot be indexed.");
                return;
            }
            Node.getIndex()->accept(*this);
            return; // همه چیز درست است (فرض می‌کنیم عناصر آرایه عددی هستند)
        }

        // حالت ۲: اگر روی متغیر معمولی اعمال شده
        if (Type == "bool") {
            error(Node, "Invalid unary operator: " + Name + " is of type bool and cannot be incremented/decremented.");
            return;
        }
        if (Type != "int" && Type != "float") {
            error(Node, "Increment/Decrement only works on int or float variables.");
            return;
        }
    }
    // --- متد جدید برای دسترسی به آرایه ---
    virtual void visit(ArrayAccess &Node) override {
        if (SymbolTable.find(Node.getName()) == SymbolTable.end()) {
            error(Node, "Variable '" + Node.getName() + "' is used but not declared.");
        }
        Node.getIndex()->accept(*this);
        CurrentExprType = "int"; // فرض بر int بودن
    }

    virtual void visit(MatchStmt &Node) override {
        Node.getTarget()->accept(*this);
        for (auto &Case : Node.getCases()) {
            Case.first->accept(*this);
            Case.second->accept(*this);
        }
    }

    virtual void visit(RangeExpr &Node) override {
        if (SymbolTable.find(Node.getList()) == SymbolTable.end()) {
            error(Node, "List '" + Node.getList() + "' not declared.");
        }
        SymbolTable[Node.getIterator()] = "int";
        if (Node.getCondition()) Node.getCondition()->accept(*this);
        Node.getTargetExpr()->accept(*this);
        SymbolTable.erase(Node.getIterator());
        CurrentExprType = "array";
    }

    virtual void visit(BinaryOp &Node) override {
        Node.getLeft()->accept(*this);
        Node.getRight()->accept(*this);
    }

    virtual void visit(Final &Node) override {
        if (Node.getKind() == Final::Ident) {
            if (SymbolTable.find(Node.getValue()) == SymbolTable.end()) {
                error(Node, "Undefined variable '" + Node.getValue() + "'");
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

    virtual void visit(IfStmt &Node) override {
        Node.getCond()->accept(*this);
        Node.getThen()->accept(*this);
        if (Node.getElse()) Node.getElse()->accept(*this);
        for (auto &Elif : Node.getElifs()) {
            Elif.first->accept(*this);
            Elif.second->accept(*this);
        }
    }

    virtual void visit(ForStmt &Node) override {
        if (Node.getInit()) Node.getInit()->accept(*this);
        if (Node.getCond()) Node.getCond()->accept(*this);
        if (Node.getStep()) Node.getStep()->accept(*this);
        Node.getBody()->accept(*this);
    }

    virtual void visit(ForEachStmt &Node) override {
        if (SymbolTable.find(Node.getCollection()) == SymbolTable.end()) {
            error(Node, "Collection '" + Node.getCollection() + "' not declared.");
        }
        SymbolTable[Node.getIterator()] = "int";
        Node.getBody()->accept(*this);
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