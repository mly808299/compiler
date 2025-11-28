#ifndef AST_H
#define AST_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include <string>
#include <vector>

// Forward declarations
class AST;
class Expr;
class Stmt;
class Block;
class Declaration;
class Assignment;
class BinaryOp;
class Final;
class IfStmt;
class ForStmt;
class ForEachStmt;
class PrintStmt;
class ArrayLiteral;
class BuiltinCall;

// Visitor Pattern
class ASTVisitor
{
public:
    virtual void visit(AST &) {}
    virtual void visit(Block &) = 0;
    virtual void visit(Declaration &) = 0;
    virtual void visit(Assignment &) = 0;
    virtual void visit(BinaryOp &) = 0;
    virtual void visit(Final &) = 0;
    virtual void visit(IfStmt &) = 0;
    virtual void visit(ForStmt &) = 0;
    virtual void visit(ForEachStmt &) = 0;
    virtual void visit(PrintStmt &) = 0;
    virtual void visit(ArrayLiteral &) = 0;
    virtual void visit(BuiltinCall &) = 0;
};

class AST
{
public:
    virtual ~AST() {}
    virtual void accept(ASTVisitor &V) = 0;
};

class Block : public AST
{
    llvm::SmallVector<AST *, 8> Statements;
public:
    Block() {}
    void addStatement(AST *S) { Statements.push_back(S); }
    const llvm::SmallVector<AST *, 8> &getStatements() const { return Statements; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class Expr : public AST {};

class Final : public Expr
{
public:
    // FIX: Added Float here
    enum ValueKind { Ident, Number, Float, Bool };
private:
    ValueKind Kind;
    llvm::StringRef Val;
public:
    Final(ValueKind Kind, llvm::StringRef Val) : Kind(Kind), Val(Val) {}
    ValueKind getKind() const { return Kind; }
    llvm::StringRef getValue() const { return Val; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class BinaryOp : public Expr
{
public:
    enum Operator {
        Plus, Minus, Mul, Div, Mod,
        And, Or,
        Eq, Neq, Lt, Gt, Lte, Gte
    };
private:
    Expr *Left;
    Expr *Right;
    Operator Op;
public:
    BinaryOp(Operator Op, Expr *L, Expr *R) : Op(Op), Left(L), Right(R) {}
    Expr *getLeft() { return Left; }
    Expr *getRight() { return Right; }
    Operator getOperator() { return Op; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class Declaration : public AST
{
    llvm::StringRef VarName;
    llvm::StringRef Type;
    Expr *Init;
public:
    Declaration(llvm::StringRef Name, llvm::StringRef Type, Expr *Init = nullptr)
        : VarName(Name), Type(Type), Init(Init) {}
    llvm::StringRef getName() const { return VarName; }
    llvm::StringRef getType() const { return Type; }
    Expr *getInit() { return Init; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class Assignment : public AST
{
    llvm::StringRef VarName;
    Expr *Value;
public:
    Assignment(llvm::StringRef Name, Expr *Val) : VarName(Name), Value(Val) {}
    llvm::StringRef getName() const { return VarName; }
    Expr *getValue() { return Value; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class IfStmt : public AST
{
    Expr *Cond;
    Block *ThenBlock;
    llvm::SmallVector<std::pair<Expr*, Block*>, 4> Elifs;
    Block *ElseBlock;
public:
    IfStmt(Expr *Cond, Block *Then, const llvm::SmallVector<std::pair<Expr*, Block*>, 4> &Elifs, Block *Else)
        : Cond(Cond), ThenBlock(Then), Elifs(Elifs), ElseBlock(Else) {}
    Expr *getCond() { return Cond; }
    Block *getThen() { return ThenBlock; }
    const auto &getElifs() { return Elifs; }
    Block *getElse() { return ElseBlock; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class ForStmt : public AST
{
    AST *Init;
    Expr *Cond;
    AST *Step;
    Block *Body;
public:
    ForStmt(AST *Init, Expr *Cond, AST *Step, Block *Body)
        : Init(Init), Cond(Cond), Step(Step), Body(Body) {}
    AST *getInit() { return Init; }
    Expr *getCond() { return Cond; }
    AST *getStep() { return Step; }
    Block *getBody() { return Body; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class ForEachStmt : public AST
{
    llvm::StringRef IteratorName;
    llvm::StringRef CollectionName;
    Block *Body;
public:
    ForEachStmt(llvm::StringRef Iter, llvm::StringRef Col, Block *Body)
        : IteratorName(Iter), CollectionName(Col), Body(Body) {}
    llvm::StringRef getIterator() const { return IteratorName; }
    llvm::StringRef getCollection() const { return CollectionName; }
    Block *getBody() { return Body; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class PrintStmt : public AST
{
    Expr *Arg;
public:
    PrintStmt(Expr *Arg) : Arg(Arg) {}
    Expr *getArg() { return Arg; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class ArrayLiteral : public Expr {
    llvm::SmallVector<Expr*, 8> Values;
public:
    ArrayLiteral(const llvm::SmallVector<Expr*, 8> &Vals) : Values(Vals) {}
    const llvm::SmallVector<Expr*, 8> &getValues() const { return Values; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class BuiltinCall : public Expr {
    std::string FuncName;
    llvm::SmallVector<Expr*, 4> Args;
public:
    BuiltinCall(const std::string &Name, const llvm::SmallVector<Expr*, 4> &Args)
        : FuncName(Name), Args(Args) {}
    std::string getName() const { return FuncName; }
    const llvm::SmallVector<Expr*, 4> &getArgs() const { return Args; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

#endif