#include "Sema.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class InputCheck : public ASTVisitor {
    llvm::StringMap<llvm::StringRef> SymbolTable;
    bool HasError;
    llvm::StringRef CurrentExprType;

    void error(const llvm::Twine &Msg) {
        llvm::errs() << "Semantic Error: " << Msg << "\n";
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
            error("Variable '" + Name + "' is already declared.");
            return;
        }

        if (Node.getInit()) {
            Node.getInit()->accept(*this);
            if (CurrentExprType != "unknown" && CurrentExprType != Type) {
                if (!(Type == "float" && CurrentExprType == "int") && Type != "array") {
                     error("Type mismatch in declaration of '" + Name + "'. Expected " + Type + ", got " + CurrentExprType);
                }
            }
        }
        SymbolTable[Name] = Type;
    }

    virtual void visit(Assignment &Node) override {
        llvm::StringRef Name = Node.getName();
        if (SymbolTable.find(Name) == SymbolTable.end()) {
            error("Variable '" + Name + "' is used but not declared.");
            return;
        }
        llvm::StringRef ExpectedType = SymbolTable[Name];

        if (Node.getValue()) {
            Node.getValue()->accept(*this);
            if (CurrentExprType != "unknown" && CurrentExprType != ExpectedType) {
                if (!(ExpectedType == "float" && CurrentExprType == "int")) {
                    error("Type mismatch in assignment to '" + Name + "'. Expected " + ExpectedType + ", got " + CurrentExprType);
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
                    error("Division by zero detected.");
                }
            }
        }

        if (Op == BinaryOp::And || Op == BinaryOp::Or) {
            if (LeftType != "bool" || RightType != "bool") {
                error("Logical operations (AND/OR) require boolean operands.");
            }
            CurrentExprType = "bool";
            return;
        }

        if (Op >= BinaryOp::Eq && Op <= BinaryOp::Gte) {
            CurrentExprType = "bool";
            return;
        }

        if (LeftType != RightType) {
             if ((LeftType == "int" && RightType == "float") || (LeftType == "float" && RightType == "int")) {
                 CurrentExprType = "float";
             } else {
                 error("Type mismatch in arithmetic operation: " + LeftType + " vs " + RightType);
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
            case Final::Ident: {
                llvm::StringRef Name = Node.getValue();
                if (SymbolTable.find(Name) == SymbolTable.end()) {
                    error("Undefined variable '" + Name + "'");
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
        if (CurrentExprType != "bool") error("If condition must evaluate to boolean.");
        Node.getThen()->accept(*this);
        if (Node.getElse()) Node.getElse()->accept(*this);
        for (auto &Elif : Node.getElifs()) {
            Elif.first->accept(*this);
            Elif.second->accept(*this);
        }
    }

    virtual void visit(ForStmt &Node) override {
        if (Node.getInit()) Node.getInit()->accept(*this);
        if (Node.getCond()) {
            Node.getCond()->accept(*this);
            if (CurrentExprType != "bool") error("Loop condition must be boolean.");
        }
        if (Node.getStep()) Node.getStep()->accept(*this);
        Node.getBody()->accept(*this);
    }

    virtual void visit(ForEachStmt &Node) override {
        if (SymbolTable.find(Node.getCollection()) == SymbolTable.end()) {
            error("Collection '" + Node.getCollection() + "' not declared.");
        }
        SymbolTable[Node.getIterator()] = "int"; // Generic type for now
        Node.getBody()->accept(*this);
    }

    virtual void visit(PrintStmt &Node) override {
        Node.getArg()->accept(*this);
    }

    // --- FIX: Implemented missing methods ---
    virtual void visit(ArrayLiteral &Node) override {
        for (auto *E : Node.getValues()) E->accept(*this);
        CurrentExprType = "array";
    }

    virtual void visit(BuiltinCall &Node) override {
        for(auto *Arg : Node.getArgs()) Arg->accept(*this);
        // Simple return type inference
        if (Node.getName() == "to_float") CurrentExprType = "float";
        else if (Node.getName() == "to_bool") CurrentExprType = "bool";
        else CurrentExprType = "int"; // length, max, index, find, to_int
    }
};
}

bool Sema::semantic(AST *Tree) {
    if (!Tree) return false;
    InputCheck Checker;
    Tree->accept(Checker);
    return Checker.hasError();
}