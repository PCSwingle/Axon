#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <iostream>

enum TokenType {
    TOK_IDENTIFIER,
    TOK_KEYWORD,
    TOK_TYPE,
    TOK_VALUE,
    TOK_EOF,
    TOK_BINOP,
    TOK_UNOP,
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

inline const std::unordered_set<std::string> UNOPS{
    "~",
    "-",
    "!",
};


#define X_TYPE \
    TYPE(INT, "int") \
    TYPE(LONG, "long") \
    TYPE(BOOL, "bool")

inline const std::unordered_set<std::string> TYPES{
#define TYPE(NAME, STR) STR,
    X_TYPE
#undef TYPE
};

#define X_KW \
    KEYWORD(FUNC , "func") \
    KEYWORD(IF, "if") \
    KEYWORD(ELIF, "elif") \
    KEYWORD(ELSE, "else") \
    KEYWORD(WHILE, "while")

#define KEYWORD(NAME, STR) const std::string KW_##NAME = STR;
X_KW
#undef KEYWORD
#define TYPE(NAME, STR) const std::string KW_##NAME = STR;
X_TYPE
#undef TYPE
inline const std::unordered_set<std::string> KEYWORDS{
#define KEYWORD(NAME, STR) STR,
    X_KW
#undef KEYWORD
#define TYPE(NAME, STR) STR,
    X_TYPE
#undef TYPE
};

struct Token {
    std::string rawToken;
    TokenType type;

    explicit Token(std::string rawToken, const TokenType type): rawToken(std::move(rawToken)), type(type) {
    }
};

class Lexer {
    char cur = EOF;
    std::string text;
    size_t index = -1;


    char next();

    char peek(int num);

    Token _consume();

public:
    Token curToken = Token("", TOK_UNKNOWN);

    explicit Lexer(std::string text): text(std::move(text)) {
        next();
        consume();
    }

    Token consume();
};
