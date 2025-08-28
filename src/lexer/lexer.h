#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <iostream>
#include <ranges>
#include <vector>

struct DebugInfo;

enum TokenType {
    TOK_IDENTIFIER,
    TOK_KEYWORD,
    TOK_TYPE,
    TOK_VALUE,
    TOK_EOF,
    TOK_BINOP,
    TOK_UNOP,
    TOK_VAROP,
    TOK_WHITESPACE,
    TOK_DELIMITER,
    TOK_UNKNOWN,
};

inline const std::unordered_map<std::string, int> BINOPS{
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

inline const std::unordered_set<std::string> VAROPS{
    "=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "|=",
    "&=",
    "^=",
    "<<=",
    ">>="
};

inline const std::unordered_map<char, std::string> ESCAPES{
    {'"', "\""},
    {'\'', "'"},
    {'\\', "\\"},
    {'n', "\n"},
};

inline const std::vector<std::string> ALLOPS = []() {
    std::vector<std::string> allOps;
    allOps.reserve(BINOPS.size() + UNOPS.size() + VAROPS.size());
    for (const auto& str: BINOPS | std::views::keys) {
        allOps.push_back(str);
    }
    for (const auto& str: UNOPS) {
        allOps.push_back(str);
    }
    for (const auto& str: VAROPS) {
        allOps.push_back(str);
    }
    std::ranges::sort(allOps,
                      [](const std::string& a, const std::string& b) {
                          return a.length() > b.length();
                      });
    return allOps;
}();

// TODO: figure out if we need ptr types (only very rarely different from size anyways)
// types
#define X_TYPE \
    TYPE(BYTE, "byte")       \
    TYPE(UBYTE, "ubyte")       \
    TYPE(INT, "int")     \
    TYPE(UINT, "uint")     \
    TYPE(LONG, "long")     \
    TYPE(ULONG, "ulong")     \
    TYPE(ISIZE, "isize") \
    TYPE(USIZE, "usize") \
/*  TYPE(PTR, "ptr")   \
    TYPE(UPTR, "uptr")*/ \
    TYPE(FLOAT, "float")     \
    TYPE(DOUBLE, "double")     \
    TYPE(BOOL, "bool")   \
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
    KEYWORD(EXTERN, "extern") \
    KEYWORD(STRUCT, "struct") \
    KEYWORD(LET, "let") \
    KEYWORD(FROM, "from") \
    KEYWORD(IMPORT, "import") \
    KEYWORD(AS, "as")

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
    // TODO: multiple types (since identifier can be a type as well, minus can be unary and binary op, etc.)
    TokenType type;

    explicit Token(std::string rawToken, const TokenType type): rawToken(std::move(rawToken)), type(type) {
    }

    explicit Token(): rawToken(std::string(1, EOF)), type(TOK_EOF) {
    }
};

class Lexer {
    std::string text;

    char cur = EOF;
    size_t index = -1;

    std::vector<Token> tokens;
    size_t tokenIndex = -1;

    char next();

    char peekChar(int num);

    Token process();

    int debugStatementStart;
    std::vector<int> debugTokenStack;

public:
    std::string parsingError;

    Token curToken;

    explicit Lexer(const std::string& text): text(text) {
        debugStatementStart = 0;
        debugTokenStack = std::vector<int>{};

        next();
        Token token;
        do {
            token = process();
            tokens.push_back(token);
        } while (token.type != TOK_EOF);
        consume();
    }

    Token consume();

    Token peek(int num);

    void startDebugStatement();

    void pushDebugInfo();

    DebugInfo popDebugInfo(bool remove = true);

    std::nullptr_t expected(const std::string& expected);

    std::string formatParsingError(const std::string& unit,
                                   const std::string& filename);

    std::string formatError(const DebugInfo& debugInfo,
                            const std::string& unit,
                            const std::string& filename,
                            const std::string& error);
};
