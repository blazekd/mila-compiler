#include "Parser.hpp"

#include <memory>
#include <sstream>

void Parser::printExpansion(string s) {
    if (showExpansion)
        cout << s << endl;
}

Parser::Parser() :
        MilaContext(),
        MilaBuilder(make_unique<llvm::IRBuilder<>>(MilaContext)),
        MilaModule(make_unique<llvm::Module>("mila", MilaContext)) {}


UnknownVarException::UnknownVarException(string varName) : varName(varName) {}

const char * UnknownVarException::what() const noexcept {
    stringstream ss;
    ss << "Unknown var: \"";
    ss << varName;
    ss << "\"";
    string * res = new string;
    *res = ss.str();
    return res->c_str();
}

UnknownTokenException::UnknownTokenException(Token got, vector <Token> expected, string type) : got(got),
                                                                                                expected(expected),
                                                                                                type(type) {}

const char * UnknownTokenException::what() const noexcept {
    stringstream ss;
    ss << "Unexpected ";
    ss << type;
    ss << "\nGot: \"";
    ss << tokens[got];
    ss << "\"\nExpected: ";
    for (uint32_t i = 0; i < expected.size(); ++i) {
        ss << "\"";
        ss << tokens[expected[i]];
        ss << "\"";
        if (i != expected.size() - 1)
            ss << ", ";
    }
    string * res = new string;
    *res = ss.str();
    return res->c_str();
}

bool Parser::Parse() {
    getNextToken();
    return true;
}

const llvm::Module & Parser::Generate() {
    //parser grammar starting symbol
    parseProgram();

    return *MilaModule;
}

/**
 * @brief Simple token buffer.
 *
 * CurTok is the current token the parser is looking at
 * getNextToken reads another token from the lexer and updates curTok with ts result
 * Every function in the parser will assume that CurTok is the cureent token that needs to be parsed
 */
int Parser::getNextToken() {
    return CurTok = m_Lexer.gettok();
}

void Parser::match(Token expected) {
    if (CurTok != expected)
        throwParseException({expected});
    getNextToken();
}

void Parser::throwParseException(vector <Token> expected) {
    throw UnknownTokenException(static_cast<Token>(CurTok), expected, "token");
}

void Parser::parseProgram() {
    printExpansion("1) A -> program ident ; B");
    match(tok_program);
    match(tok_identifier);
    match(tok_semicolon);
    vector <shared_ptr<Statement>> statements;
    statements.push_back(make_shared<Program>());
    parseDecls(statements);
    for (auto & statement: statements) {
        statement->translateToLLVM(MilaModule, MilaBuilder);
    }
}

void Parser::parseDecls(vector <shared_ptr<Statement>> & statements) {
    switch (CurTok) {
        case tok_var: {
            printExpansion("2) B -> var E B");
            match(tok_var);
            vector <shared_ptr<Var>> vars;
            parseVarDecl(vars, true);
            for (auto & var: vars) {
                statements.push_back(var);
            }
            parseDecls(statements);
            break;
        }
        case tok_begin: {
            printExpansion("3) B -> U .");
            vector <shared_ptr<Var>> params;
            vector <shared_ptr<Var>> vars;
            auto main = make_shared<Function>("main", params, shared_ptr<Type>(new Integer()), parseBlock(), vars);
            match(tok_dot);
            statements.push_back(main);
            break;
        }
        case tok_function: {
            printExpansion("4) B -> function ident ( Q ) : H ; C T ; B");
            match(tok_function);
            string name = m_Lexer.identifierStr();
            match(tok_identifier);
            match(tok_leftParenthesis);
            vector <shared_ptr<Var>> params;
            parseFuncParamDecl(params);
            match(tok_rightParenthesis);
            match(tok_declaration);
            auto type = parseType();
            match(tok_semicolon);
            vector <shared_ptr<Var>> vars;
            parseLocalVar(vars);
            auto block = parseFunctionForward();
            match(tok_semicolon);
            statements.push_back(make_shared<Function>(name, params, type, block, vars));
            parseDecls(statements);
            break;
        }
        case tok_procedure: {
            printExpansion("5) B -> procedure ident ( Q ) ; C T ; B");
            match(tok_procedure);
            string name = m_Lexer.identifierStr();
            match(tok_identifier);
            match(tok_leftParenthesis);
            vector <shared_ptr<Var>> params;
            parseFuncParamDecl(params);
            match(tok_rightParenthesis);
            match(tok_semicolon);
            vector <shared_ptr<Var>> vars;
            parseLocalVar(vars);
            auto block = parseFunctionForward();
            match(tok_semicolon);
            statements.push_back(make_shared<Procedure>(name, params, block, vars));
            parseDecls(statements);
            break;
        }
        case tok_const: {
            printExpansion("6) B -> const J B");
            match(tok_const);
            auto consts = parseConstDecl();
            for (auto & c: consts) {
                statements.push_back(c);
            }
            parseDecls(statements);
            break;
        }
        default:
            printExpansion("B exception");
            throwParseException({tok_var, tok_begin, tok_function, tok_procedure, tok_const});
    }
}

void Parser::parseLocalVar(vector <shared_ptr<Var>> & vars) {
    switch (CurTok) {
        case tok_var: {
            printExpansion("7) C -> var E C");
            match(tok_var);
            parseVarDecl(vars, false);
            parseLocalVar(vars);
            break;
        }
        default:
            printExpansion("8) C -> ε");
            return;
    }
}

void Parser::parseStatement(vector <shared_ptr<Statement>> & statements) {
    switch (CurTok) {
        case tok_identifier: {
            printExpansion("9) D -> O R");
            auto statement = parseIdentLine();
            statements.push_back(statement);
            parseNextStatement(statements);
            break;
        }
        case tok_begin:
            printExpansion("10) D -> U R");
            parseBlock();
            parseNextStatement(statements);
            break;
        case tok_for: {
            printExpansion("11) D -> for ident := I D'");
            match(tok_for);
            const string varName = m_Lexer.identifierStr();
            match(tok_identifier);
            match(tok_assign);
            auto startExpr = parseExpression();
            parseFor(varName, startExpr, statements);
            break;
        }
        case tok_while: {
            printExpansion("12) D -> while I do U R");
            match(tok_while);
            auto condition = parseExpression();
            match(tok_do);
            auto block = parseBlock();
            statements.push_back(make_shared<While>(block, condition));
            parseNextStatement(statements);
            break;
        }
        case tok_exit:
            printExpansion("13) D -> exit R");
            match(tok_exit);
            statements.push_back(make_shared<Special>(tok_exit));
            parseNextStatement(statements);
            break;
        case tok_break:
            printExpansion("14) D -> break R");
            match(tok_break);
            statements.push_back(make_shared<Special>(tok_break));
            parseNextStatement(statements);
            break;
        case tok_continue:
            printExpansion("15) D -> continue R");
            match(tok_continue);
            statements.push_back(make_shared<Special>(tok_continue));
            parseNextStatement(statements);
            break;
        case tok_if:
            printExpansion("16) D -> S R");
            statements.push_back(parseIf());
            parseNextStatement(statements);
            break;
        default:
            printExpansion("D exception");
            throwParseException(
                    {tok_identifier, tok_begin, tok_for, tok_while, tok_exit, tok_break, tok_if, tok_continue});

    }
}

void
Parser::parseFor(const string varName, shared_ptr<Expression> startExpr, vector <shared_ptr<Statement>> & statements) {
    bool ascending = true;
    switch (CurTok) {
        case tok_to: {
            printExpansion("17) D' -> to I do U R");
            match(tok_to);
            break;
        }
        case tok_downto:
            printExpansion("18) D' -> downto I do U R");
            match(tok_downto);
            ascending = false;
            break;
        default:
            printExpansion("D' exception");
            throwParseException({tok_to, tok_downto});
    }
    auto endExpr = parseExpression();
    match(tok_do);
    auto block = parseBlock();
    statements.push_back(make_shared<For>(varName, block, startExpr, endExpr, ascending));
    parseNextStatement(statements);
}

void Parser::parseVarDecl(vector <shared_ptr<Var>> & vars, const bool global) {
    printExpansion("19) E -> ident E' : H ; E''");
    vector <string> names;
    names.push_back(m_Lexer.identifierStr());
    match(tok_identifier);
    parseMultIdent(names);
    match(tok_declaration);
    auto type = parseType();
    for (auto & name: names) {
        vars.push_back(make_shared<Var>(name, type, global));
    }

    match(tok_semicolon);
    parseMultVarDecls(vars, global);
}

void Parser::parseMultIdent(vector <string> & names) {
    switch (CurTok) {
        case tok_comma:
            printExpansion("20) E' -> , ident E'");
            match(tok_comma);
            names.push_back(m_Lexer.identifierStr());
            match(tok_identifier);
            parseMultIdent(names);
            break;
        default:
            printExpansion("21) E' -> ε");
            return;
    }
}


void Parser::parseMultVarDecls(vector <shared_ptr<Var>> & vars, const bool global) {
    switch (CurTok) {
        case tok_identifier:
            printExpansion("22) E'' -> E");
            parseVarDecl(vars, global);
            break;
        default:
            printExpansion("23) E'' -> ε");
            return;
    }
}


shared_ptr<Expression> Parser::parseIdentSuffix(const string name) {
    switch (CurTok) {
        case tok_leftBracket: {
            printExpansion("24) F -> [ I ] F'''");
            match(tok_leftBracket);
            auto expression = parseExpression();
            match(tok_rightBracket);
            //multidim array
            return parseIdentArraySuffix(make_shared<ArrayItemReference>(make_shared<VarReference>(name), expression));
        }
        case tok_leftParenthesis: {
            printExpansion("25) F -> ( G )");
            match(tok_leftParenthesis);
            vector <shared_ptr<Expression>> params;
            parseFuncParam(params);
            match(tok_rightParenthesis);
            return make_shared<FunctionCall>(name, params);
        }
        default:
            printExpansion("26) F -> ε");
            return make_shared<VarReference>(name);
    }
}

shared_ptr<Expression> Parser::parseIdent() {
    printExpansion("27) F' -> ident F");
    const string name = m_Lexer.identifierStr();
    match(tok_identifier);
    return parseIdentSuffix(name);
}

shared_ptr<Expression> Parser::parseIdentOrNumb() {
    switch (CurTok) {
        case tok_number: {
            printExpansion("28) F'' -> numb");
            auto number = make_shared<Number>(m_Lexer.numVal());
            match(tok_number);
            return number;
        }
        case tok_identifier:
            printExpansion("29) F'' -> F'");
            return parseIdent();
        default:
            printExpansion("F'' exception");
            throwParseException({tok_number, tok_identifier});
    }
    return nullptr;
}

shared_ptr<Expression> Parser::parseIdentArraySuffix(shared_ptr<Reference> var) {
    switch (CurTok) {
        case tok_leftBracket: {
            printExpansion("30) F''' -> [ I ] F'''");
            match(tok_leftBracket);
            auto expression = parseExpression();
            match(tok_rightBracket);
            //multidim array
            return parseIdentArraySuffix(make_shared<ArrayItemReference>(var, expression));
        }
        default:
            printExpansion("31) F''' -> ε");
            return var;
    }
}

void Parser::parseFuncParam(vector <shared_ptr<Expression>> & params) {
    switch (CurTok) {
        case tok_number://case = first I
        case tok_not:
        case tok_minus:
        case tok_leftParenthesis:
        case tok_identifier: {
            printExpansion("32) G -> I G'");
            auto tmp = parseExpression();
            params.push_back(tmp);
            parseMultFuncParams(params);
            break;
        }
        case tok_string: {
            printExpansion("33) G -> string G'");
            auto tmp = make_shared<String>(m_Lexer.strVal());
            match(tok_string);
            params.push_back(tmp);
            parseMultFuncParams(params);
            break;
        }
        default:
            printExpansion("34) G -> ε");
            return;
    }
}

void Parser::parseMultFuncParams(vector <shared_ptr<Expression>> & params) {
    switch (CurTok) {
        case tok_comma:
            printExpansion("35) G' -> , G");
            match(tok_comma);
            parseFuncParam(params);
            break;
        default:
            printExpansion("36) G' -> ε");
            return;
    }
}


shared_ptr<Type> Parser::parseType() {
    switch (CurTok) {
        case tok_integer:
            printExpansion("37) H -> integer");
            match(tok_integer);
            return make_shared<Integer>();
        case tok_array: {
            printExpansion("38) H -> array [ P . . P ] of H");
            match(tok_array);
            match(tok_leftBracket);
            auto minIndex = parseNumber();
            match(tok_dot);
            match(tok_dot);
            auto maxIndex = parseNumber();
            match(tok_rightBracket);
            match(tok_of);
            auto type = parseType();
            return make_shared<Array>(minIndex->getValue(), maxIndex->getValue(), type);
        }
        default:
            printExpansion("H exception");
            throwParseException({tok_integer, tok_array});
    }
    return nullptr;
}

shared_ptr<Expression> Parser::parseExpression() {
    printExpansion("39) I -> K I'");
    shared_ptr<Expression> expression = parseLevel2Op();
    return parseLevel1Ops(expression);
}

shared_ptr<Expression> Parser::parseLevel1Ops(shared_ptr<Expression> expression) {
    int op = CurTok;
    switch (CurTok) {
        case tok_equal:
            printExpansion("40) I' -> = K I'");
            match(tok_equal);
            break;
        case tok_notequal:
            printExpansion("41) I' -> <> K I'");
            match(tok_notequal);
            break;
        case tok_less:
            printExpansion("42) I' -> < K I'");
            match(tok_less);
            break;
        case tok_lessequal:
            printExpansion("43) I' -> <= K I'");
            match(tok_lessequal);
            break;
        case tok_greater:
            printExpansion("44) I' -> > K I'");
            match(tok_greater);
            break;
        case tok_greaterequal:
            printExpansion("45) I' -> >= K I'");
            match(tok_greaterequal);
            break;
        default:
            printExpansion("46) I' -> ε");
            return expression;
    }
//    parseLevel2Op();
//    parseLevel1Ops();
    return make_shared<BinOp>(op, expression, parseLevel1Ops(parseLevel2Op()));
}

vector <shared_ptr<Const>> Parser::parseConstDecl() {
    printExpansion("47) J -> ident = numb ; J'");
    vector <shared_ptr<Const>> consts;
    string name = m_Lexer.identifierStr();
    match(tok_identifier);
    match(tok_equal);
    auto val = make_shared<Number>(m_Lexer.numVal());
    match(tok_number);
    match(tok_semicolon);
    consts.push_back(make_shared<Const>(name, val->getValue()));
    parseMultConstDecls(consts);
    return consts;
}

void Parser::parseMultConstDecls(vector <shared_ptr<Const>> & consts) {
    switch (CurTok) {
        case tok_identifier: {
            printExpansion("48) J' -> ident = numb ; J'");
            string name = m_Lexer.identifierStr();
            match(tok_identifier);
            match(tok_equal);
            auto val = make_shared<Number>(m_Lexer.numVal());
            match(tok_number);
            match(tok_semicolon);
            consts.push_back(make_shared<Const>(name, val->getValue()));
            parseMultConstDecls(consts);
            break;
        }
        default:
            printExpansion("49) J' -> ε");
            return;
    }
}

shared_ptr<Expression> Parser::parseLevel2Op() {
    printExpansion("50) K -> L K'");
    auto expression = parseLevel3Op();
    return parseLevel2Ops(expression);
}

shared_ptr<Expression> Parser::parseLevel2Ops(shared_ptr<Expression> expression) {
    int op = CurTok;
    switch (CurTok) {
        case tok_plus:
            printExpansion("51) K' -> + L K'");
            match(tok_plus);
            break;
        case tok_minus:
            printExpansion("52) K' -> - L K'");
            match(tok_minus);
            break;
        case tok_or:
            printExpansion("53) K' -> or L K'");
            match(tok_or);
            break;
        default:
            printExpansion("54) K' -> ε");
            return expression;
    }
//    parseLevel3Op();
//    parseLevel2Ops();
    return make_shared<BinOp>(op, expression, parseLevel2Ops(parseLevel3Op()));
}

shared_ptr<Expression> Parser::parseLevel3Op() {
    printExpansion("55) L -> M L'");
    auto expression = parseLevel4Op();
    return parseLevel3Ops(expression);
}

shared_ptr<Expression> Parser::parseLevel3Ops(shared_ptr<Expression> expression) {
    int op = CurTok;
    switch (CurTok) {
        case tok_multiply:
            printExpansion("56) L' -> * M L'");
            match(tok_multiply);
            break;
        case tok_div:
            printExpansion("57) L' -> div M L'");
            match(tok_div);
            break;
        case tok_mod:
            printExpansion("58) L' -> mod M L'");
            match(tok_mod);
            break;
        case tok_and:
            printExpansion("59) L' -> and M L'");
            match(tok_and);
            break;
        case tok_xor:
            printExpansion("60) L' -> xor M L'");
            match(tok_xor);
            break;
        default:
            printExpansion("61) L' -> ε");
            return expression;
    }
//    parseLevel4Op();
//    parseLevel3Ops();

    return make_shared<BinOp>(op, expression, parseLevel3Ops(parseLevel4Op()));
}

shared_ptr<Expression> Parser::parseLevel4Op() {
    switch (CurTok) {
        case tok_not:
            printExpansion("62) M -> not M");
            match(tok_not);
            return make_shared<UnOp>(tok_not, parseLevel4Op());
        case tok_minus: //first N
        case tok_leftParenthesis:
        case tok_number:
        case tok_identifier:
            printExpansion("63) M -> N");
            return parseLevel5Op();
        default:
            printExpansion("M exception");
            throwParseException({tok_not, tok_minus, tok_leftParenthesis, tok_number, tok_identifier});
    }
    return nullptr;
}

shared_ptr<Expression> Parser::parseLevel5Op() {
    switch (CurTok) {
        case tok_minus:
            printExpansion("64) N -> - N");
            match(tok_minus);
            return make_shared<UnOp>(tok_minus, parseLevel5Op());
        case tok_number://first F''
        case tok_identifier:
            printExpansion("65) N -> F''");
            return parseIdentOrNumb();
        case tok_leftParenthesis: {
            printExpansion("66) N -> ( I )");
            match(tok_leftParenthesis);
            auto expression = parseExpression();
            match(tok_rightParenthesis);
            return expression;
        }
        default:
            printExpansion("N exception");
            throwParseException({tok_minus, tok_number, tok_identifier, tok_leftParenthesis});
    }
    return nullptr;
}

shared_ptr<Statement> Parser::parseIdentLine() {
    printExpansion("67) O -> ident O'");
    const string name = m_Lexer.identifierStr();
    match(tok_identifier);
    return parseAfterIdent(name);
}

shared_ptr<Statement> Parser::parseAfterIdent(const string name) {
    switch (CurTok) {
        case tok_assign:
            printExpansion("68) O' -> := I");
            match(tok_assign);
            return make_shared<Assign>(make_shared<VarReference>(name), parseExpression());
        case tok_leftBracket: {
            printExpansion("69) O' -> [ I ] O''");
            match(tok_leftBracket);
            auto expression = parseExpression();
            match(tok_rightBracket);
            auto var = make_shared<VarReference>(name);
            return parseArrayElement(make_shared<ArrayItemReference>(var, expression));
        }
        case tok_leftParenthesis: {
            printExpansion("70) O' -> ( G )");
            //procedure call
            match(tok_leftParenthesis);
            vector <shared_ptr<Expression>> params;
            parseFuncParam(params);
            match(tok_rightParenthesis);
            return make_shared<ProcedureCall>(name, params);
        }

        default:
            printExpansion("O' exception");
            throwParseException({tok_assign, tok_leftBracket, tok_leftParenthesis});
    }
    return nullptr;
}

shared_ptr<Statement> Parser::parseArrayElement(shared_ptr<ArrayItemReference> var) {
    switch (CurTok) {
        case tok_assign:
            printExpansion("71) O'' -> := I");
            match(tok_assign);
            return make_shared<Assign>(var, parseExpression());
        case tok_leftBracket: {
            printExpansion("72) O'' -> [ I ] O''");
            match(tok_leftBracket);
            auto expression = parseExpression();
            match(tok_rightBracket);
            return parseArrayElement(make_shared<ArrayItemReference>(var, expression));
        }
        default:
            printExpansion("O'' exception");
            throwParseException({tok_assign, tok_leftBracket});
    }
    return nullptr;
}

shared_ptr<Number> Parser::parseNumber() {
    switch (CurTok) {
        case tok_minus: {
            printExpansion("73) P -> - P");
            match(tok_minus);
            auto tmp = parseNumber();
            tmp->neg();
            return tmp;
        }
        case tok_number: {
            printExpansion("74) P -> numb");
            int tmp = m_Lexer.numVal();
            match(tok_number);
            return make_shared<Number>(tmp);
        }
        default:
            printExpansion("P exception");
            throwParseException({tok_minus, tok_number});
    }
    return nullptr;
}

void Parser::parseFuncParamDecl(vector <shared_ptr<Var>> & params) {
    printExpansion("75) Q -> ident E' : H Q'");
    vector <string> names;
    names.push_back(m_Lexer.identifierStr());
    match(tok_identifier);
    parseMultIdent(names);
    match(tok_declaration);
    auto type = parseType();
    for (auto & name: names) {
        params.push_back(make_shared<Var>(name, type, false));
    }
    parseFunctMultParamDecls(params);
}

void Parser::parseFunctMultParamDecls(vector <shared_ptr<Var>> & params) {
    switch (CurTok) {
        case tok_semicolon:
            printExpansion("76) Q' -> ; Q");
            match(tok_semicolon);
            parseFuncParamDecl(params);
            break;
        default:
            printExpansion("77) Q' -> ε");
            return;
    }
}

void Parser::parseNextStatement(vector <shared_ptr<Statement>> & statements) {
    switch (CurTok) {
        case tok_semicolon:
            printExpansion("78) R -> ; R'");
            match(tok_semicolon);
            parseAddNextStatement(statements);
            break;
        default:
            printExpansion("79) R -> ε");
            return;
    }
}

void Parser::parseAddNextStatement(vector <shared_ptr<Statement>> & statements) {
    switch (CurTok) {
        case tok_begin://first D
        case tok_for:
        case tok_while:
        case tok_exit:
        case tok_continue:
        case tok_break:
        case tok_identifier:
        case tok_if:
            printExpansion("80) R' -> D");
            parseStatement(statements);
            break;
        default:
            printExpansion("81) R' -> ε");
            return;
    }
}


shared_ptr<Statement> Parser::parseIf() {
    printExpansion("82) S -> if I then S'");
    match(tok_if);
    auto condition = parseExpression();
    match(tok_then);
    return parseIfBlock(condition);
}

shared_ptr<Statement> Parser::parseIfBlock(shared_ptr<Expression> condition) {
    switch (CurTok) {
        case tok_begin:
            printExpansion("83) S' -> U S''");
            return make_shared<If>(parseBlock(), parseElse(), condition);
        case tok_identifier: {
            printExpansion("84) S' -> O S''");
            vector <shared_ptr<Statement>> statements;
            statements.push_back(parseIdentLine());
            return make_shared<If>(make_shared<Block>(statements), parseElse(), condition);
        }
        case tok_exit: {
            printExpansion("85) S' -> exit S''");
            match(tok_exit);
            vector <shared_ptr<Statement>> statements;
            statements.push_back(make_shared<Special>(tok_exit));
            return make_shared<If>(make_shared<Block>(statements), parseElse(), condition);
        }
        case tok_continue: {
            printExpansion("86) S' -> continue S''");
            match(tok_continue);
            vector <shared_ptr<Statement>> statements;
            statements.push_back(make_shared<Special>(tok_continue));
            return make_shared<If>(make_shared<Block>(statements), parseElse(), condition);
        }
        case tok_break: {
            printExpansion("87) S' -> break S''");
            match(tok_break);
            vector <shared_ptr<Statement>> statements;
            statements.push_back(make_shared<Special>(tok_break));
            return make_shared<If>(make_shared<Block>(statements), parseElse(), condition);
        }
        default:
            printExpansion("S' exception");
            throwParseException({tok_begin, tok_identifier, tok_exit, tok_continue, tok_break});
    }
    return nullptr;
}

shared_ptr<Block> Parser::parseElse() {
    switch (CurTok) {
        case tok_else:
            printExpansion("88) S' -> else S'''");
            match(tok_else);
            return parseElseBlock();
        default:
            printExpansion("89) S'' -> ε");
            return nullptr;
    }
}

shared_ptr<Block> Parser::parseElseBlock() {
    switch (CurTok) {
        case tok_identifier: {
            printExpansion("90) S''' -> O");
            vector <shared_ptr<Statement>> statements;
            statements.push_back(parseIdentLine());
            return make_shared<Block>(statements);
        }
        case tok_exit: {
            printExpansion("91) S''' -> exit");
            match(tok_exit);
            vector <shared_ptr<Statement>> statements;
            statements.push_back(make_shared<Special>(tok_exit));
            return make_shared<Block>(statements);
        }
        case tok_break: {
            printExpansion("92) S''' -> break");
            match(tok_break);
            vector <shared_ptr<Statement>> statements;
            statements.push_back(make_shared<Special>(tok_break));
            return make_shared<Block>(statements);
        }
        case tok_continue: {
            printExpansion("93) S''' -> continue");
            match(tok_continue);
            vector <shared_ptr<Statement>> statements;
            statements.push_back(make_shared<Special>(tok_continue));
            return make_shared<Block>(statements);
        }
        case tok_begin:
            printExpansion("94) S''' -> U");
            return parseBlock();
        default:
            printExpansion("S''' exception");
            throwParseException({tok_identifier, tok_exit, tok_begin, tok_continue, tok_break});
    }
    return nullptr;
}

shared_ptr<Block> Parser::parseFunctionForward() {
    switch (CurTok) {
        case tok_begin:
            printExpansion("95) T -> U");
            return parseBlock();
        case tok_forward:
            printExpansion("96) T -> forward");
            match(tok_forward);
            return nullptr;
        default:
            printExpansion("T exception");
            throwParseException({tok_begin, tok_forward});
    }
    return nullptr;
}

shared_ptr<Block> Parser::parseBlock() {
    printExpansion("97) U -> begin D end");
    match(tok_begin);
    vector <shared_ptr<Statement>> statements;
    parseStatement(statements);
    match(tok_end);
    return make_shared<Block>(statements);
}


