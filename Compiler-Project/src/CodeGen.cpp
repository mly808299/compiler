#include "CodeGen.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

CodeGen::CodeGen() : Builder(Context) {
    M = new Module("main", Context);
}

void CodeGen::compile(Block *Tree) {
    FunctionType *MainType = FunctionType::get(Type::getInt32Ty(Context), false);
    Function *MainFunc = Function::Create(MainType, Function::ExternalLinkage, "main", M);
    BasicBlock *Entry = BasicBlock::Create(Context, "entry", MainFunc);
    Builder.SetInsertPoint(Entry);

    Tree->accept(*this);

    Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(Context), 0));
    M->print(outs(), nullptr);
}

AllocaInst *CodeGen::CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName, Type *Type) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type, 0, VarName);
}

void CodeGen::visit(Block &Node) {
    for (auto *Stmt : Node.getStatements()) {
        if (Stmt) Stmt->accept(*this);
    }
}

void CodeGen::visit(Declaration &Node) {
    Type *Ty = Type::getInt32Ty(Context);
    if (Node.getType() == "bool") Ty = Type::getInt1Ty(Context);
    if (Node.getType() == "array") Ty = Type::getInt32PtrTy(Context);

    AllocaInst *Alloca = CreateEntryBlockAlloca(Builder.GetInsertBlock()->getParent(), Node.getName(), Ty);
    NamedValues[std::string(Node.getName())] = Alloca;

    if (Node.getInit()) {
        Node.getInit()->accept(*this);
        if (V->getType() != Ty && V->getType()->isPointerTy() && Ty->isPointerTy()) {
             V = Builder.CreatePointerCast(V, Ty);
        }
        Builder.CreateStore(V, Alloca);
    } else {
        Builder.CreateStore(Constant::getNullValue(Ty), Alloca);
    }
}

void CodeGen::visit(Assignment &Node) {
    Node.getValue()->accept(*this);
    Value *Val = V;

    if (NamedValues.find(std::string(Node.getName())) == NamedValues.end()) {
        errs() << "CodeGen Error: Unknown variable " << Node.getName() << "\n";
        return;
    }
    Value *VarPtr = NamedValues[std::string(Node.getName())];
    Builder.CreateStore(Val, VarPtr);
}

void CodeGen::visit(Final &Node) {
    if (Node.getKind() == Final::Number) {
        V = ConstantInt::get(Type::getInt32Ty(Context), std::stoi(std::string(Node.getValue())));
    } else if (Node.getKind() == Final::Bool) {
        V = ConstantInt::get(Type::getInt1Ty(Context), (Node.getValue() == "true") ? 1 : 0);
    } else if (Node.getKind() == Final::Ident) {
        Value *VarPtr = NamedValues[std::string(Node.getValue())];
        if (!VarPtr) {
             errs() << "Error: Unknown variable " << Node.getValue() << "\n";
             V = UndefValue::get(Type::getInt32Ty(Context));
             return;
        }
        V = Builder.CreateLoad(VarPtr->getType()->getPointerElementType(), VarPtr, Node.getValue());
    }
}

void CodeGen::visit(BinaryOp &Node) {
    Node.getLeft()->accept(*this);
    Value *L = V;
    Node.getRight()->accept(*this);
    Value *R = V;
    if (!L || !R) return;

    switch (Node.getOperator()) {
        case BinaryOp::Plus: V = Builder.CreateAdd(L, R, "addtmp"); break;
        case BinaryOp::Minus: V = Builder.CreateSub(L, R, "subtmp"); break;
        case BinaryOp::Mul: V = Builder.CreateMul(L, R, "multmp"); break;
        case BinaryOp::Div: V = Builder.CreateSDiv(L, R, "divtmp"); break;
        case BinaryOp::Mod: V = Builder.CreateSRem(L, R, "modtmp"); break;
        case BinaryOp::And: V = Builder.CreateAnd(L, R, "andtmp"); break;
        case BinaryOp::Or:  V = Builder.CreateOr(L, R, "ortmp"); break;
        case BinaryOp::Eq: V = Builder.CreateICmpEQ(L, R, "eqtmp"); break;
        case BinaryOp::Neq: V = Builder.CreateICmpNE(L, R, "neqtmp"); break;
        case BinaryOp::Lt: V = Builder.CreateICmpSLT(L, R, "lttmp"); break;
        case BinaryOp::Gt: V = Builder.CreateICmpSGT(L, R, "gttmp"); break;
        case BinaryOp::Lte: V = Builder.CreateICmpSLE(L, R, "letmp"); break;
        case BinaryOp::Gte: V = Builder.CreateICmpSGE(L, R, "getmp"); break;
    }
}

void CodeGen::visit(IfStmt &Node) {
    Node.getCond()->accept(*this);
    Value *CondV = V;
    CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(CondV->getType(), 0), "ifcond");

    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *ThenBB = BasicBlock::Create(Context, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(Context, "else");
    BasicBlock *MergeBB = BasicBlock::Create(Context, "ifcont");

    bool hasElse = Node.getElse() != nullptr;
    Builder.CreateCondBr(CondV, ThenBB, hasElse ? ElseBB : MergeBB);

    Builder.SetInsertPoint(ThenBB);
    Node.getThen()->accept(*this);
    Builder.CreateBr(MergeBB);

    if (hasElse) {
        TheFunction->getBasicBlockList().push_back(ElseBB);
        Builder.SetInsertPoint(ElseBB);
        Node.getElse()->accept(*this);
        Builder.CreateBr(MergeBB);
    }
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
}

// --- تغییرات جدید برای حلقه For ---
void CodeGen::visit(ForStmt &Node) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();

    // 1. مقداردهی اولیه (Init)
    if (Node.getInit()) Node.getInit()->accept(*this);

    // ساخت بلاک‌های مورد نیاز
    BasicBlock *CondBB = BasicBlock::Create(Context, "loop.cond", TheFunction);
    BasicBlock *BodyBB = BasicBlock::Create(Context, "loop.body");
    BasicBlock *IncBB = BasicBlock::Create(Context, "loop.inc");
    BasicBlock *EndBB = BasicBlock::Create(Context, "loop.end");

    // پرش به شرط
    Builder.CreateBr(CondBB);

    // 2. بررسی شرط (Condition)
    Builder.SetInsertPoint(CondBB);
    if (Node.getCond()) {
        Node.getCond()->accept(*this);
        Value *CondV = V;
        CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(CondV->getType(), 0), "loopcond");
        Builder.CreateCondBr(CondV, BodyBB, EndBB);
    } else {
        Builder.CreateBr(BodyBB); // حلقه بی‌نهایت
    }

    // 3. بدنه حلقه (Body)
    TheFunction->getBasicBlockList().push_back(BodyBB);
    Builder.SetInsertPoint(BodyBB);
    Node.getBody()->accept(*this);
    Builder.CreateBr(IncBB);

    // 4. گام افزایشی (Step/Increment)
    TheFunction->getBasicBlockList().push_back(IncBB);
    Builder.SetInsertPoint(IncBB);
    if (Node.getStep()) Node.getStep()->accept(*this);
    Builder.CreateBr(CondBB); // بازگشت به شرط

    // 5. پایان حلقه
    TheFunction->getBasicBlockList().push_back(EndBB);
    Builder.SetInsertPoint(EndBB);
}

void CodeGen::visit(ForEachStmt &Node) {
    // پیاده‌سازی Foreach نیاز به مدیریت طول آرایه دارد که در فاز بعدی کامل می‌شود.
}

void CodeGen::visit(PrintStmt &Node) {
    Node.getArg()->accept(*this);
    Value *Val = V;
    std::vector<Type*> Args;
    Args.push_back(Type::getInt32Ty(Context));
    FunctionType *PrintType = FunctionType::get(Type::getVoidTy(Context), Args, false);
    FunctionCallee PrintFunc = M->getOrInsertFunction("compiler_write", PrintType);
    Builder.CreateCall(PrintFunc, Val);
}

void CodeGen::visit(ArrayLiteral &Node) {
    int Size = Node.getValues().size();
    Type *IntType = Type::getInt32Ty(Context);
    ArrayType *ArrType = ArrayType::get(IntType, Size);
    AllocaInst *ArrAlloca = Builder.CreateAlloca(ArrType, nullptr, "arraytmp");

    for (int i = 0; i < Size; ++i) {
        Node.getValues()[i]->accept(*this);
        Value *Val = V;
        std::vector<Value*> Indices;
        Indices.push_back(ConstantInt::get(IntType, 0));
        Indices.push_back(ConstantInt::get(IntType, i));
        Value *Ptr = Builder.CreateInBoundsGEP(ArrType, ArrAlloca, Indices, "ptr");
        Builder.CreateStore(Val, Ptr);
    }

    std::vector<Value*> Indices0;
    Indices0.push_back(ConstantInt::get(IntType, 0));
    Indices0.push_back(ConstantInt::get(IntType, 0));
    V = Builder.CreateInBoundsGEP(ArrType, ArrAlloca, Indices0, "arrayptr");
}

void CodeGen::visit(BuiltinCall &Node) {
    if (Node.getName() == "length") {
        V = ConstantInt::get(Type::getInt32Ty(Context), 5);
    } else if (Node.getName() == "index") {
        auto Args = Node.getArgs();
        Args[0]->accept(*this);
        Value *ArrPtr = V;
        Args[1]->accept(*this);
        Value *Idx = V;
        Value *ElemPtr = Builder.CreateGEP(Type::getInt32Ty(Context), ArrPtr, Idx, "elemptr");
        V = Builder.CreateLoad(Type::getInt32Ty(Context), ElemPtr, "elemval");
    }
}