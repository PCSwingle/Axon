#include <vector>

#include "lexer.h"
#include "logging.h"
#include "ast/ast.h"

char Lexer::next() {
    index += 1;
    if (index >= text.length()) {
        cur = EOF;
    } else {
        cur = text[index];
    }
    return cur;
}

char Lexer::peekChar(int num) {
    if (index + num >= text.length()) {
        return EOF;
    } else {
        return text[index + num];
    }
}

Token Lexer::consume() {
    do {
        tokenIndex += 1;
        if (tokenIndex < tokens.size()) {
            curToken = tokens[tokenIndex];
        } else {
            break;
        }
    } while (curToken.type == TOK_WHITESPACE);
    return curToken;
}

Token Lexer::peek(const int num) {
    if (tokenIndex + num < tokens.size()) {
        return tokens[tokenIndex + num];
    } else {
        return Token();
    }
}


Token Lexer::process() {
    // whitespace
    if (isspace(cur) && cur != '\n') {
        std::string rawToken;
        while (isspace(cur) && cur != '\n') {
            rawToken += cur;
            next();
        }
        return Token(rawToken, TOK_WHITESPACE);
    }

    // token delimiters (; and newline)
    // TODO: newline should be treated differently from ;
    if (cur == ';' || cur == '\n') {
        std::string rawToken(1, cur);
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
        if (TYPES.contains(rawToken)) {
            type = TOK_TYPE;
        } else if (VALUES.contains(rawToken)) {
            type = TOK_VALUE;
        } else if (KEYWORDS.contains(rawToken)) {
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
        return Token(rawToken, TOK_VALUE);
    }

    // comments
    if (cur == '#') {
        while (cur != '\n' && cur != EOF) {
            next();
        }
        return process();
    }
    if (cur == '/' && peekChar(1) == '*') {
        // prevent /*/ from being a full comment
        next();
        next();
        while (!(cur == '*' && peekChar(1) == '/') && cur != EOF) {
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
            auto type = BINOPS.contains(op)
                            ? TOK_BINOP
                            : UNOPS.contains(op)
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
                if (!ESCAPES.contains(cur)) {
                    logWarning("Non escapeable character `" + std::string(1, cur) + "` escaped");
                } else {
                    rawToken += ESCAPES.at(cur);
                    next();
                    continue;
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

void Lexer::startDebugStatement() {
    debugStatementStart = tokenIndex;
}


void Lexer::pushDebugInfo() {
    debugTokenStack.push_back(tokenIndex);
}

DebugInfo Lexer::popDebugInfo(const bool remove) {
    auto startToken = debugTokenStack.back();
    if (remove) {
        debugTokenStack.pop_back();
    }
    return DebugInfo(debugStatementStart, startToken, tokenIndex);
}

std::nullptr_t Lexer::expected(const std::string& expected) {
    parsingError = "Expected " + expected + ", got " + (curToken.rawToken == "\n" ? "\\n" : curToken.rawToken);
    return nullptr;
}


std::string Lexer::formatParsingError(const std::string& unit,
                                      const std::string& filename) {
    return formatError(
        DebugInfo(debugStatementStart, tokenIndex, tokenIndex + 1),
        unit,
        filename,
        parsingError);
}

std::string Lexer::formatError(const DebugInfo& debugInfo,
                               const std::string& unit,
                               const std::string& filename,
                               const std::string& error) {
    int line = 1;
    int column = 1;
    for (int i = 0; i < debugInfo.startToken; i++) {
        if (tokens[i].rawToken == "\n") {
            line += 1;
            column = 1;
        } else {
            column += tokens[i].rawToken.length();
        }
    }

    auto prefix = "    > ";
    std::string highlighted = "";
    highlighted += prefix;

    int startColumn = -1;
    int endColumn = -1;
    int curColumn = 0;

    for (int i = debugInfo.statementStartToken; i < tokens.size(); i++) {
        auto token = tokens[i];
        if (token.type == TOK_EOF) {
            break;
        }

        if (i >= debugInfo.startToken && startColumn == -1) {
            startColumn = curColumn;
        }
        if (i == debugInfo.endToken) {
            endColumn = curColumn;
        }

        highlighted += tokens[i].rawToken;
        curColumn += tokens[i].rawToken.length();
        if (token.type == TOK_DELIMITER) {
            if (tokens[i].rawToken != "\n") {
                highlighted += "\n";
            }
            highlighted += prefix;
            if (startColumn != -1) {
                if (endColumn == -1) {
                    endColumn = curColumn - 1;
                }
                highlighted += std::string(startColumn, ' ');
                highlighted += std::string(endColumn - startColumn, '^');
                if (i < debugInfo.endToken) {
                    highlighted += "\n";
                    highlighted += prefix;
                }
            }
            startColumn = -1;
            endColumn = -1;
            curColumn = 0;
            if (i >= debugInfo.endToken) {
                break;
            }
        }
    }

    return std::format("Error in {} at {}:{}:{}: {}\n{}\n",
                       unit,
                       filename,
                       line,
                       column,
                       error,
                       highlighted
    );
}
