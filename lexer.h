#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
    TOK_UNKNOWN,
};

std::unordered_map<std::string, int> BINOPS{
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

std::unordered_set<std::string> UNOPS{
    "~",
    "-",
    "!",
};


#define X_TYPE \
    TYPE(INT, "int") \
    TYPE(LONG, "long") \
    TYPE(BOOL, "bool")

std::unordered_set<std::string> TYPES{
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
std::unordered_set<std::string> KEYWORDS{
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


    char next() {
        index += 1;
        if (index >= text.length()) {
            cur = EOF;
        } else {
            cur = text[index];
        }
        return cur;
    }

    char peek(int num) {
        if (index + num >= text.length()) {
            return EOF;
        } else {
            return text[index + num];
        }
    }

    Token _consume() {
        // token delimiters
        if (isspace(cur) || cur == ';') {
            next();
            return _consume();
        }

        // eof
        if (cur == EOF) {
            return Token(std::string(1, EOF), TOK_EOF);
        }

        // identifiers / keywords
        if (isalpha(cur)) {
            std::string rawToken;
            while (isalnum(cur) || cur == '_') {
                rawToken += cur;
                next();
            }
            if (TYPES.find(rawToken) != KEYWORDS.end()) {
                return Token(rawToken, TOK_TYPE);
            }
            if (KEYWORDS.find(rawToken) != KEYWORDS.end()) {
                return Token(rawToken, TOK_KEYWORD);
            }
            return Token(rawToken, TOK_IDENTIFIER);
        }

        // values
        if (isdigit(cur) || cur == '.') {
            std::string rawToken;
            bool dot = false;
            while (isdigit(cur) || (!dot && cur == '.')) {
                if (cur == '.') {
                    dot = true;
                }
                rawToken += cur;
                next();
            }
            return Token(rawToken, TOK_VALUE);
        }

        // comments
        if (cur == '#') {
            while (cur != '\n' && cur != EOF) {
                next();
            }
            return _consume();
        }
        if (cur == '/' && peek(1) == '*') {
            // prevent /*/ from being a full comment
            next();
            next();
            while (!(cur == '*' && peek(1) == '/') && cur != EOF) {
                next();
            }
            return _consume();
        }

        // operators
        std::vector<std::string> allOps;
        allOps.reserve(BINOPS.size() + UNOPS.size());
        for (const auto &[str, _]: BINOPS) {
            allOps.push_back(str);
        }
        for (const auto &str: UNOPS) {
            allOps.push_back(str);
        }
        std::sort(allOps.begin(),
                  allOps.end(),
                  [](const std::string &a, const std::string &b) {
                      return a.length() > b.length();
                  });

        for (const auto &op: allOps) {
            auto length = op.length();
            if (index + length > text.length()) {
                continue;
            }
            if (text.substr(index, length) == op) {
                index += length - 1;
                next();
                auto type = BINOPS.find(op) != BINOPS.end() ? TOK_BINOP : TOK_UNOP;
                return Token(op, type);
            }
        }

        for (const auto &op: UNOPS) {
            auto length = op.length();
            if (index + length > text.length()) {
                continue;
            }
            if (text.substr(index, length) == op) {
                index += length - 1;
                next();
                return Token(op, TOK_UNOP);
            }
        }

        auto rawToken = std::string(1, cur);
        next();
        return Token(rawToken, TOK_UNKNOWN);
    }

public:
    Token curToken = Token("", TOK_UNKNOWN);

    explicit Lexer(std::string text): text(std::move(text)) {
        next();
        consume();
    }

    Token consume() {
        curToken = _consume();
        if (DEBUG_LEXER_PRINT_TOKENS) {
            std::cout << "Token: " << curToken.rawToken << " Type: " << curToken.type << std::endl;
        }
        return curToken;
    }
};
