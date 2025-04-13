#include "lexer.h"

#include <vector>
#include <debug_consts.h>

char Lexer::next() {
    index += 1;
    if (index >= text.length()) {
        cur = EOF;
    } else {
        cur = text[index];
    }
    return cur;
}

char Lexer::peek(int num) {
    if (index + num >= text.length()) {
        return EOF;
    } else {
        return text[index + num];
    }
}

Token Lexer::_consume() {
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

Token Lexer::consume() {
    curToken = _consume();
    if (DEBUG_LEXER_PRINT_TOKENS) {
        std::cout << "Token: " << curToken.rawToken << " Type: " << curToken.type << std::endl;
    }
    return curToken;
}
