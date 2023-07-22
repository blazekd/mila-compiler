//
// Created by Daniel on 13.06.2022.
//

#ifndef MILA_TREE_HPP
#define MILA_TREE_HPP

#include "Lexer.hpp"
#include <llvm/IR/Value.h>
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h"
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm-10/llvm/IR/IRBuilder.h>
#include <llvm-10/llvm/IR/BasicBlock.h>
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ADT/IndexedMap.h"

static map<string, llvm::Value *> NamedConsts;
static map<string, llvm::Value *> NamedVars;
static map <string, pair<int, int>> arrayBounds;
static bool exited = false;
static bool breaked = false;
static llvm::BasicBlock * whereBreak = nullptr;
static llvm::BasicBlock * whereContinue = nullptr;
static llvm::Value * strFormat = nullptr;
static llvm::Value * strFormatNl = nullptr;

class UnknownVarException : public exception {
    string varName;
public:
    UnknownVarException(string varName);

    const char * what() const noexcept override;
};

class UnknownTokenException : public exception {
    Token got;
    vector <Token> expected;
    string type;
public:
    UnknownTokenException(Token got, vector <Token> expected, string type);

    const char * what() const noexcept override;
};

class Type {
public:
    virtual llvm::Type * getLLVMType(shared_ptr <llvm::IRBuilder<>> builder) = 0;

    virtual llvm::Constant * getInitConstant(shared_ptr <llvm::IRBuilder<>> builder) = 0;
};

class Integer : public Type {
public:
    llvm::Type * getLLVMType(shared_ptr <llvm::IRBuilder<>> builder) override;

    llvm::Constant * getInitConstant(shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Array : public Type {
    int minIndex;
    int maxIndex;
    shared_ptr <Type> type;
public:
    Array(int minIndex, int maxIndex, shared_ptr <Type> type);

    llvm::Type * getLLVMType(shared_ptr <llvm::IRBuilder<>> builder) override;

    llvm::Constant * getInitConstant(shared_ptr <llvm::IRBuilder<>> builder) override;

    int getMinIndex() const;

    int getMaxIndex() const;
};

class Node {
public:

};

class Statement : public Node {
public:
    virtual void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) = 0;
};

class Expression : public Node {
public:
    virtual llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) = 0;
};

class Number : public Expression {
    int value;
public:
    Number(int value);

    int getValue() const;

    llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;

    void neg();
};

class String : public Expression {
    const string value;
public:
    String(string value);

    string getValue() const;

    llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Block : public Statement {
    vector <shared_ptr<Statement>> statements;
public:
    Block(vector <shared_ptr<Statement>> statements);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Var : public Statement {
    string name;
    shared_ptr <Type> type;
    bool global;
public:
    Var(string name, shared_ptr <Type> type, bool global);

    shared_ptr <Type> getType() const;

    string getName() const;

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Const : public Statement {
    const string name;
    const int value;
public:
    Const(string name, int value);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Special : public Statement {
    Token token;
public:
    Special(Token token);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class BinOp : public Expression {
    int token;
    shared_ptr <Expression> left;
    shared_ptr <Expression> right;
public:
    BinOp(int token, shared_ptr <Expression> left, shared_ptr <Expression> right);

    llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class UnOp : public Expression {
    int token;
    shared_ptr <Expression> expr;
public:
    UnOp(int token, shared_ptr <Expression> expr);

    llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Reference : public Expression {
protected:
    const string name;
public:
    Reference() = default;

    Reference(const string name);

    string getName() const;

    virtual llvm::Value * getLLVMAddress(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) = 0;
};

class VarReference : public Reference {
public:
    VarReference(const string name);

    llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;

    llvm::Value * getLLVMAddress(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class ArrayItemReference : public Reference {
    shared_ptr <Reference> var;
    shared_ptr <Expression> index;
public:
    ArrayItemReference(shared_ptr <Reference> var, shared_ptr <Expression> index);

    llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;

    llvm::Value * getLLVMAddress(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Assign : public Statement {
    shared_ptr <Reference> left;
    shared_ptr <Expression> right;
public:
    Assign(shared_ptr <Reference> left, shared_ptr <Expression> right);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class For : public Statement {
    const string varName;
    shared_ptr <Block> block;
    shared_ptr <Expression> startExpr;
    shared_ptr <Expression> endExpr;
    const bool ascending;
public:
    For(const string varName, shared_ptr <Block> block, shared_ptr <Expression> startExpr,
        shared_ptr <Expression> endExpr, const bool ascending);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class While : public Statement {
    shared_ptr <Block> block;
    shared_ptr <Expression> condition;
public:
    While(shared_ptr <Block> block, shared_ptr <Expression> condition);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class If : public Statement {
    shared_ptr <Block> ifBlock;
    shared_ptr <Block> elseBlock;
    shared_ptr <Expression> condition;
public:
    If(shared_ptr <Block> ifBlock, shared_ptr <Block> elseBlock, shared_ptr <Expression> condition);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class FunctionCall : public Expression {
    string name;
    vector <shared_ptr<Expression>> params;
public:
    FunctionCall(string name, vector <shared_ptr<Expression>> params);

    llvm::Value * getLLVMValue(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class ProcedureCall : public Statement {
    string name;
    vector <shared_ptr<Expression>> params;
public:
    ProcedureCall(string name, vector <shared_ptr<Expression>> params);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

class Function : public Statement {
    string name;
    vector <shared_ptr<Var>> params;
    shared_ptr <Type> returnType;
    shared_ptr <Block> block;
    vector <shared_ptr<Var>> localVars;
public:
    Function(string name, vector <shared_ptr<Var>> params, shared_ptr <Type> returnType, shared_ptr <Block> block,
             vector <shared_ptr<Var>> localVars);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;

    void initFunction(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder);
};

class Procedure : public Statement {
    string name;
    vector <shared_ptr<Var>> params;
    shared_ptr <Block> block;
    vector <shared_ptr<Var>> localVars;
public:
    Procedure(string name, vector <shared_ptr<Var>> params, shared_ptr <Block> block,
              vector <shared_ptr<Var>> localVars);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;

    void initProcedure(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder);
};

class Program : public Statement {
public:
    void initFunctions(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder);

    void translateToLLVM(shared_ptr <llvm::Module> module, shared_ptr <llvm::IRBuilder<>> builder) override;
};

#endif //MILA_TREE_HPP
