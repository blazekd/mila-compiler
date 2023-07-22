#ifndef PJPPROJECT_PARSER_HPP
#define PJPPROJECT_PARSER_HPP

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include "Lexer.hpp"
#include "Tree.hpp"

static unordered_map<Token, string> tokens = {
        {tok_error,            "UNKNOWN TOKEN"},
        {tok_eof,              "EOF"},

        // numbers and identifiers
        {tok_identifier,       "identifier"},
        {tok_number,           "number"},

        // keywords
        {tok_begin,            "begin"},
        {tok_end,              "end"},
        {tok_const,            "const"},
        {tok_procedure,        "procedure"},
        {tok_forward,          "forward"},
        {tok_function,         "function"},
        {tok_if,               "if"},
        {tok_then,             "then"},
        {tok_else,             "else"},
        {tok_program,          "program"},
        {tok_while,            "while"},
        {tok_exit,             "exit"},
        {tok_var,              "var"},
        {tok_integer,          "integer"},
        {tok_for,              "for"},
        {tok_do,               "do"},

        // 2-character operators
        {tok_notequal,         "<>"},
        {tok_lessequal,        "<="},
        {tok_greaterequal,     ">="},
        {tok_assign,           ":="},
        {tok_or,               "or"},

        // 3-character operators (keywords)
        {tok_mod,              "mod"},
        {tok_div,              "div"},
        {tok_not,              "not"},
        {tok_and,              "and"},
        {tok_xor,              "xor"},

        // keywords in for loop
        {tok_to,               "to"},
        {tok_downto,           "downto"},

        // keywords for array
        {tok_array,            "array"},

        // my
        {tok_string,           "string"},
        {tok_leftParenthesis,  "("},
        {tok_rightParenthesis, ")"},
        {tok_leftBracket,      "["},
        {tok_rightBracket,     "]"},
        {tok_dot,              "."},
        {tok_declaration,      ":"},
        {tok_less,             "<"},
        {tok_greater,          ">"},
        {tok_plus,             "+"},
        {tok_minus,            "-"},
        {tok_multiply,         "*"},
        {tok_equal,            "="},
        {tok_comma,            ","},
        {tok_semicolon,        ";"},
        {tok_of,               "of"},
        {tok_break,            "break"},
        {tok_continue,         "continue"}
};


class Parser {
public:
    Parser();

    ~Parser() = default;

    bool Parse();                    // parse
    const llvm::Module & Generate();  // generate
    bool showExpansion = false; // if true, print used expansion rules
    void printExpansion(string s);

private:
    int getNextToken();

    Lexer m_Lexer;                   // lexer is used to read tokens
    int CurTok = 0;                      // to keep the current token
    void match(Token expected);

    void throwParseException(vector <Token> expected);


    //A - program
    void parseProgram();

    //B - global var, function, procedure, const, main declarations
    void parseDecls(vector <shared_ptr<Statement>> & statements);

    //C - local var
    void parseLocalVar(vector <shared_ptr<Var>> & vars);

    //D - statement - line, call, block, for, while, exit, if
    void parseStatement(vector <shared_ptr<Statement>> & statements);

    //D' - remaining part of for
    void parseFor(string varName, shared_ptr <Expression> startExpr, vector <shared_ptr<Statement>> & statements);

    //E - ident and type of var, multiple vars
    void parseVarDecl(vector <shared_ptr<Var>> & vars, bool global);

    //E' - multiple identifier
    void parseMultIdent(vector <string> & names);

    //E'' multiple var declarations
    void parseMultVarDecls(vector <shared_ptr<Var>> & vars, bool global);

    //F - identifier suffix - array, function call
    shared_ptr <Expression> parseIdentSuffix(string name);

    //F' - identifier
    shared_ptr <Expression> parseIdent();

    //F'' - identifier or number
    shared_ptr <Expression> parseIdentOrNumb();

    //F''' - multidimensional array
    shared_ptr <Expression> parseIdentArraySuffix(shared_ptr <Reference> var);

    //G - function parameter
    void parseFuncParam(vector <shared_ptr<Expression>> & params);

    //G' - multiple function parameters
    void parseMultFuncParams(vector <shared_ptr<Expression>> & params);

    //H - type
    shared_ptr <Type> parseType();

    //I - expression - level 1 operand
    shared_ptr <Expression> parseExpression();

    //I' - 1. level operators - < <= <> = => >
    shared_ptr <Expression> parseLevel1Ops(shared_ptr <Expression> expression);

    //J - const declaration
    vector <shared_ptr<Const>> parseConstDecl();

    //J' - multiple const declarations
    void parseMultConstDecls(vector <shared_ptr<Const>> & consts);

    //K - level 2 operand
    shared_ptr <Expression> parseLevel2Op();

    //K' - 2. level operators + - or
    shared_ptr <Expression> parseLevel2Ops(shared_ptr <Expression> expression);

    //L - level 3 operand
    shared_ptr <Expression> parseLevel3Op();

    //L' - 3. level operators * div mod and
    shared_ptr <Expression> parseLevel3Ops(shared_ptr <Expression> expression);

    //M - level 4 operand
    shared_ptr <Expression> parseLevel4Op();

    //N - level 5 operand
    shared_ptr <Expression> parseLevel5Op();

    //O - parse ident line
    shared_ptr <Statement> parseIdentLine();

    //O' - parse after ident - assign, array element, procedure call
    shared_ptr <Statement> parseAfterIdent(string name);

    //O'' - parse array element
    shared_ptr <Statement> parseArrayElement(shared_ptr <ArrayItemReference> var);

    //P - number
    shared_ptr <Number> parseNumber();

    //Q - function/procedure param declaration
    void parseFuncParamDecl(vector <shared_ptr<Var>> & params);

    //Q' - more function/procedure param declarations
    void parseFunctMultParamDecls(vector <shared_ptr<Var>> & params);

    //R - parse statement if there is, else end block with or without ; (last statement can end without ;)
    void parseNextStatement(vector <shared_ptr<Statement>> & statements);

    //R' - if there is next statement force ;
    void parseAddNextStatement(vector <shared_ptr<Statement>> & statements);

    //S - if
    shared_ptr <Statement> parseIf();

    //S' - block or single statement (starting with identifier) = assign, exit, function call
    shared_ptr <Statement> parseIfBlock(shared_ptr <Expression> condition);

    //S'' - else it exists
    shared_ptr <Block> parseElse();

    //S''' - block or single statement (starting with identifier) = assign, exit, function call
    shared_ptr <Block> parseElseBlock();

    //T - function forward
    shared_ptr <Block> parseFunctionForward();

    //U - block
    shared_ptr <Block> parseBlock();

    llvm::LLVMContext MilaContext;   // llvm context
    shared_ptr <llvm::IRBuilder<>> MilaBuilder;   // llvm builder
    shared_ptr <llvm::Module> MilaModule;         // llvm module
};

#endif //PJPPROJECT_PARSER_HPP
