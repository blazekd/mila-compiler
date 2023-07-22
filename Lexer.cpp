#include "Lexer.hpp"


enum CharType {
    LETTER, NUMBER, WHITE_SPACE, END, NO_TYPE
};
static int character;   // input symbol
static CharType input; // input symbol type


unordered_map<string, Token> keyWords = {
        {"begin",     tok_begin},
        {"end",       tok_end},
        {"const",     tok_const},
        {"procedure", tok_procedure},
        {"forward",   tok_forward},
        {"function",  tok_function},
        {"if",        tok_if},
        {"then",      tok_then},
        {"else",      tok_else},
        {"program",   tok_program},
        {"while",     tok_while},
        {"exit",      tok_exit},
        {"var",       tok_var},
        {"integer",   tok_integer},
        {"for",       tok_for},
        {"do",        tok_do},
        {"or",        tok_or},
        {"mod",       tok_mod},
        {"div",       tok_div},
        {"not",       tok_not},
        {"and",       tok_and},
        {"xor",       tok_xor},
        {"to",        tok_to},
        {"downto",    tok_downto},
        {"array",     tok_array},
        {"of",        tok_of},
        {"break",     tok_break},
        {"continue",  tok_continue}
};


void readInput() {

    character = getchar();
    if (character == EOF)
        input = END;
    else if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z'))
        input = LETTER;
    else if (character >= '0' && character <= '9')
        input = NUMBER;
    else if (isspace(character))
        input = WHITE_SPACE;
    else
        input = NO_TYPE;
}


/**
 * @brief Function to return the next token from standard input
 *
 * the variable 'm_IdentifierStr' is set there in case of an identifier,
 * the variable 'm_NumVal' is set there in case of a number.
 */
int Lexer::gettok() {
    readInput();
    int base = 10;
    int digit = 0;
    q0:
    switch (character) {
        case ':':
            readInput();
            goto q1;
        case '\'':
            readInput();
            m_StrVal.clear();
            goto q2;
        case '.':
            return tok_dot;
        case '&':
            readInput();
            m_NumVal = 0;
            base = 8;
            goto q4;
        case '$':
            readInput();
            m_NumVal = 0;
            base = 16;
            goto q4;
        case '<':
            readInput();
            goto q5;
        case '>':
            readInput();
            goto q6;
        case '(':
            return tok_leftParenthesis;
        case ')':
            return tok_rightParenthesis;
        case '[':
            return tok_leftBracket;
        case ']':
            return tok_rightBracket;
        case '+':
            return tok_plus;
        case '-':
            return tok_minus;
        case '*':
            return tok_multiply;
        case '=':
            return tok_equal;
        case ';':
            return tok_semicolon;
        case ',':
            return tok_comma;
        case '{':
            goto q7;
    }

    switch (input) {
        case END:
            return tok_eof;
        case LETTER:
            m_IdentifierStr.clear();
            goto q3;
        case NUMBER:
            m_NumVal = 0;
            goto q4;
        case WHITE_SPACE:
            readInput();
            goto q0;
        default:
            return tok_error;
    }

    q1: //type declaration, assign
    switch (input) {
        case WHITE_SPACE:
            return tok_declaration;
        default:
            break;
    }
    switch (character) {
        case '=':
            return tok_assign;
        default:
            return tok_error;
    }


    q2: //string
    switch (character) {
        case '\\':
            readInput();
            m_StrVal += character;
            readInput();
            break;
        case '\'':
            return tok_string;
        default:
            m_StrVal += character;
            readInput();
            goto q2;
    }

    q3: //identifier Special symbols - + * / := , . ;. () [] = {} ` white space
    switch (character) {
        case '[':
        case '(':
        case ']':
        case ')':
        case '.':
        case ':':
        case '+':
        case '-':
        case '*':
        case ',':
        case ';':
            ungetc(character, stdin);
            auto a = keyWords.find(m_IdentifierStr);
            if (a == keyWords.end())
                return tok_identifier;
            else
                return a->second;
    }
    switch (input) {
        case WHITE_SPACE: {
            auto a = keyWords.find(m_IdentifierStr);
            if (a == keyWords.end())
                return tok_identifier;
            else
                return a->second;
        }
        case NO_TYPE:
            if (character != '_')
                return tok_error;
        case NUMBER:
        case LETTER:
            m_IdentifierStr += character;
            readInput();
            goto q3;
        default:
            break;
    }

    q4: //number
    switch (character) {
        case ']':
        case ')':
        case '+':
        case '-':
        case '*':
        case ',':
        case ';':
            ungetc(character, stdin);
            return tok_number;
    }
    switch (input) {
        case LETTER:
        case NUMBER:
            if (character >= 'a')
                digit = character - 'a' + 10;
            else if (character >= 'A')
                digit = character - 'A' + 10;
            else
                digit = character - '0';
            if (digit >= base)
                return tok_error;
            m_NumVal = m_NumVal * base + digit;
            readInput();
            goto q4;
        case WHITE_SPACE:
            return tok_number;
        default:
            return tok_error;
    }

    q5:   //less, lessequal, not equal
    switch (input) {
        case WHITE_SPACE:
            return tok_less;
        default:
            break;
    }
    switch (character) {
        case '>':
            return tok_notequal;
        case '=':
            return tok_lessequal;
        default:
            return tok_error;
    }

    q6: //greater, greaterequal
    switch (input) {
        case WHITE_SPACE:
            return tok_greater;
        default:
            break;
    }
    switch (character) {
        case '=':
            return tok_greaterequal;
        default:
            return tok_error;
    }

    q7: //comments
        switch (character) {
            case '}':
                readInput();
                goto q0;
            default:
                readInput();
                goto q7;

        }
}


