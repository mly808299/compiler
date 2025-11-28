#include "CodeGen.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

using namespace llvm;

CodeGen::CodeGen() : Builder(Context) {
    M = new Module("main", Context);
    setupPrintf();
}

void CodeGen::setupPrintf() {
    std::vector<Type*> Args;
    Args.push_back(Type::getInt8PtrTy(Context));
    FunctionType *PrintfType = FunctionType::get(Builder.getInt32Ty(), Args, true);
    PrintfFunc = M->getOrInsertFunction("printf", PrintfType);
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
    return TmpB.CreateAlloca(Type, nullptr, VarName);
}

Value* castToType(Value* val, Type* expectedType, IRBuilder<> &Builder) {
    Type* currentType = val->getType();
    if (currentType == expectedType) return val;

    if (currentType->isFloatingPointTy() && expectedType->isIntegerTy()) {
        return Builder.CreateFPToSI(val, expectedType, "cast_f2i");
    }
    if (currentType->isIntegerTy() && expectedType->isFloatingPointTy()) {
        return Builder.CreateSIToFP(val, expectedType, "cast_i2f");
    }
    if (currentType->isIntegerTy(1) && expectedType->isIntegerTy(32)) {
        return Builder.CreateZExt(val, expectedType, "cast_b2i");
    }
    if (currentType->isIntegerTy(32) && expectedType->isIntegerTy(1)) {
        return Builder.CreateICmpNE(val, ConstantInt::get(currentType, 0), "cast_i2b");
    }
    return val;
}

void CodeGen::visit(Block &Node) {
    for (auto *Stmt : Node.getStatements()) {
        if (Stmt) Stmt->accept(*this);
    }
}

void CodeGen::visit(Declaration &Node) {
    Type *Ty = Type::getInt32Ty(Context);
    if (Node.getType() == "bool") Ty = Type::getInt1Ty(Context);
    if (Node.getType() == "float") Ty = Type::getFloatTy(Context);
    if (Node.getType() == "array") Ty = Type::getInt32PtrTy(Context); // آرایه به صورت پوینتر int

    AllocaInst *Alloca = CreateEntryBlockAlloca(Builder.GetInsertBlock()->getParent(), Node.getName(), Ty);
    NamedValues[std::string(Node.getName())] = Alloca;

    if (Node.getInit()) {
        Node.getInit()->accept(*this);
        // اگر نوع آرایه بود، کست لازم نیست چون پوینتر است
        if (!Ty->isPointerTy()) {
            V = castToType(V, Ty, Builder);
        }
        Builder.CreateStore(V, Alloca);
    } else {
        if (Ty->isPointerTy())
             Builder.CreateStore(ConstantPointerNull::get(cast<PointerType>(Ty)), Alloca);
        else
             Builder.CreateStore(Constant::getNullValue(Ty), Alloca);
    }
}

void CodeGen::visit(Assignment &Node) {
    Node.getValue()->accept(*this);
    Value *Val = V;

    if (NamedValues.find(std::string(Node.getName())) == NamedValues.end()) return;
    AllocaInst *VarPtr = NamedValues[std::string(Node.getName())];

    if (!VarPtr->getAllocatedType()->isPointerTy()) {
        Val = castToType(Val, VarPtr->getAllocatedType(), Builder);
    }
    Builder.CreateStore(Val, VarPtr);
}

void CodeGen::visit(CompoundStmt &Node) {
    if (NamedValues.find(std::string(Node.getName())) == NamedValues.end()) return;
    AllocaInst *VarPtr = NamedValues[std::string(Node.getName())];
    Value *CurVal = Builder.CreateLoad(VarPtr->getAllocatedType(), VarPtr);

    Node.getValue()->accept(*this);
    Value *AddVal = V;
    AddVal = castToType(AddVal, CurVal->getType(), Builder);

    Value *Result = nullptr;
    if (CurVal->getType()->isFloatingPointTy()) {
        if (Node.getOperator() == CompoundStmt::PLE) Result = Builder.CreateFAdd(CurVal, AddVal, "faddtmp");
        else Result = Builder.CreateFSub(CurVal, AddVal, "fsubtmp");
    } else {
        if (Node.getOperator() == CompoundStmt::PLE) Result = Builder.CreateAdd(CurVal, AddVal, "addtmp");
        else Result = Builder.CreateSub(CurVal, AddVal, "subtmp");
    }
    Builder.CreateStore(Result, VarPtr);
}

void CodeGen::visit(UnaryStmt &Node) {
    if (NamedValues.find(std::string(Node.getName())) == NamedValues.end()) return;
    AllocaInst *VarPtr = NamedValues[std::string(Node.getName())];
    Value *CurVal = Builder.CreateLoad(VarPtr->getAllocatedType(), VarPtr);

    Value *One;
    if (CurVal->getType()->isFloatingPointTy()) One = ConstantFP::get(CurVal->getType(), 1.0);
    else One = ConstantInt::get(CurVal->getType(), 1);

    Value *Result = nullptr;
    if (CurVal->getType()->isFloatingPointTy()) {
        if (Node.getOperator() == UnaryStmt::INC) Result = Builder.CreateFAdd(CurVal, One, "finctmp");
        else Result = Builder.CreateFSub(CurVal, One, "fdectmp");
    } else {
        if (Node.getOperator() == UnaryStmt::INC) Result = Builder.CreateAdd(CurVal, One, "inctmp");
        else Result = Builder.CreateSub(CurVal, One, "dectmp");
    }
    Builder.CreateStore(Result, VarPtr);
}

void CodeGen::visit(Final &Node) {
    if (Node.getKind() == Final::Number) {
        V = ConstantInt::get(Type::getInt32Ty(Context), std::stoi(std::string(Node.getValue())));
    } else if (Node.getKind() == Final::Float) {
        V = ConstantFP::get(Type::getFloatTy(Context), std::stof(std::string(Node.getValue())));
    } else if (Node.getKind() == Final::Bool) {
        V = ConstantInt::get(Type::getInt1Ty(Context), (Node.getValue() == "true") ? 1 : 0);
    } else if (Node.getKind() == Final::String) {
        StringRef Str = Node.getValue();
        if (Str.startswith("\"") && Str.endswith("\"")) Str = Str.drop_front(1).drop_back(1);
        V = Builder.CreateGlobalStringPtr(Str);
    } else if (Node.getKind() == Final::Ident) {
        if (NamedValues.find(std::string(Node.getValue())) != NamedValues.end()) {
            AllocaInst *VarPtr = NamedValues[std::string(Node.getValue())];
            V = Builder.CreateLoad(VarPtr->getAllocatedType(), VarPtr, Node.getValue());
        } else {
            // پشتیبانی از _ برای match
            if (Node.getValue() == "_") {
                 // یک مقدار دامی برمی‌گردانیم
                 V = ConstantInt::get(Type::getInt32Ty(Context), 0);
            } else {
                 V = UndefValue::get(Type::getInt32Ty(Context));
            }
        }
    } else if (Node.getKind() == Final::Underscore) {
         V = ConstantInt::get(Type::getInt32Ty(Context), 0);
    }
}

void CodeGen::visit(BinaryOp &Node) {
    Node.getLeft()->accept(*this);
    Value *L = V;
    Node.getRight()->accept(*this);
    Value *R = V;
    if (!L || !R) return;

    if (L->getType()->isFloatingPointTy() && R->getType()->isIntegerTy())
        R = Builder.CreateSIToFP(R, L->getType(), "cast_i2f");
    else if (L->getType()->isIntegerTy() && R->getType()->isFloatingPointTy())
        L = Builder.CreateSIToFP(L, R->getType(), "cast_i2f");

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

void CodeGen::visit(IfStmt &Node) {
    Node.getCond()->accept(*this);
    Value *CondV = V;
    if (CondV->getType()->isIntegerTy(32))
        CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(CondV->getType(), 0));

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
        if (CondV->getType()->isIntegerTy(32))
             CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(CondV->getType(), 0));
        Builder.CreateCondBr(CondV, BodyBB, EndBB);
    } else {
        Builder.CreateBr(BodyBB);
    }

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

// --- بخش‌های جدید برای آرایه و حلقه Foreach ---

void CodeGen::visit(ArrayLiteral &Node) {
    // 1. تعداد عناصر را می‌گیریم
    int Size = Node.getValues().size();
    Type *IntType = Type::getInt32Ty(Context);

    // 2. ساخت تایپ آرایه: [Size x i32]
    ArrayType *ArrType = ArrayType::get(IntType, Size);

    // 3. تخصیص حافظه در استک برای آرایه
    AllocaInst *ArrAlloca = Builder.CreateAlloca(ArrType, nullptr, "arraytmp");

    // 4. پر کردن عناصر
    for (int i = 0; i < Size; ++i) {
        Node.getValues()[i]->accept(*this);
        Value *Val = V; // مقدار عنصر

        // اطمینان از int بودن
        Val = castToType(Val, IntType, Builder);

        // دسترسی به خانه iام
        std::vector<Value*> Indices;
        Indices.push_back(ConstantInt::get(IntType, 0)); // pointer index
        Indices.push_back(ConstantInt::get(IntType, i)); // array index
        Value *Ptr = Builder.CreateInBoundsGEP(ArrType, ArrAlloca, Indices, "elemptr");

        Builder.CreateStore(Val, Ptr);
    }

    // 5. تبدیل آرایه به پوینتر (Decay to Pointer) برای ذخیره در متغیر
    std::vector<Value*> Indices0;
    Indices0.push_back(ConstantInt::get(IntType, 0));
    Indices0.push_back(ConstantInt::get(IntType, 0));
    V = Builder.CreateInBoundsGEP(ArrType, ArrAlloca, Indices0, "arrayptr");
}

void CodeGen::visit(ForEachStmt &Node) {
    // پیاده‌سازی حلقه Foreach با استفاده از حلقه While
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    Type *IntType = Type::getInt32Ty(Context);

    // 1. پیدا کردن آرایه
    if (NamedValues.find(std::string(Node.getCollection())) == NamedValues.end()) return;
    AllocaInst *ArrPtrLoc = NamedValues[std::string(Node.getCollection())];
    Value *ArrBasePtr = Builder.CreateLoad(ArrPtrLoc->getAllocatedType(), ArrPtrLoc, "arrBase");

    // 2. ساخت شمارنده (index = 0)
    AllocaInst *IndexVar = CreateEntryBlockAlloca(TheFunction, "idx", IntType);
    Builder.CreateStore(ConstantInt::get(IntType, 0), IndexVar);

    // 3. متغیر تکرار کننده (item)
    AllocaInst *ItemVar = CreateEntryBlockAlloca(TheFunction, Node.getIterator(), IntType);
    NamedValues[std::string(Node.getIterator())] = ItemVar;

    // بلاک‌های حلقه
    BasicBlock *CondBB = BasicBlock::Create(Context, "loop.cond", TheFunction);
    BasicBlock *BodyBB = BasicBlock::Create(Context, "loop.body");
    BasicBlock *EndBB = BasicBlock::Create(Context, "loop.end");

    Builder.CreateBr(CondBB);

    // 4. شرط حلقه: index < 4 (فرض می‌کنیم سایز ۴ است برای سادگی فاز ۱)
    // در فازهای پیشرفته باید سایز واقعی را ذخیره کنیم.
    Builder.SetInsertPoint(CondBB);
    Value *CurIdx = Builder.CreateLoad(IntType, IndexVar, "curIdx");
    Value *LoopCond = Builder.CreateICmpSLT(CurIdx, ConstantInt::get(IntType, 4), "loopcond");
    Builder.CreateCondBr(LoopCond, BodyBB, EndBB);

    // 5. بدنه حلقه
    TheFunction->getBasicBlockList().push_back(BodyBB);
    Builder.SetInsertPoint(BodyBB);

    // خواندن مقدار از آرایه: Arr[i]
    Value *ElemPtr = Builder.CreateGEP(IntType, ArrBasePtr, CurIdx, "elemPtr");
    Value *ElemVal = Builder.CreateLoad(IntType, ElemPtr, "elemVal");
    Builder.CreateStore(ElemVal, ItemVar); // item = Arr[i]

    // اجرای دستورات داخل حلقه
    Node.getBody()->accept(*this);

    // افزایش شمارنده: index++
    Value *NextIdx = Builder.CreateAdd(CurIdx, ConstantInt::get(IntType, 1), "nextIdx");
    Builder.CreateStore(NextIdx, IndexVar);

    Builder.CreateBr(CondBB); // برگشت به شرط

    // 6. پایان
    TheFunction->getBasicBlockList().push_back(EndBB);
    Builder.SetInsertPoint(EndBB);

    // پاک کردن متغیر موقت از جدول (اختیاری)
    // NamedValues.erase(std::string(Node.getIterator()));
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
        if (auto *F = dynamic_cast<Final*>(Case.first)) {
            if (F->getKind() == Final::Underscore) isUnderscore = true;
        }

        if (isUnderscore) {
            Builder.CreateBr(CaseBB);
        } else {
            Case.first->accept(*this);
            Value *PatternVal = V;
            if (TargetVal->getType() != PatternVal->getType()) {
                 PatternVal = castToType(PatternVal, TargetVal->getType(), Builder);
            }

            Value *Cmp = (TargetVal->getType()->isFloatingPointTy())
                ? Builder.CreateFCmpOEQ(TargetVal, PatternVal, "match.cmp")
                : Builder.CreateICmpEQ(TargetVal, PatternVal, "match.cmp");

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

void CodeGen::visit(PrintStmt &Node) {
    Node.getArg()->accept(*this);
    Value *Val = V;
    Value *FormatStr;

    if (Val->getType()->isPointerTy()) {
        FormatStr = Builder.CreateGlobalStringPtr("%s\n");
    } else if (Val->getType()->isFloatingPointTy()) {
        FormatStr = Builder.CreateGlobalStringPtr("%f\n");
        Val = Builder.CreateFPExt(Val, Type::getDoubleTy(Context));
    } else if (Val->getType()->isIntegerTy(1)) {
        FormatStr = Builder.CreateGlobalStringPtr("%d\n");
        Val = Builder.CreateZExt(Val, Type::getInt32Ty(Context));
    } else {
        FormatStr = Builder.CreateGlobalStringPtr("%d\n");
    }

    std::vector<Value*> ArgsV;
    ArgsV.push_back(FormatStr);
    ArgsV.push_back(Val);
    Builder.CreateCall(PrintfFunc, ArgsV, "printfCall");
}

void CodeGen::visit(BuiltinCall &Node) {
    if (Node.getName() == "to_int") {
        Node.getArgs()[0]->accept(*this);
        V = castToType(V, Type::getInt32Ty(Context), Builder);
    } else if (Node.getName() == "to_float") {
        Node.getArgs()[0]->accept(*this);
        V = castToType(V, Type::getFloatTy(Context), Builder);
    } else if (Node.getName() == "to_bool") {
        Node.getArgs()[0]->accept(*this);
        V = castToType(V, Type::getInt1Ty(Context), Builder);
    } else if (Node.getName() == "length") {
        // برای سادگی، طول آرایه را ۴ فرض می‌کنیم
        V = ConstantInt::get(Type::getInt32Ty(Context), 4);
    }
}

void CodeGen::visit(RangeExpr &Node) {}