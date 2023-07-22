//
// Created by Daniel on 13.06.2022.
//

#include "Tree.hpp"
#include <sstream>


llvm::Type * Integer::getLLVMType(shared_ptr <llvm::IRBuilder<>> builder) {
    return llvm::Type::getInt32Ty(builder->getContext());
}

llvm::Constant * Integer::getInitConstant(shared_ptr <llvm::IRBuilder<>> builder) {
    return llvm::ConstantInt::get(getLLVMType(builder), 0, true);
}

Array::Array(int minIndex, int maxIndex, shared_ptr <Type> type) : minIndex(minIndex), maxIndex(maxIndex), type(type) {}

llvm::Type * Array::getLLVMType(shared_ptr <llvm::IRBuilder<>> builder) {
    return llvm::ArrayType::get(type->getLLVMType(builder), maxIndex - minIndex + 1);
}

llvm::Constant * Array::getInitConstant(shared_ptr <llvm::IRBuilder<>> builder) {
    return llvm::ConstantArray::get((llvm::ArrayType *) getLLVMType(builder), this->type->getInitConstant(builder));
}

int Array::getMinIndex() const {
    return minIndex;
}

int Array::getMaxIndex() const {
    return maxIndex;
}

Number::Number(int value) : value(value) {}

int Number::getValue() const {
    return value;
}

llvm::Value * Number::getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder->getContext()), value, true);
}

void Number::neg() {
    value = -value;
}

String::String(string value) : value(value) {}

string String::getValue() const {
    return value;
}

llvm::Value * String::getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    return llvm::ConstantDataArray::getString(builder->getContext(), value);
}

Block::Block(vector <shared_ptr<Statement>> statements) : statements(statements) {}

void Block::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    for (auto & statement: statements) {
        statement->translateToLLVM(module, builder);
    }
}

Var::Var(string name, shared_ptr <Type> type, bool global) : name(name), type(type), global(global) {}

shared_ptr <Type> Var::getType() const {
    return type;
}

string Var::getName() const {
    return name;
}

void Var::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    if (!global) {
        llvm::AllocaInst * alloca = builder->CreateAlloca(type->getLLVMType(builder), NULL, name);
        NamedVars[name] = alloca;
    } else {
        module->getOrInsertGlobal(name, type->getLLVMType(builder));
        llvm::GlobalVariable * gVar = module->getNamedGlobal(name);
        gVar->setLinkage(llvm::GlobalValue::CommonLinkage);
        gVar->setInitializer(type->getInitConstant(builder));
        NamedVars[name] = gVar;
    }
    if (type->getLLVMType(builder)->isArrayTy()) {
        shared_ptr <Array> array = static_pointer_cast<Array>(type);
        arrayBounds[name] = {array->getMinIndex(), array->getMaxIndex()};
    }
}

Const::Const(string name, int value) : name(name), value(value) {}

void Const::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    NamedConsts[name] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder->getContext()), value, false);
}

Special::Special(Token token) : token(token) {}

void Special::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    switch (token) {
        case tok_exit: {
            exited = true;
            llvm::Function * F = builder->GetInsertBlock()->getParent();
            if (F->getReturnType() != llvm::Type::getVoidTy(builder->getContext())) {
                string name = builder->GetInsertBlock()->getParent()->getName().str();
                builder->CreateRet(builder->CreateLoad(NamedVars[name]));
            } else
                builder->CreateRetVoid();
            break;
        }
        case tok_break: {
            breaked = true;
            builder->CreateBr(whereBreak);
            break;
        }
        case tok_continue: {
            breaked = true;
            builder->CreateBr(whereContinue);
            break;
        }
        default:
            throw UnknownTokenException(token, {tok_exit, tok_break, tok_continue}, "special keyword");
    }
}

BinOp::BinOp(int token, shared_ptr <Expression> left, shared_ptr <Expression> right) : token(token), left(left),
                                                                                       right(right) {}

llvm::Value * BinOp::getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    llvm::Value * result;
    switch (token) {
        case tok_equal:
            result = builder->CreateICmpEQ(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_notequal:
            result = builder->CreateICmpNE(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_less:
            result = builder->CreateICmpSLT(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_lessequal:
            result = builder->CreateICmpSLE(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_greater:
            result = builder->CreateICmpSGT(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_greaterequal:
            result = builder->CreateICmpSGE(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_plus:
            result = builder->CreateAdd(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_minus:
            result = builder->CreateSub(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_or:
            result = builder->CreateOr(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_multiply:
            result = builder->CreateMul(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_div:
            result = builder->CreateSDiv(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_mod:
            result = builder->CreateSRem(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_and:
            result = builder->CreateAnd(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        case tok_xor:
            result = builder->CreateXor(left->getLLVMValue(module, builder), right->getLLVMValue(module, builder));
            break;
        default:
            throw UnknownTokenException(static_cast<Token>(token),
                                        {tok_equal, tok_notequal, tok_less, tok_lessequal, tok_greater,
                                         tok_greaterequal, tok_plus, tok_minus, tok_or, tok_multiply, tok_div, tok_mod,
                                         tok_and, tok_xor}, "operator");
    }
//cast to 32 bit int
    return builder->CreateIntCast(result, llvm::Type::getInt32Ty(builder->getContext()), false);
}

UnOp::UnOp(int token, shared_ptr <Expression> expr) : token(token), expr(expr) {}

llvm::Value * UnOp::getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    llvm::Value * result;
    switch (token) {
        case tok_minus:
            result = builder->CreateNeg(expr->getLLVMValue(module, builder));
            break;
        case tok_not:
            result = builder->CreateNot(expr->getLLVMValue(module, builder));
            break;
        default:
            throw UnknownTokenException(static_cast<Token>(token), {tok_minus, tok_not}, "operator");
    }
//cast to 32 bit int
    return builder->CreateIntCast(result, llvm::Type::getInt32Ty(builder->getContext()), false);
}

Reference::Reference(const string name) : name(name) {}

string Reference::getName() const {
    return name;
}

VarReference::VarReference(const string name) : Reference(name) {}

llvm::Value * VarReference::getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    if (NamedVars.find(name) == NamedVars.end()) {
        if (NamedConsts.find(name) != NamedConsts.end())
            return NamedConsts[name];
        throw UnknownVarException(name);
    }
    return builder->CreateLoad(NamedVars[name]);
}

llvm::Value * VarReference::getLLVMAddress(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {

    if (NamedVars.find(name) == NamedVars.end()) {
        if (NamedConsts.find(name) != NamedConsts.end())
            return NamedConsts[name];
        throw UnknownVarException(name);
    }
    return NamedVars[name];
}

ArrayItemReference::ArrayItemReference(shared_ptr <Reference> var, shared_ptr <Expression> index) : var(var),
                                                                                                    index(index) {}

llvm::Value *
ArrayItemReference::getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    return builder->CreateLoad(getLLVMAddress(module, builder));
}

llvm::Value *
ArrayItemReference::getLLVMAddress(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    llvm::Value * idx = builder->CreateSub(index->getLLVMValue(module, builder),
                                           llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder->getContext()),
                                                                  arrayBounds[var->getName()].first, true));
    return builder->CreateGEP(var->getLLVMAddress(module, builder),
                              {llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder->getContext()), 0, true), idx});
}

Assign::Assign(shared_ptr <Reference> left, shared_ptr <Expression> right) : left(left), right(right) {}

void Assign::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    builder->CreateStore(right->getLLVMValue(module, builder), left->getLLVMAddress(module, builder));
}


For::For(const string varName, shared_ptr <Block> block, shared_ptr <Expression> startExpr,
         shared_ptr <Expression> endExpr, const bool ascending) : varName(varName), block(block), startExpr(startExpr),
                                                                  endExpr(endExpr), ascending(ascending) {}


void For::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {

    llvm::BasicBlock * oldBreakPoint = whereBreak;
    llvm::BasicBlock * oldContinuePoint = whereContinue;

    llvm::Function * TheFunction = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock * HeaderBB = llvm::BasicBlock::Create(builder->getContext(), "for_header", TheFunction);

//JUMP TO INIT
    builder->CreateBr(HeaderBB);

//INITIALIZE VARIABLE
    builder->SetInsertPoint(HeaderBB);
//shadowing variable
    llvm::Value * OldVal = NamedVars[varName];

    shared_ptr <Expression> stepVal = make_shared<Number>(ascending ? 1 : -1);
    shared_ptr <Var> stepVarDecl = make_shared<Var>(varName, make_shared<Integer>(), false);
    stepVarDecl->translateToLLVM(module, builder);


    auto stepVar = make_shared<VarReference>(varName);
    auto stepVarLLVMAddress = stepVar->getLLVMAddress(module, builder);

    builder->CreateStore(startExpr->getLLVMValue(module, builder), stepVar->getLLVMAddress(module, builder));

//checkcond
    llvm::BasicBlock * CondCheckBB = llvm::BasicBlock::Create(builder->getContext(), "for_condcheck", TheFunction);
    llvm::BasicBlock * BodyBB = llvm::BasicBlock::Create(builder->getContext(), "for_body", TheFunction);
    llvm::BasicBlock * AfterBB = llvm::BasicBlock::Create(builder->getContext(), "for_after", TheFunction);
    builder->CreateBr(CondCheckBB);
    builder->SetInsertPoint(CondCheckBB);

// CHECK CONDITION BEFORE ENTERING LOOP
    auto condition = make_shared<BinOp>(ascending ? tok_lessequal : tok_greaterequal, stepVar, endExpr);
    llvm::Value * EndCond = builder->CreateICmpNE(condition->getLLVMValue(module, builder),
                                                  Number(0).getLLVMValue(module, builder), "for_cond");
    llvm::BasicBlock * NextVarBB = llvm::BasicBlock::Create(builder->getContext(), "for_nextvar", TheFunction);
    builder->CreateCondBr(EndCond, BodyBB, AfterBB);

    whereBreak = AfterBB;
    whereContinue = NextVarBB;



// LOOP BODY
    builder->SetInsertPoint(BodyBB);
    block->translateToLLVM(module, builder);




//exit
    if (!exited && !breaked) {
        builder->CreateBr(NextVarBB);
    }

// INCREASE VAR
    builder->SetInsertPoint(NextVarBB);
    llvm::Value * nextVar = builder->CreateAdd(stepVar->getLLVMValue(module, builder),
                                               stepVal->getLLVMValue(module, builder));

    builder->CreateStore(nextVar, stepVarLLVMAddress);
    builder->CreateBr(CondCheckBB);

// AFTER LOOP
    builder->SetInsertPoint(AfterBB);

// Restore the unshadowed variable.
    if (OldVal)
        NamedVars[varName] = OldVal;
    else
        NamedVars.erase(varName);
    exited = false;
    breaked = false;


    whereBreak = oldBreakPoint;
    whereContinue = oldContinuePoint;
}

While::While(shared_ptr <Block> block, shared_ptr <Expression> condition) : block(block), condition(condition) {}

void While::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    llvm::BasicBlock * oldBreakPoint = whereBreak;
    llvm::BasicBlock * oldContinuePoint = whereContinue;
    llvm::Function * TheFunction = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock * CondCheckBB = llvm::BasicBlock::Create(builder->getContext(), "while_condcheck", TheFunction);
    llvm::BasicBlock * BodyBB = llvm::BasicBlock::Create(builder->getContext(), "while_body", TheFunction);

//INITIALIZE BREAK AND CONTINUE JUMP DESTINATIONS
    llvm::BasicBlock * AfterBB = llvm::BasicBlock::Create(builder->getContext(), "while_after", TheFunction);
    whereBreak = AfterBB;
    whereContinue = CondCheckBB;

    builder->CreateBr(CondCheckBB);
//CHECK CONDITION
    builder->SetInsertPoint(CondCheckBB);
    llvm::Value * EndCond = builder->CreateICmpNE(condition->getLLVMValue(module, builder),
                                                  Number(0).getLLVMValue(module, builder), "while_cond");
    builder->CreateCondBr(EndCond, BodyBB, AfterBB);

// LOOP BODY
    builder->SetInsertPoint(BodyBB);
    block->translateToLLVM(module, builder);

// IF EXIT, BREAK OR CONTINUE DON'T CREATE JUMP
    if (!exited && !breaked) {
        builder->CreateBr(CondCheckBB);
    }

// AFTER LOOP
    builder->SetInsertPoint(AfterBB);
    breaked = false;
    exited = false;
    whereBreak = oldBreakPoint;
    whereContinue = oldContinuePoint;
}


If::If(shared_ptr <Block> ifBlock, shared_ptr <Block> elseBlock, shared_ptr <Expression> condition) : ifBlock(ifBlock),
                                                                                                      elseBlock(
                                                                                                              elseBlock),
                                                                                                      condition(
                                                                                                              condition) {}

void If::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
// Convert condition to a bool by comparing non-equal to 0.0.
    llvm::Value * cond = builder->CreateICmpNE(condition->getLLVMValue(module, builder),
                                               Number(0).getLLVMValue(module, builder), "if_cond");
    llvm::Function * TheFunction = builder->GetInsertBlock()->getParent();

// Create blocks for the then and else cases.  Insert the 'then' block at the
// end of the function.
    llvm::BasicBlock * IfBB = llvm::BasicBlock::Create(builder->getContext(), "if", TheFunction);
    llvm::BasicBlock * ElseBB = llvm::BasicBlock::Create(builder->getContext(), "else");
    llvm::BasicBlock * MergeBB = llvm::BasicBlock::Create(builder->getContext(), "if_after");

    builder->CreateCondBr(cond, IfBB, ElseBB);

// Emit then value.

    builder->SetInsertPoint(IfBB);
    ifBlock->translateToLLVM(module, builder);
    if (!exited && !breaked)
        builder->CreateBr(MergeBB);

    exited = false;
    breaked = false;
// Emit else block.
    TheFunction->getBasicBlockList().push_back(ElseBB);
    builder->SetInsertPoint(ElseBB);
    if (elseBlock != nullptr) {
        elseBlock->translateToLLVM(module, builder);
    }
    if (!exited && !breaked)
        builder->CreateBr(MergeBB);
    exited = false;
    breaked = false;
// Emit merge block.
    TheFunction->getBasicBlockList().push_back(MergeBB);
    builder->SetInsertPoint(MergeBB);
}

FunctionCall::FunctionCall(string name, vector <shared_ptr<Expression>> params) : name(name), params(params) {}


llvm::Value * FunctionCall::getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    llvm::Value * result = nullptr;


    if ((name == "write" || name == "writeln") && params.size() == 1) {
        if (!strFormat || !strFormatNl) {
            strFormat = builder->CreateGlobalStringPtr("%s", "&strFormat");
            strFormatNl = builder->CreateGlobalStringPtr("%s\n", "&strFormatNl");
        }
        if (params[0]->getLLVMValue(module, builder)->getType() == llvm::Type::getInt32Ty(builder->getContext())) {
            auto var = params[0]->getLLVMValue(module, builder);
            result = builder->CreateCall(module->getFunction(name), {var});
        } else {
            auto var = builder->CreateGlobalStringPtr(((String *) params[0].get())->getValue(), "&globalStr");
            result = builder->CreateCall(module->getFunction("printf"),
                                         {name == "write" ? strFormat : strFormatNl, var});
        }
    } else if (name == "dec" && params.size()) {
        llvm::Value * paramAddress = ((Reference *) params[0].get())->getLLVMAddress(module, builder);
        result = builder->CreateStore(
                builder->CreateSub(params[0]->getLLVMValue(module, builder), Number(1).getLLVMValue(module, builder)),
                paramAddress);
    } else {
        auto F = module->getFunction(name);
        if (!F)
            throw invalid_argument("Call to unknown function \"" + name + "\"\n");
        vector < llvm::Value * > LLVMParams;
        int i = 0;
        if (params.size() != F->arg_size())
            throw invalid_argument("Call to function \"" + name + "\" with wrong number of parameters. Got " + to_string(params.size()) + " expected " + to_string(F->arg_size()) + "\n");

        for (auto & x: F->args()) {
            auto param = params[i++];
            auto ptr = dynamic_pointer_cast<Reference>(param);
//is pointer and function expects pointer
            if (ptr && x.getType()->isPointerTy()) {
                LLVMParams.push_back(ptr->getLLVMAddress(module, builder));
            } else
                LLVMParams.push_back(param->getLLVMValue(module, builder));
        }
        result = builder->CreateCall(module->getFunction(name), LLVMParams);
    }
    exited = false;
    return result;
}

ProcedureCall::ProcedureCall(string name, vector <shared_ptr<Expression>> params) : name(name), params(params) {}

void ProcedureCall::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    FunctionCall(name, params).getLLVMValue(module, builder);
}

Function::Function(string name, vector <shared_ptr<Var>> params, shared_ptr <Type> returnType, shared_ptr <Block> block,
                   vector <shared_ptr<Var>> localVars) : name(name), params(params), returnType(returnType),
                                                         block(block), localVars(localVars) {}

void Function::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    if (!module->getFunction(name))
        initFunction(module, builder);
    if (block != nullptr) {
        llvm::Function * F = module->getFunction(name);
        auto oldInsert = builder->GetInsertBlock();
        builder->SetInsertPoint(&F->getEntryBlock());
        block->translateToLLVM(module, builder);
        builder->CreateRet(builder->CreateLoad(NamedVars[name]));
        builder->SetInsertPoint(oldInsert);
    }
}

void Function::initFunction(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    vector < llvm::Type * > llvmParams;
    for (auto & x: params) {
        llvmParams.push_back(x->getType()->getLLVMType(builder));
    }
    llvm::FunctionType * FT = llvm::FunctionType::get(returnType->getLLVMType(builder), llvmParams, false);
    llvm::Function * F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, module.get());

    int idx = 0;
    for (auto & arg: F->args()) {
        arg.setName(this->params[idx++]->getName());
    }

    llvm::BasicBlock * BB = llvm::BasicBlock::Create(builder->getContext(), name, F);
    builder->SetInsertPoint(BB);
    NamedVars[name] = builder->CreateAlloca(returnType->getLLVMType(builder),
                                            llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder->getContext()), 0,
                                                                   false), name);

//initialize variables
    int i = 0;
    for (auto & x: F->args()) {
        auto param = params[i++];
        param->translateToLLVM(module, builder);
        auto varRef = make_shared<VarReference>(param->getName());
        builder->CreateStore(&x, varRef->getLLVMAddress(module, builder));
    }
//create local vars
    for (auto & var: localVars)
        var->translateToLLVM(module, builder);
}

Procedure::Procedure(string name, vector <shared_ptr<Var>> params, shared_ptr <Block> block,
                     vector <shared_ptr<Var>> localVars) : name(name), params(params), block(block),
                                                           localVars(localVars) {}

void Procedure::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    if (!module->getFunction(name))
        initProcedure(module, builder);
    if (block != nullptr) {
        llvm::Function * F = module->getFunction(name);
        auto oldInsert = builder->GetInsertBlock();
        builder->SetInsertPoint(&F->getEntryBlock());
        block->translateToLLVM(module, builder);
        builder->CreateRetVoid();
        builder->SetInsertPoint(oldInsert);
    }
}

void Procedure::initProcedure(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    vector < llvm::Type * > llvmParams;
    for (auto & x: params) {
        llvmParams.push_back(x->getType()->getLLVMType(builder));
    }
    llvm::FunctionType * FT = llvm::FunctionType::get(llvm::Type::getVoidTy(builder->getContext()), llvmParams, false);
    llvm::Function * F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, module.get());
    int idx = 0;
    for (auto & arg: F->args()) {
        arg.setName(this->params[idx++]->getName());
    }

    llvm::BasicBlock * BB = llvm::BasicBlock::Create(builder->getContext(), name, F);
    builder->SetInsertPoint(BB);

//initialize variables
    int i = 0;
    for (auto & x: F->args()) {
        auto param = params[i];
        param->translateToLLVM(module, builder);
        auto varRef = make_shared<VarReference>(param->getName());
        builder->CreateStore(&x, varRef->getLLVMAddress(module, builder));
        i++;
    }
//create local vars
    for (auto & var: localVars)
        var->translateToLLVM(module, builder);

}

void Program::initFunctions(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    {
        std::vector<llvm::Type *> Ints(1, llvm::Type::getInt32Ty(builder->getContext()));
        llvm::FunctionType * FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(builder->getContext()), Ints, true);
        {   //write
            llvm::Function * F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "write", module.get());
            for (auto & Arg: F->args())
                Arg.setName("x");
        }
        {   //writeln
            llvm::Function * F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeln", module.get());
            for (auto & Arg: F->args())
                Arg.setName("x");
        }
    }
    {   //printf
        std::vector<llvm::Type *> IntPtrs(2, llvm::Type::getInt8PtrTy(builder->getContext()));
        llvm::FunctionType * FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(builder->getContext()), IntPtrs,
                                                          false);
        llvm::Function * F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "printf", module.get());
        for (auto & Arg: F->args())
            Arg.setName("x");
        F->setCallingConv(llvm::CallingConv::C);
    }
    {   //readln
        std::vector<llvm::Type *> IntPtrs(1, llvm::Type::getInt32PtrTy(builder->getContext()));
        llvm::FunctionType * FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(builder->getContext()), IntPtrs,
                                                          false);
        llvm::Function * F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readln", module.get());
        for (auto & Arg: F->args())
            Arg.setName("x");
    }
}

void Program::translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) {
    initFunctions(module, builder);
}