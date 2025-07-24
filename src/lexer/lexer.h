#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <iostream>
#include <vector>

#include "debug_consts.h"

enum TokenType {
    TOK_IDENTIFIER,
    TOK_KEYWORD,
    TOK_TYPE,
    TOK_VALUE,
    TOK_EOF,
    TOK_BINOP,
    TOK_UNOP,
    TOK_DELIMITER,
    TOK_UNKNOWN,
};

static const std::unordered_map<std::string, int> BINOPS{
    {"*", 100},
    {"/", 100},
    {"%", 100},

    {"+", 90},
    {"-", 90},

    {"<<", 80},
    {">>", 80},

    {"<", 70},
    {">", 70},
    {"<=", 70},
    {">=", 70},

    {"==", 60},
    {"!=", 60},

    {"&", 50},

    {"^", 40},

    {"|", 30},

    {"&&", 20},

    {"||", 10}
};

static const std::unordered_map<char, std::string> ESCAPES{
    {'"', "\""},
    {'\'', "'"},
    {'\\', "\\"}
};

inline const std::unordered_set<std::string> UNOPS{
    "~",
    "-",
    "!",
};


// types
#define X_TYPE \
    TYPE(INT, "int") \
    TYPE(LONG, "long") \
    TYPE(DOUBLE, "double") \
    TYPE(BOOL, "bool") \
    TYPE(VOID, "void")

inline const std::unordered_set<std::string> TYPES{
#define TYPE(NAME, STR) STR,
    X_TYPE
#undef TYPE
};

// values
#define X_VALUE \
    VALUE(TRUE, "true") \
    VALUE(FALSE, "false")

inline const std::unordered_set<std::string> VALUES{
#define VALUE(NAME, STR) STR,
    X_VALUE
#undef VALUE
};

// keywords
#define X_KW \
    KEYWORD(FUNC, "func") \
    KEYWORD(IF, "if") \
    KEYWORD(ELIF, "elif") \
    KEYWORD(ELSE, "else") \
    KEYWORD(WHILE, "while") \
    KEYWORD(RETURN, "return") \
    KEYWORD(NATIVE, "native")

#define KEYWORD(NAME, STR) const std::string KW_##NAME = STR;
X_KW
#undef KEYWORD
#define TYPE(NAME, STR) const std::string KW_##NAME = STR;
X_TYPE
#undef TYPE
#define VALUE(NAME, STR) const std::string KW_##NAME = STR;
X_VALUE
#undef VALUE

inline const std::unordered_set<std::string> KEYWORDS{
#define KEYWORD(NAME, STR) STR,
    X_KW
#undef KEYWORD
#define TYPE(NAME, STR) STR,
    X_TYPE
#undef TYPE
#define VALUE(NAME, STR) STR,
    X_VALUE
#undef VALUE
};

struct Token {
    std::string rawToken;
    TokenType type;

    explicit Token(std::string rawToken, const TokenType type): rawToken(std::move(rawToken)), type(type) {
    }

    explicit Token(): rawToken(std::string(1, EOF)), type(TOK_EOF) {
    }
};

class Lexer {
    char cur = EOF;
    std::string text;
    size_t index = -1;

    std::vector<Token> tokens;
    size_t token_index = -1;

    char next();

    char peek_char(int num);

    Token process();

public:
    Token curToken;

    explicit Lexer(std::string text): text(std::move(text)) {
        next();
        Token token;
        do {
            token = process();
            tokens.push_back(token);
            if (DEBUG_LEXER_PRINT_TOKENS) {
                std::cout << "Token: " << token.rawToken << " Type: " << token.type << std::endl;
            }
        } while (token.type != TOK_EOF);
        consume();
    }

    Token consume();

    Token peek(int num);
};
