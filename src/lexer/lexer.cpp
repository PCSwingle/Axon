#include <vector>

#include "lexer.h"
#include "logging.h"


char Lexer::next() {
    index += 1;
    if (index >= text.length()) {
        cur = EOF;
    } else {
        cur = text[index];
    }
    return cur;
}

char Lexer::peek_char(int num) {
    if (index + num >= text.length()) {
        return EOF;
    } else {
        return text[index + num];
    }
}

Token Lexer::consume() {
    token_index += 1;
    if (token_index < tokens.size()) {
        curToken = tokens[token_index];
    }
    return curToken;
}

Token Lexer::peek(int num) {
    if (token_index + num < tokens.size()) {
        return tokens[token_index + num];
    } else {
        return Token();
    }
}


Token Lexer::process() {
    // whitespace
    while (isspace(cur) && cur != '\n') {
        next();
    }

    // token delimiters (; and newline)
    if (cur == ';' || cur == '\n') {
        std::string rawToken{1, cur};
        next();
        return Token(rawToken, TOK_DELIMITER);
    }

    // eof
    if (cur == EOF) {
        return Token(std::string(1, EOF), TOK_EOF);
    }

    // identifiers / keywords
    if (isalpha(cur) || cur == '_') {
        std::string rawToken;
        while (isalnum(cur) || cur == '_') {
            rawToken += cur;
            next();
        }
        TokenType type;
        if (TYPES.find(rawToken) != TYPES.end()) {
            type = TOK_TYPE;
        } else if (VALUES.find(rawToken) != VALUES.end()) {
            type = TOK_VALUE;
        } else if (KEYWORDS.find(rawToken) != KEYWORDS.end()) {
            type = TOK_KEYWORD;
        } else {
            type = TOK_IDENTIFIER;
        }
        return Token(rawToken, type);
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
        if (cur == 'L' || cur == 'D') {
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
        return process();
    }
    if (cur == '/' && peek_char(1) == '*') {
        // prevent /*/ from being a full comment
        next();
        next();
        while (!(cur == '*' && peek_char(1) == '/') && cur != EOF) {
            next();
        }
        return process();
    }

    // operators
    for (const auto& op: ALLOPS) {
        auto length = op.length();
        if (index + length > text.length()) {
            continue;
        }
        if (text.substr(index, length) == op) {
            index += length - 1;
            next();
            auto type = BINOPS.find(op) != BINOPS.end()
                            ? TOK_BINOP
                            : UNOPS.find(op) != UNOPS.end()
                                  ? TOK_UNOP
                                  : TOK_VAROP;
            return Token(op, type);
        }
    }

    // string literals
    if (cur == '\'' || cur == '"') {
        std::string rawToken;
        rawToken += cur;
        auto endChar = cur;
        next();
        while (cur != endChar && cur != EOF) {
            if (cur == '\\') {
                next();
                if (ESCAPES.find(cur) != ESCAPES.end()) {
                    logWarning("Non escapeable character " + std::string(1, cur) + " escaped");
                }
            }
            rawToken += cur;
            next();
        }
        rawToken += cur;
        next();
        return Token(rawToken, TOK_VALUE);
    }

    auto rawToken = std::string(1, cur);
    next();
    return Token(rawToken, TOK_UNKNOWN);
}
