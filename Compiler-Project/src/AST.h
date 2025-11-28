#ifndef AST_H
#define AST_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include <string>
#include <vector>
#include <utility>

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
class MatchStmt;
class CompoundStmt;
class UnaryStmt;
class RangeExpr;

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
    virtual void visit(MatchStmt &) = 0;
    virtual void visit(CompoundStmt &) = 0;
    virtual void visit(UnaryStmt &) = 0;
    virtual void visit(RangeExpr &) = 0;
};

class AST
{
    // --- تغییر جدید: ذخیره مکان برای مدیریت خطا ---
    int Line;
    int Col;
public:
    AST(int Line, int Col) : Line(Line), Col(Col) {}
    virtual ~AST() {}

    int getLine() const { return Line; }
    int getCol() const { return Col; }

    virtual void accept(ASTVisitor &V) = 0;
};

class Block : public AST
{
    llvm::SmallVector<AST *, 8> Statements;
public:
    // Block معمولا لوکیشن خاصی ندارد یا لوکیشن اولین دستور است، فعلا 0,0 میگذاریم یا از پارسر میگیریم
    Block(int Line, int Col) : AST(Line, Col) {}

    void addStatement(AST *S) { Statements.push_back(S); }
    const llvm::SmallVector<AST *, 8> &getStatements() const { return Statements; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class Expr : public AST {
public:
    Expr(int Line, int Col) : AST(Line, Col) {}
};

class Final : public Expr
{
public:
    enum ValueKind { Ident, Number, Float, Bool, String, Underscore };
private:
    ValueKind Kind;
    llvm::StringRef Val;
public:
    Final(int Line, int Col, ValueKind Kind, llvm::StringRef Val)
        : Expr(Line, Col), Kind(Kind), Val(Val) {}

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
    BinaryOp(int Line, int Col, Operator Op, Expr *L, Expr *R)
        : Expr(Line, Col), Op(Op), Left(L), Right(R) {}

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
    Declaration(int Line, int Col, llvm::StringRef Name, llvm::StringRef Type, Expr *Init = nullptr)
        : AST(Line, Col), VarName(Name), Type(Type), Init(Init) {}

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
    Assignment(int Line, int Col, llvm::StringRef Name, Expr *Val)
        : AST(Line, Col), VarName(Name), Value(Val) {}

    llvm::StringRef getName() const { return VarName; }
    Expr *getValue() { return Value; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class CompoundStmt : public AST
{
public:
    enum OpType { PLE, MIE };
private:
    llvm::StringRef VarName;
    Expr *Value;
    OpType Op;
public:
    CompoundStmt(int Line, int Col, llvm::StringRef Name, Expr *Val, OpType Op)
        : AST(Line, Col), VarName(Name), Value(Val), Op(Op) {}

    llvm::StringRef getName() const { return VarName; }
    Expr *getValue() { return Value; }
    OpType getOperator() { return Op; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class UnaryStmt : public AST
{
public:
    enum OpType { INC, DEC };
private:
    llvm::StringRef VarName;
    OpType Op;
public:
    UnaryStmt(int Line, int Col, llvm::StringRef Name, OpType Op)
        : AST(Line, Col), VarName(Name), Op(Op) {}

    llvm::StringRef getName() const { return VarName; }
    OpType getOperator() { return Op; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class IfStmt : public AST
{
    Expr *Cond;
    Block *ThenBlock;
    llvm::SmallVector<std::pair<Expr*, Block*>, 4> Elifs;
    Block *ElseBlock;
public:
    IfStmt(int Line, int Col, Expr *Cond, Block *Then, const llvm::SmallVector<std::pair<Expr*, Block*>, 4> &Elifs, Block *Else)
        : AST(Line, Col), Cond(Cond), ThenBlock(Then), Elifs(Elifs), ElseBlock(Else) {}

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
    ForStmt(int Line, int Col, AST *Init, Expr *Cond, AST *Step, Block *Body)
        : AST(Line, Col), Init(Init), Cond(Cond), Step(Step), Body(Body) {}

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
    ForEachStmt(int Line, int Col, llvm::StringRef Iter, llvm::StringRef ColName, Block *Body)
        : AST(Line, Col), IteratorName(Iter), CollectionName(ColName), Body(Body) {}

    llvm::StringRef getIterator() const { return IteratorName; }
    llvm::StringRef getCollection() const { return CollectionName; }
    Block *getBody() { return Body; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class MatchStmt : public AST
{
    Expr *Target;
    llvm::SmallVector<std::pair<Expr*, AST*>, 8> Cases;
public:
    MatchStmt(int Line, int Col, Expr *T, const llvm::SmallVector<std::pair<Expr*, AST*>, 8> &Cs)
        : AST(Line, Col), Target(T), Cases(Cs) {}

    Expr *getTarget() { return Target; }
    const auto &getCases() { return Cases; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class PrintStmt : public AST
{
    Expr *Arg;
public:
    PrintStmt(int Line, int Col, Expr *Arg) : AST(Line, Col), Arg(Arg) {}
    Expr *getArg() { return Arg; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class ArrayLiteral : public Expr {
    llvm::SmallVector<Expr*, 8> Values;
public:
    ArrayLiteral(int Line, int Col, const llvm::SmallVector<Expr*, 8> &Vals)
        : Expr(Line, Col), Values(Vals) {}

    const llvm::SmallVector<Expr*, 8> &getValues() const { return Values; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class RangeExpr : public Expr {
    Expr *TargetExpr;
    llvm::StringRef IterName;
    llvm::StringRef ListName;
    Expr *Condition;

public:
    RangeExpr(int Line, int Col, Expr *Target, llvm::StringRef Iter, llvm::StringRef List, Expr *Cond = nullptr)
        : Expr(Line, Col), TargetExpr(Target), IterName(Iter), ListName(List), Condition(Cond) {}

    Expr *getTargetExpr() { return TargetExpr; }
    llvm::StringRef getIterator() const { return IterName; }
    llvm::StringRef getList() const { return ListName; }
    Expr *getCondition() { return Condition; }

    void accept(ASTVisitor &V) override { V.visit(*this); }
};

class BuiltinCall : public Expr {
    std::string FuncName;
    llvm::SmallVector<Expr*, 4> Args;
public:
    BuiltinCall(int Line, int Col, const std::string &Name, const llvm::SmallVector<Expr*, 4> &Args)
        : Expr(Line, Col), FuncName(Name), Args(Args) {}

    std::string getName() const { return FuncName; }
    const llvm::SmallVector<Expr*, 4> &getArgs() const { return Args; }
    void accept(ASTVisitor &V) override { V.visit(*this); }
};

#endif