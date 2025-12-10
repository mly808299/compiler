#include "Sema.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringExtras.h"
#include <string>

namespace {

    class InputCheck : public ASTVisitor {
        llvm::StringMap<llvm::StringRef> SymbolTable;
        bool HasError;
        llvm::StringRef CurrentExprType;
        llvm::StringRef SourceCode;

        bool InRestrictedContext = false;

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
            llvm::StringRef Name = Node.getName();
            llvm::StringRef Type = Node.getType();

            if (SymbolTable.count(Name)) {
                error(Node, "Variable '" + Name + "' is already declared.");
                return;
            }

            if (Node.getInit()) {
                Node.getInit()->accept(*this);
                if (CurrentExprType != "unknown" && CurrentExprType != Type) {
                    if (!(Type == "float" && CurrentExprType == "int")) {
                        error(Node, "Type mismatch in declaration of '" + Name + "'. Expected " + Type + ", got " +
                                    CurrentExprType);
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

            if (Node.getIndex()) {
                if (ExpectedType != "array") {
                    error(Node, "Variable '" + Name + "' is not an array.");
                    return;
                }
                Node.getIndex()->accept(*this);
                if (CurrentExprType != "int") errorOnNode(Node.getIndex(), "Array index must be an integer.");

                if (Node.getValue()) {
                    Node.getValue()->accept(*this);
                    if (CurrentExprType != "int" && CurrentExprType != "float")
                        errorOnNode(Node.getValue(), "Array elements must be numeric.");
                }
                return;
            }

            if (Node.getValue()) {
                Node.getValue()->accept(*this);
                if (CurrentExprType != "unknown" && CurrentExprType != ExpectedType) {
                    if (!(ExpectedType == "float" && CurrentExprType == "int")) {
                        if (CurrentExprType == "bool")
                            error(Node, "Type mismatch in assignment to '" + Name + "'. Expected bool, got " +
                                        ExpectedType);
                        else
                            error(Node, "Type mismatch in assignment to '" + Name + "'. Expected " + ExpectedType +
                                        ", got " + CurrentExprType);
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

            if (Node.getIndex()) {
                if (Type != "array") {
                    error(Node, "Variable '" + Name + "' is not an array.");
                    return;
                }
                Node.getIndex()->accept(*this);
            } else {
                if (Type == "bool") {
                    error(Node, "Invalid compound assignment: " + Name + " is of type bool.");
                    return;
                }
                if (Type != "int" && Type != "float") {
                    error(Node, "Compound assignment only works on int or float.");
                    return;
                }
            }

            Node.getValue()->accept(*this);
            if (CurrentExprType != "int" && CurrentExprType != "float")
                errorOnNode(Node.getValue(), "Operand must be numeric.");
        }

        virtual void visit(UnaryStmt &Node) override {
            llvm::StringRef Name = Node.getName();
            if (SymbolTable.find(Name) == SymbolTable.end()) {
                error(Node, "Variable '" + Name + "' is used but not declared.");
                return;
            }
            llvm::StringRef Type = SymbolTable[Name];

            if (Node.getIndex()) {
                if (Type != "array") {
                    error(Node, "Variable '" + Name + "' is not an array and cannot be indexed.");
                    return;
                }
                Node.getIndex()->accept(*this);
                return;
            }

            if (Type == "bool") {
                error(Node, "Invalid unary operator: " + Name + " is of type bool.");
                return;
            }
            if (Type != "int" && Type != "float") {
                error(Node, "Increment/Decrement only works on int or float variables.");
                return;
            }
        }

        virtual void visit(ArrayAccess &Node) override {
            if (SymbolTable.find(Node.getName()) == SymbolTable.end())
                error(Node, "Variable '" + Node.getName() + "' is used but not declared.");
            Node.getIndex()->accept(*this);
            CurrentExprType = "int";
        }

        virtual void visit(MatchStmt &Node) override {
            Node.getTarget()->accept(*this);
            for (auto &Case: Node.getCases()) {
                Case.first->accept(*this);
                Case.second->accept(*this);
            }
        }

        virtual void visit(RangeExpr &Node) override {
            if (SymbolTable.find(Node.getList()) == SymbolTable.end())
                error(Node, "List '" + Node.getList() + "' not declared.");
            SymbolTable[Node.getIterator()] = "int";

            bool PrevState = InRestrictedContext;
            InRestrictedContext = true;
            if (Node.getCondition()) Node.getCondition()->accept(*this);
            InRestrictedContext = PrevState;

            Node.getTargetExpr()->accept(*this);
            SymbolTable.erase(Node.getIterator());
            CurrentExprType = "array";
        }

        virtual void visit(BinaryOp &Node) override {
            BinaryOp::Operator Op = Node.getOperator();

            if ((Op == BinaryOp::And || Op == BinaryOp::Or) && InRestrictedContext) {
                error(Node, "Restricted Usage: AND/OR operators are NOT allowed in this context.");
                CurrentExprType = "unknown";
                return;
            }

            Node.getLeft()->accept(*this);
            std::string LeftType = CurrentExprType.str();

            Node.getRight()->accept(*this);
            std::string RightType = CurrentExprType.str();

            if (Op == BinaryOp::And || Op == BinaryOp::Or) {
                if (LeftType != "bool" || RightType != "bool") {
                    error(Node,
                          "Type Error: Logical operators AND/OR require 'bool' operands. Got '" + LeftType + "' and '" +
                          RightType + "'.");
                    CurrentExprType = "unknown";
                    return;
                }
                CurrentExprType = "bool";
            } else if (Op == BinaryOp::Eq || Op == BinaryOp::Neq || Op == BinaryOp::Lt ||
                       Op == BinaryOp::Gt || Op == BinaryOp::Lte || Op == BinaryOp::Gte) {
                bool isNumericLeft = (LeftType == "int" || LeftType == "float");
                bool isNumericRight = (RightType == "int" || RightType == "float");
                if (LeftType != RightType) {
                    if (!(isNumericLeft && isNumericRight)) {
                        error(Node,
                              "Type Error: Comparison between incompatible types '" + LeftType + "' and '" + RightType +
                              "'.");
                        CurrentExprType = "unknown";
                        return;
                    }
                }
                CurrentExprType = "bool";
            } else {
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
                if (auto *RightVal = dynamic_cast<Final *>(Node.getRight())) {
                    if (RightVal->getKind() == Final::Number) {
                        if (RightVal->getValue() == "0") error(Node, "Math Error: Division by zero is undefined.");
                    } else if (RightVal->getKind() == Final::Float) {
                        if (std::stod(RightVal->getValue().str()) == 0.0)
                            error(Node, "Math Error: Division by zero is undefined.");
                    }
                }
            }
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
                    case Final::Number:
                        CurrentExprType = "int";
                        break;
                    case Final::Float:
                        CurrentExprType = "float";
                        break;
                    case Final::Bool:
                        CurrentExprType = "bool";
                        break;
                    default:
                        CurrentExprType = "string";
                        break;
                }
            }
        }

        // --- حذف محدودیت در IfStmt (TC-C01 OK) ---
        virtual void visit(IfStmt &Node) override {
            if (Node.getCond()) {
                Node.getCond()->accept(*this);
                if (CurrentExprType != "bool" && CurrentExprType != "int" && CurrentExprType != "unknown") {
                    errorOnNode(Node.getCond(), "Condition must evaluate to bool or int.");
                }
            }

            if (Node.getThen()) Node.getThen()->accept(*this);
            if (Node.getElse()) Node.getElse()->accept(*this);

            for (auto &Elif: Node.getElifs()) {
                if (Elif.first) Elif.first->accept(*this);
                if (Elif.second) Elif.second->accept(*this);
            }
        }

        // --- حذف محدودیت در ForStmt (TC-C01 OK) ---
        virtual void visit(ForStmt &Node) override {
            if (Node.getInit()) Node.getInit()->accept(*this);

            if (Node.getCond()) {
                Node.getCond()->accept(*this);
                if (CurrentExprType != "bool" && CurrentExprType != "int" && CurrentExprType != "unknown") {
                    errorOnNode(Node.getCond(), "Loop condition must be bool or int.");
                }
            }

            if (Node.getStep()) Node.getStep()->accept(*this);
            if (Node.getBody()) Node.getBody()->accept(*this);
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
            for (auto *E: Node.getValues()) E->accept(*this);
            CurrentExprType = "array";
        }

        virtual void visit(BuiltinCall &Node) override {
            for (auto *Arg: Node.getArgs()) {
                if (Arg) Arg->accept(*this);
            }

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