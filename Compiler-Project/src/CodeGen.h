#ifndef CODEGEN_H
#define CODEGEN_H

#include "AST.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include <map>

class CodeGen : public ASTVisitor
{
    llvm::LLVMContext Context;
    llvm::Module *M;
    llvm::IRBuilder<> Builder;

    std::map<std::string, llvm::AllocaInst *> NamedValues;
    llvm::Value *V;

    llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, llvm::StringRef VarName, llvm::Type *Type);

    void setupPrintf();
    void setupExit();
    llvm::FunctionCallee PrintfFunc;
    llvm::FunctionCallee ExitFunc;

    // --- تابع کمکی جدید که فراموش شده بود ---
    llvm::Value* getElemPtr(llvm::StringRef VarName, Expr *IndexExpr); // <--- اضافه شد

public:
    CodeGen();
    void compile(Block *Tree);

    virtual void visit(AST &) override {}
    virtual void visit(Block &Node) override;
    virtual void visit(Declaration &Node) override;
    virtual void visit(Assignment &Node) override;
    virtual void visit(BinaryOp &Node) override;
    virtual void visit(Final &Node) override;
    virtual void visit(IfStmt &Node) override;
    virtual void visit(ForStmt &Node) override;
    virtual void visit(ForEachStmt &Node) override;
    virtual void visit(PrintStmt &Node) override;
    virtual void visit(ArrayLiteral &Node) override;
    virtual void visit(BuiltinCall &Node) override;

    virtual void visit(MatchStmt &Node) override;
    virtual void visit(CompoundStmt &Node) override;
    virtual void visit(UnaryStmt &Node) override;
    virtual void visit(RangeExpr &Node) override;

    // --- متد ویزیتور جدید که فراموش شده بود ---
    virtual void visit(ArrayAccess &Node) override; // <--- اضافه شد
    // در کلاس CodeGen (public):
    void dumpLLVMIR(); // <--- تابع جدید
};

#endif