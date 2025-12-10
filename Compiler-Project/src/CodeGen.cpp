#include "CodeGen.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

CodeGen::CodeGen() : Builder(Context) {
    M = new Module("main", Context);
    setupPrintf();
    setupExit();
}

void CodeGen::setupPrintf() {
    std::vector<Type*> Args;
    Args.push_back(Type::getInt8PtrTy(Context));
    FunctionType *PrintfType = FunctionType::get(Builder.getInt32Ty(), Args, true);
    PrintfFunc = M->getOrInsertFunction("printf", PrintfType);
}

void CodeGen::setupExit() {
    std::vector<Type*> Args;
    Args.push_back(Type::getInt32Ty(Context));
    FunctionType *ExitType = FunctionType::get(Type::getVoidTy(Context), Args, false);
    ExitFunc = M->getOrInsertFunction("exit", ExitType);
}

void CodeGen::dumpLLVMIR() {
    std::error_code EC;
    llvm::raw_fd_ostream dest("compiler.ll", EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file compiler.ll: " << EC.message() << "\n";
        return;
    }
    M->print(dest, nullptr);
}

void CodeGen::compile(Block *Tree) {
    FunctionType *MainType = FunctionType::get(Type::getInt32Ty(Context), false);
    Function *MainFunc = Function::Create(MainType, Function::ExternalLinkage, "main", M);
    BasicBlock *Entry = BasicBlock::Create(Context, "entry", MainFunc);
    Builder.SetInsertPoint(Entry);

    Tree->accept(*this);

    Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(Context), 0));
    dumpLLVMIR();
}

AllocaInst *CodeGen::CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName, Type *Type) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type, nullptr, VarName);
}

Value* castToType(Value* val, Type* expectedType, IRBuilder<> &Builder) {
    Type* currentType = val->getType();
    if (currentType == expectedType) return val;
    if (currentType->isFloatingPointTy() && expectedType->isIntegerTy()) return Builder.CreateFPToSI(val, expectedType, "cast_f2i");
    if (currentType->isIntegerTy() && expectedType->isFloatingPointTy()) return Builder.CreateSIToFP(val, expectedType, "cast_i2f");
    if (currentType->isIntegerTy(1) && expectedType->isIntegerTy(32)) return Builder.CreateZExt(val, expectedType, "cast_b2i");
    if (currentType->isIntegerTy(32) && expectedType->isIntegerTy(1)) return Builder.CreateICmpNE(val, ConstantInt::get(currentType, 0), "cast_i2b");
    return val;
}

void CodeGen::visit(Block &Node) { for (auto *Stmt : Node.getStatements()) if (Stmt) Stmt->accept(*this); }

void CodeGen::visit(Declaration &Node) {
    Type *Ty = Type::getInt32Ty(Context);
    if (Node.getType() == "bool") Ty = Type::getInt1Ty(Context);
    if (Node.getType() == "float") Ty = Type::getFloatTy(Context);
    if (Node.getType() == "array") Ty = Type::getInt32PtrTy(Context);
    AllocaInst *Alloca = CreateEntryBlockAlloca(Builder.GetInsertBlock()->getParent(), Node.getName(), Ty);
    NamedValues[std::string(Node.getName())] = Alloca;
    if (Node.getInit()) {
        Node.getInit()->accept(*this);
        if (!Ty->isPointerTy()) V = castToType(V, Ty, Builder);
        Builder.CreateStore(V, Alloca);
    } else {
        if (Ty->isPointerTy()) Builder.CreateStore(ConstantPointerNull::get(cast<PointerType>(Ty)), Alloca);
        else Builder.CreateStore(Constant::getNullValue(Ty), Alloca);
    }
}

Value* CodeGen::getElemPtr(StringRef VarName, Expr *IndexExpr) {
    if (NamedValues.find(std::string(VarName)) == NamedValues.end()) return nullptr;
    AllocaInst *VarPtr = NamedValues[std::string(VarName)];
    if (IndexExpr) {
        Value *ArrBase = Builder.CreateLoad(VarPtr->getAllocatedType(), VarPtr, "arrBase");
        IndexExpr->accept(*this);
        Value *Idx = V;
        Value *RealIdx = Builder.CreateAdd(Idx, ConstantInt::get(Type::getInt32Ty(Context), 1));
        return Builder.CreateGEP(Type::getInt32Ty(Context), ArrBase, RealIdx, "elemPtr");
    }
    return VarPtr;
}

void CodeGen::visit(Assignment &Node) {
    Value *Ptr = getElemPtr(Node.getName(), Node.getIndex());
    if (!Ptr) return;
    Node.getValue()->accept(*this);
    Value *Val = V;
    if (Node.getIndex()) {
        Val = castToType(Val, Type::getInt32Ty(Context), Builder);
        Builder.CreateStore(Val, Ptr);
    } else {
        AllocaInst* Alloc = cast<AllocaInst>(Ptr);
        if (!Alloc->getAllocatedType()->isPointerTy()) Val = castToType(Val, Alloc->getAllocatedType(), Builder);
        Builder.CreateStore(Val, Ptr);
    }
}

void CodeGen::visit(CompoundStmt &Node) {
    Value *Ptr = getElemPtr(Node.getName(), Node.getIndex());
    if (!Ptr) return;
    Value *CurVal = Builder.CreateLoad(Type::getInt32Ty(Context), Ptr);
    Node.getValue()->accept(*this);
    Value *AddVal = castToType(V, CurVal->getType(), Builder);
    Value *Result = nullptr;
    if (Node.getOperator() == CompoundStmt::PLE) Result = Builder.CreateAdd(CurVal, AddVal);
    else Result = Builder.CreateSub(CurVal, AddVal);
    Builder.CreateStore(Result, Ptr);
}

void CodeGen::visit(UnaryStmt &Node) {
    Value *Ptr = getElemPtr(Node.getName(), Node.getIndex());
    if (!Ptr) return;
    Value *CurVal = Builder.CreateLoad(Type::getInt32Ty(Context), Ptr);
    Value *One = ConstantInt::get(CurVal->getType(), 1);
    Value *Result = nullptr;
    if (Node.getOperator() == UnaryStmt::INC) Result = Builder.CreateAdd(CurVal, One);
    else Result = Builder.CreateSub(CurVal, One);
    Builder.CreateStore(Result, Ptr);
}

void CodeGen::visit(ArrayAccess &Node) {
    Value *Ptr = getElemPtr(Node.getName(), Node.getIndex());
    if (Ptr) V = Builder.CreateLoad(Type::getInt32Ty(Context), Ptr, "elemVal");
}

void CodeGen::visit(Final &Node) {
    if (Node.getKind() == Final::Number) V = ConstantInt::get(Type::getInt32Ty(Context), std::stoi(std::string(Node.getValue())));
    else if (Node.getKind() == Final::Float) V = ConstantFP::get(Type::getFloatTy(Context), std::stof(std::string(Node.getValue())));
    else if (Node.getKind() == Final::Bool) V = ConstantInt::get(Type::getInt1Ty(Context), (Node.getValue() == "true") ? 1 : 0);
    else if (Node.getKind() == Final::String) {
        StringRef Str = Node.getValue();
        if (Str.startswith("\"") && Str.endswith("\"")) Str = Str.drop_front(1).drop_back(1);
        V = Builder.CreateGlobalStringPtr(Str);
    } else if (Node.getKind() == Final::Ident) {
        if (NamedValues.find(std::string(Node.getValue())) != NamedValues.end()) {
            AllocaInst *VarPtr = NamedValues[std::string(Node.getValue())];
            V = Builder.CreateLoad(VarPtr->getAllocatedType(), VarPtr, Node.getValue());
        } else {
            if (Node.getValue() == "_") V = ConstantInt::get(Type::getInt32Ty(Context), 0);
            else V = UndefValue::get(Type::getInt32Ty(Context));
        }
    } else if (Node.getKind() == Final::Underscore) V = ConstantInt::get(Type::getInt32Ty(Context), 0);
}

void CodeGen::visit(BinaryOp &Node) {
    Node.getLeft()->accept(*this); Value *L = V;
    Node.getRight()->accept(*this); Value *R = V;
    if (!L || !R) return;
    if (L->getType()->isFloatingPointTy() && R->getType()->isIntegerTy()) R = Builder.CreateSIToFP(R, L->getType());
    else if (L->getType()->isIntegerTy() && R->getType()->isFloatingPointTy()) L = Builder.CreateSIToFP(L, R->getType());
    bool isFloat = L->getType()->isFloatingPointTy();
    switch (Node.getOperator()) {
        case BinaryOp::Plus:  V = isFloat ? Builder.CreateFAdd(L, R) : Builder.CreateAdd(L, R); break;
        case BinaryOp::Minus: V = isFloat ? Builder.CreateFSub(L, R) : Builder.CreateSub(L, R); break;
        case BinaryOp::Mul:   V = isFloat ? Builder.CreateFMul(L, R) : Builder.CreateMul(L, R); break;
        case BinaryOp::Div:   V = isFloat ? Builder.CreateFDiv(L, R) : Builder.CreateSDiv(L, R); break;
        case BinaryOp::Mod:   V = isFloat ? Builder.CreateFRem(L, R) : Builder.CreateSRem(L, R); break;
        case BinaryOp::And: V = Builder.CreateAnd(L, R); break;
        case BinaryOp::Or:  V = Builder.CreateOr(L, R); break;
        case BinaryOp::Eq:  V = isFloat ? Builder.CreateFCmpOEQ(L, R) : Builder.CreateICmpEQ(L, R); break;
        case BinaryOp::Neq: V = isFloat ? Builder.CreateFCmpONE(L, R) : Builder.CreateICmpNE(L, R); break;
        case BinaryOp::Lt:  V = isFloat ? Builder.CreateFCmpOLT(L, R) : Builder.CreateICmpSLT(L, R); break;
        case BinaryOp::Gt:  V = isFloat ? Builder.CreateFCmpOGT(L, R) : Builder.CreateICmpSGT(L, R); break;
        case BinaryOp::Lte: V = isFloat ? Builder.CreateFCmpOLE(L, R) : Builder.CreateICmpSLE(L, R); break;
        case BinaryOp::Gte: V = isFloat ? Builder.CreateFCmpOGE(L, R) : Builder.CreateICmpSGE(L, R); break;
    }
}

// --- تابعی که ارور Linker می‌داد، حالا اضافه شده ---
void CodeGen::visit(ArrayLiteral &Node) {
    int Size = Node.getValues().size();
    Type *IntType = Type::getInt32Ty(Context);
    ArrayType *ArrType = ArrayType::get(IntType, Size + 1);
    AllocaInst *ArrAlloca = Builder.CreateAlloca(ArrType, nullptr, "arraytmp");
    Value *LenPtr = Builder.CreateConstInBoundsGEP2_32(ArrType, ArrAlloca, 0, 0);
    Builder.CreateStore(ConstantInt::get(IntType, Size), LenPtr);
    for (int i = 0; i < Size; ++i) {
        Node.getValues()[i]->accept(*this);
        Value *Val = castToType(V, IntType, Builder);
        Value *ElemPtr = Builder.CreateConstInBoundsGEP2_32(ArrType, ArrAlloca, 0, i + 1);
        Builder.CreateStore(Val, ElemPtr);
    }
    std::vector<Value*> Indices0 = {ConstantInt::get(IntType, 0), ConstantInt::get(IntType, 0)};
    V = Builder.CreateInBoundsGEP(ArrType, ArrAlloca, Indices0, "arrayptr");
}
// -----------------------------------------------------

// --- تابع IF با پشتیبانی کامل از ELIF ---
void CodeGen::visit(IfStmt &Node) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *MergeBB = BasicBlock::Create(Context, "if.end");

    // جمع‌آوری تمام شرط‌ها (IF و ELIF ها)
    std::vector<std::pair<Expr*, Block*>> Conditions;
    Conditions.push_back({Node.getCond(), Node.getThen()});
    for(auto &E : Node.getElifs()) {
        Conditions.push_back(E);
    }

    // حلقه تولید کد برای هر شرط
    for (size_t i = 0; i < Conditions.size(); ++i) {
        Conditions[i].first->accept(*this);
        Value *CondV = V;
        if (CondV->getType()->isIntegerTy(32))
            CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(CondV->getType(), 0));

        BasicBlock *ThenBB = BasicBlock::Create(Context, "then", TheFunction);
        BasicBlock *FalseBB = nullptr;

        // اگر هنوز شرطی مانده یا else داریم، بلاک بعدی بساز
        if (i < Conditions.size() - 1) {
            FalseBB = BasicBlock::Create(Context, "next.elif", TheFunction);
        } else if (Node.getElse()) {
            FalseBB = BasicBlock::Create(Context, "else", TheFunction);
        } else {
            FalseBB = MergeBB; // اگر هیچی نمانده، برو پایان
        }

        Builder.CreateCondBr(CondV, ThenBB, FalseBB);

        Builder.SetInsertPoint(ThenBB);
        Conditions[i].second->accept(*this);
        Builder.CreateBr(MergeBB);

        if (FalseBB != MergeBB) {
            Builder.SetInsertPoint(FalseBB);
        }
    }

    // تولید کد برای ELSE
    if (Node.getElse()) {
        Node.getElse()->accept(*this);
        Builder.CreateBr(MergeBB);
    }

    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
}
// ----------------------------------------

void CodeGen::visit(ForStmt &Node) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    if (Node.getInit()) Node.getInit()->accept(*this);
    BasicBlock *CondBB = BasicBlock::Create(Context, "loop.cond", TheFunction);
    BasicBlock *BodyBB = BasicBlock::Create(Context, "loop.body");
    BasicBlock *IncBB = BasicBlock::Create(Context, "loop.inc");
    BasicBlock *EndBB = BasicBlock::Create(Context, "loop.end");
    Builder.CreateBr(CondBB);
    Builder.SetInsertPoint(CondBB);
    if (Node.getCond()) {
        Node.getCond()->accept(*this);
        Value *CondV = V;
        if (CondV->getType()->isIntegerTy(32)) CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(CondV->getType(), 0));
        Builder.CreateCondBr(CondV, BodyBB, EndBB);
    } else { Builder.CreateBr(BodyBB); }
    TheFunction->getBasicBlockList().push_back(BodyBB);
    Builder.SetInsertPoint(BodyBB);
    Node.getBody()->accept(*this);
    Builder.CreateBr(IncBB);
    TheFunction->getBasicBlockList().push_back(IncBB);
    Builder.SetInsertPoint(IncBB);
    if (Node.getStep()) Node.getStep()->accept(*this);
    Builder.CreateBr(CondBB);
    TheFunction->getBasicBlockList().push_back(EndBB);
    Builder.SetInsertPoint(EndBB);
}

void CodeGen::visit(ForEachStmt &Node) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    Type *IntType = Type::getInt32Ty(Context);
    if (NamedValues.find(std::string(Node.getCollection())) == NamedValues.end()) return;
    AllocaInst *ArrPtrLoc = NamedValues[std::string(Node.getCollection())];
    Value *ArrBasePtr = Builder.CreateLoad(ArrPtrLoc->getAllocatedType(), ArrPtrLoc, "arrBase");
    Value *SizeVal = Builder.CreateLoad(IntType, ArrBasePtr, "arrSize");
    AllocaInst *IndexVar = CreateEntryBlockAlloca(TheFunction, "idx", IntType);
    Builder.CreateStore(ConstantInt::get(IntType, 0), IndexVar);
    AllocaInst *ItemVar = CreateEntryBlockAlloca(TheFunction, Node.getIterator(), IntType);
    NamedValues[std::string(Node.getIterator())] = ItemVar;
    BasicBlock *CondBB = BasicBlock::Create(Context, "loop.cond", TheFunction);
    BasicBlock *BodyBB = BasicBlock::Create(Context, "loop.body");
    BasicBlock *EndBB = BasicBlock::Create(Context, "loop.end");
    Builder.CreateBr(CondBB);
    Builder.SetInsertPoint(CondBB);
    Value *CurIdx = Builder.CreateLoad(IntType, IndexVar);
    Value *LoopCond = Builder.CreateICmpSLT(CurIdx, SizeVal);
    Builder.CreateCondBr(LoopCond, BodyBB, EndBB);
    TheFunction->getBasicBlockList().push_back(BodyBB);
    Builder.SetInsertPoint(BodyBB);
    Value *RealIdx = Builder.CreateAdd(CurIdx, ConstantInt::get(IntType, 1));
    Value *ElemPtr = Builder.CreateGEP(IntType, ArrBasePtr, RealIdx);
    Value *ElemVal = Builder.CreateLoad(IntType, ElemPtr);
    Builder.CreateStore(ElemVal, ItemVar);
    Node.getBody()->accept(*this);
    Value *NextIdx = Builder.CreateAdd(CurIdx, ConstantInt::get(IntType, 1));
    Builder.CreateStore(NextIdx, IndexVar);
    Builder.CreateBr(CondBB);
    TheFunction->getBasicBlockList().push_back(EndBB);
    Builder.SetInsertPoint(EndBB);
}

void CodeGen::visit(MatchStmt &Node) {
    Node.getTarget()->accept(*this);
    Value *TargetVal = V;
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *MergeBB = BasicBlock::Create(Context, "match.cont");
    for (auto &Case : Node.getCases()) {
        BasicBlock *CaseBB = BasicBlock::Create(Context, "case.body", TheFunction);
        BasicBlock *NextCheckBB = BasicBlock::Create(Context, "case.next");
        bool isUnderscore = false;
        if (auto *F = dynamic_cast<Final*>(Case.first)) { if (F->getKind() == Final::Underscore) isUnderscore = true; }
        if (isUnderscore) { Builder.CreateBr(CaseBB); }
        else {
            Case.first->accept(*this);
            Value *PatternVal = castToType(V, TargetVal->getType(), Builder);
            Value *Cmp = (TargetVal->getType()->isFloatingPointTy()) ? Builder.CreateFCmpOEQ(TargetVal, PatternVal) : Builder.CreateICmpEQ(TargetVal, PatternVal);
            Builder.CreateCondBr(Cmp, CaseBB, NextCheckBB);
        }
        Builder.SetInsertPoint(CaseBB);
        Case.second->accept(*this);
        Builder.CreateBr(MergeBB);
        TheFunction->getBasicBlockList().push_back(NextCheckBB);
        Builder.SetInsertPoint(NextCheckBB);
    }
    Builder.CreateBr(MergeBB);
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
}

void CodeGen::visit(BuiltinCall &Node) {
    if (Node.getName() == "to_int") {
        Node.getArgs()[0]->accept(*this); V = castToType(V, Type::getInt32Ty(Context), Builder);
    } else if (Node.getName() == "to_float") {
        Node.getArgs()[0]->accept(*this); V = castToType(V, Type::getFloatTy(Context), Builder);
    } else if (Node.getName() == "to_bool") {
        Node.getArgs()[0]->accept(*this); V = castToType(V, Type::getInt1Ty(Context), Builder);
    } else if (Node.getName() == "length") {
        Node.getArgs()[0]->accept(*this); V = Builder.CreateLoad(Type::getInt32Ty(Context), V, "len");
    } else if (Node.getName() == "index") {
        Node.getArgs()[0]->accept(*this); Value *ArrPtr = V;
        Node.getArgs()[1]->accept(*this); Value *Idx = V;
        Value *Len = Builder.CreateLoad(Type::getInt32Ty(Context), ArrPtr, "len");
        Function *TheFunction = Builder.GetInsertBlock()->getParent();
        BasicBlock *ValidBB = BasicBlock::Create(Context, "valid.index");
        BasicBlock *ErrorBB = BasicBlock::Create(Context, "error.index");
        BasicBlock *MergeBB = BasicBlock::Create(Context, "merge.index");
        Value *Cond1 = Builder.CreateICmpSGE(Idx, ConstantInt::get(Type::getInt32Ty(Context), 0));
        Value *Cond2 = Builder.CreateICmpSLT(Idx, Len);
        Value *IsValid = Builder.CreateAnd(Cond1, Cond2);
        Builder.CreateCondBr(IsValid, ValidBB, ErrorBB);
        TheFunction->getBasicBlockList().push_back(ErrorBB);
        Builder.SetInsertPoint(ErrorBB);
        Value *ErrMsg = Builder.CreateGlobalStringPtr("Runtime Error: Index out of bounds!\n");
        Builder.CreateCall(PrintfFunc, {ErrMsg});
        std::vector<Value*> ExitArgs; ExitArgs.push_back(ConstantInt::get(Type::getInt32Ty(Context), 1));
        Builder.CreateCall(ExitFunc, ExitArgs);
        Builder.CreateUnreachable();
        TheFunction->getBasicBlockList().push_back(ValidBB);
        Builder.SetInsertPoint(ValidBB);
        Value *RealIdx = Builder.CreateAdd(Idx, ConstantInt::get(Type::getInt32Ty(Context), 1));
        Value *ElemPtr = Builder.CreateGEP(Type::getInt32Ty(Context), ArrPtr, RealIdx);
        V = Builder.CreateLoad(Type::getInt32Ty(Context), ElemPtr);
        Builder.CreateBr(MergeBB);
        TheFunction->getBasicBlockList().push_back(MergeBB);
        Builder.SetInsertPoint(MergeBB);
    }
    else if (Node.getName() == "abs") {
        Node.getArgs()[0]->accept(*this);
        Value *Val = V;
        if (Val->getType()->isFloatingPointTy()) {
            Value *Zero = ConstantFP::get(Val->getType(), 0.0);
            Value *IsNeg = Builder.CreateFCmpOLT(Val, Zero);
            Value *NegVal = Builder.CreateFSub(Zero, Val);
            V = Builder.CreateSelect(IsNeg, NegVal, Val);
        } else {
            Value *Zero = ConstantInt::get(Val->getType(), 0);
            Value *IsNeg = Builder.CreateICmpSLT(Val, Zero);
            Value *NegVal = Builder.CreateSub(Zero, Val);
            V = Builder.CreateSelect(IsNeg, NegVal, Val);
        }
    }
    else if (Node.getName() == "max") {
        Node.getArgs()[0]->accept(*this);
        Value *ArrPtr = V;
        Type *IntType = Type::getInt32Ty(Context);
        Function *TheFunction = Builder.GetInsertBlock()->getParent();
        Value *Len = Builder.CreateLoad(IntType, ArrPtr, "len");
        Value *FirstPtr = Builder.CreateGEP(IntType, ArrPtr, ConstantInt::get(IntType, 1));
        Value *FirstVal = Builder.CreateLoad(IntType, FirstPtr, "firstVal");
        AllocaInst *MaxVar = CreateEntryBlockAlloca(TheFunction, "maxVal", IntType);
        Builder.CreateStore(FirstVal, MaxVar);
        AllocaInst *IdxVar = CreateEntryBlockAlloca(TheFunction, "idx", IntType);
        Builder.CreateStore(ConstantInt::get(IntType, 2), IdxVar);
        BasicBlock *CondBB = BasicBlock::Create(Context, "max.cond", TheFunction);
        BasicBlock *BodyBB = BasicBlock::Create(Context, "max.body");
        BasicBlock *EndBB = BasicBlock::Create(Context, "max.end");
        Builder.CreateBr(CondBB);
        Builder.SetInsertPoint(CondBB);
        Value *CurIdx = Builder.CreateLoad(IntType, IdxVar);
        Value *LoopCond = Builder.CreateICmpSLE(CurIdx, Len);
        Builder.CreateCondBr(LoopCond, BodyBB, EndBB);
        TheFunction->getBasicBlockList().push_back(BodyBB);
        Builder.SetInsertPoint(BodyBB);
        Value *ElemPtr = Builder.CreateGEP(IntType, ArrPtr, CurIdx);
        Value *ElemVal = Builder.CreateLoad(IntType, ElemPtr);
        Value *CurMax = Builder.CreateLoad(IntType, MaxVar);
        Value *IsGreater = Builder.CreateICmpSGT(ElemVal, CurMax);
        Value *NewMax = Builder.CreateSelect(IsGreater, ElemVal, CurMax);
        Builder.CreateStore(NewMax, MaxVar);
        Value *NextIdx = Builder.CreateAdd(CurIdx, ConstantInt::get(IntType, 1));
        Builder.CreateStore(NextIdx, IdxVar);
        Builder.CreateBr(CondBB);
        TheFunction->getBasicBlockList().push_back(EndBB);
        Builder.SetInsertPoint(EndBB);
        V = Builder.CreateLoad(IntType, MaxVar);
    }
}

void CodeGen::visit(PrintStmt &Node) {
    Node.getArg()->accept(*this);
    Value *Val = V;
    Value *FormatStr;
    if (Val->getType()->isPointerTy()) FormatStr = Builder.CreateGlobalStringPtr("%s\n");
    else if (Val->getType()->isFloatingPointTy()) {
        FormatStr = Builder.CreateGlobalStringPtr("%f\n");
        Val = Builder.CreateFPExt(Val, Type::getDoubleTy(Context));
    } else if (Val->getType()->isIntegerTy(1)) {
        FormatStr = Builder.CreateGlobalStringPtr("%d\n");
        Val = Builder.CreateZExt(Val, Type::getInt32Ty(Context));
    } else FormatStr = Builder.CreateGlobalStringPtr("%d\n");
    std::vector<Value*> ArgsV = {FormatStr, Val};
    Builder.CreateCall(PrintfFunc, ArgsV);
}

void CodeGen::visit(RangeExpr &Node) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    Type *IntType = Type::getInt32Ty(Context);
    if (NamedValues.find(std::string(Node.getList())) == NamedValues.end()) {
        errs() << "Error: Source list '" << Node.getList() << "' not found\n";
        return;
    }
    AllocaInst *SrcPtrLoc = NamedValues[std::string(Node.getList())];
    Value *SrcBasePtr = Builder.CreateLoad(SrcPtrLoc->getAllocatedType(), SrcPtrLoc, "srcBase");
    Value *SizeVal = Builder.CreateLoad(IntType, SrcBasePtr, "srcSize");
    Value *AllocSize = Builder.CreateAdd(SizeVal, ConstantInt::get(IntType, 1));
    AllocaInst *DestArr = Builder.CreateAlloca(IntType, AllocSize, "destArr");
    AllocaInst *SrcIdxVar = CreateEntryBlockAlloca(TheFunction, "srcIdx", IntType);
    Builder.CreateStore(ConstantInt::get(IntType, 0), SrcIdxVar);
    AllocaInst *DestIdxVar = CreateEntryBlockAlloca(TheFunction, "destIdx", IntType);
    Builder.CreateStore(ConstantInt::get(IntType, 0), DestIdxVar);
    AllocaInst *IterVar = CreateEntryBlockAlloca(TheFunction, Node.getIterator(), IntType);
    NamedValues[std::string(Node.getIterator())] = IterVar;
    BasicBlock *CondBB = BasicBlock::Create(Context, "range.cond", TheFunction);
    BasicBlock *BodyBB = BasicBlock::Create(Context, "range.body");
    BasicBlock *IncBB = BasicBlock::Create(Context, "range.inc");
    BasicBlock *EndBB = BasicBlock::Create(Context, "range.end");
    Builder.CreateBr(CondBB);
    Builder.SetInsertPoint(CondBB);
    Value *CurSrcIdx = Builder.CreateLoad(IntType, SrcIdxVar, "curSrcIdx");
    Value *LoopCond = Builder.CreateICmpSLT(CurSrcIdx, SizeVal);
    Builder.CreateCondBr(LoopCond, BodyBB, EndBB);
    TheFunction->getBasicBlockList().push_back(BodyBB);
    Builder.SetInsertPoint(BodyBB);
    Value *RealSrcIdx = Builder.CreateAdd(CurSrcIdx, ConstantInt::get(IntType, 1));
    Value *SrcElemPtr = Builder.CreateGEP(IntType, SrcBasePtr, RealSrcIdx);
    Value *Val = Builder.CreateLoad(IntType, SrcElemPtr, "val");
    Builder.CreateStore(Val, IterVar);
    BasicBlock *ProcessBB = BasicBlock::Create(Context, "range.process", TheFunction);
    if (Node.getCondition()) {
        Node.getCondition()->accept(*this);
        Value *CondVal = V;
        if (CondVal->getType()->isIntegerTy(32)) CondVal = Builder.CreateICmpNE(CondVal, ConstantInt::get(IntType, 0));
        Builder.CreateCondBr(CondVal, ProcessBB, IncBB);
    } else { Builder.CreateBr(ProcessBB); }
    Builder.SetInsertPoint(ProcessBB);
    Node.getTargetExpr()->accept(*this);
    Value *NewVal = castToType(V, IntType, Builder);
    Value *CurDestCount = Builder.CreateLoad(IntType, DestIdxVar, "curDestCount");
    Value *DestRealIdx = Builder.CreateAdd(CurDestCount, ConstantInt::get(IntType, 1));
    Value *DestElemPtr = Builder.CreateGEP(IntType, DestArr, DestRealIdx);
    Builder.CreateStore(NewVal, DestElemPtr);
    Value *NextDestCount = Builder.CreateAdd(CurDestCount, ConstantInt::get(IntType, 1));
    Builder.CreateStore(NextDestCount, DestIdxVar);
    Builder.CreateBr(IncBB);
    TheFunction->getBasicBlockList().push_back(IncBB);
    Builder.SetInsertPoint(IncBB);
    Value *NextSrcIdx = Builder.CreateAdd(CurSrcIdx, ConstantInt::get(IntType, 1));
    Builder.CreateStore(NextSrcIdx, SrcIdxVar);
    Builder.CreateBr(CondBB);
    TheFunction->getBasicBlockList().push_back(EndBB);
    Builder.SetInsertPoint(EndBB);
    Value *FinalLen = Builder.CreateLoad(IntType, DestIdxVar);
    Value *LenPtr = Builder.CreateGEP(IntType, DestArr, ConstantInt::get(IntType, 0));
    Builder.CreateStore(FinalLen, LenPtr);
    V = DestArr;
}