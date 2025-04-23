#include <iostream>
#include <memory>

#include "lexer/lexer.h"
#include "logging.h"
#include "debug_consts.h"

#include "ast.h"

std::unique_ptr<ExprAST> parseExprNoBinop(Lexer& lexer) {
    // doesn't check for binops; use parseExpr

    // values
    if (lexer.curToken.type == TOK_VALUE) {
        // TODO: accept ints
        lexer.consume();
        return std::make_unique<ValueExprAST>(lexer.curToken.rawToken);
    }

    // variables and calls
    if (lexer.curToken.type == TOK_IDENTIFIER) {
        auto identifier = std::make_unique<IdentifierAST>(lexer.curToken.rawToken);
        lexer.consume();
        if (lexer.curToken.rawToken == "(") {
            // TODO: somehow don't do call args in 2 spots
            lexer.consume();
            std::vector<std::unique_ptr<ExprAST> > args;
            while (lexer.curToken.rawToken != ")") {
                auto expr = parseExpr(lexer);
                if (!expr) {
                    return nullptr;
                }
                args.push_back(std::move(expr));
                if (lexer.curToken.rawToken == ",") {
                    lexer.consume();
                } else if (lexer.curToken.rawToken != ")") {
                    return logError("Expected )");
                }
            }
            lexer.consume();
            return std::make_unique<CallExprAST>(std::move(identifier), std::move(args));
        } else {
            return std::make_unique<VariableExprAST>(std::move(identifier));
        }
    }

    // parentheses
    if (lexer.curToken.rawToken == "(") {
        lexer.consume();
        if (auto expr = parseExprNoBinop(lexer)) {
            if (lexer.curToken.rawToken != ")") {
                return logError("Expected )");
            }
            lexer.consume();
            return expr;
        } else {
            return nullptr;
        }
    }

    // unary ops
    if (lexer.curToken.type == TOK_UNOP) {
        auto unOp = lexer.curToken.rawToken;
        lexer.consume();
        if (auto expr = parseExpr(lexer)) {
            return std::make_unique<UnaryOpExprAST>(std::move(expr), unOp);
        } else {
            return nullptr;
        }
    }

    return logError("Expected expression");
}

std::unique_ptr<ExprAST> parseExpr(Lexer& lexer) {
    // Parses an expression _with_ binop checking

    auto firstExpr = parseExprNoBinop(lexer);
    if (!firstExpr) {
        return nullptr;
    }

    std::vector<std::unique_ptr<ExprAST> > stack;
    stack.push_back(std::move(firstExpr));
    std::vector<std::string> opStack;
    while (lexer.curToken.type == TOK_BINOP) {
        auto op = lexer.curToken.rawToken;
        lexer.consume();

        auto expr = parseExprNoBinop(lexer);
        if (!expr) {
            return nullptr;
        }

        while (opStack.size() > 0 && BINOPS.at(op) <= BINOPS.at(opStack.back())) {
            auto RHS = std::move(stack.back());
            stack.pop_back();
            auto LHS = std::move(stack.back());
            stack.pop_back();
            auto binOp = opStack.back();
            opStack.pop_back();
            auto binExpr = std::make_unique<BinaryOpExprAST>(std::move(LHS), std::move(RHS), binOp);
            stack.push_back(std::move(binExpr));
        }
        stack.push_back(std::move(expr));
        opStack.push_back(op);
    }
    while (opStack.size() > 0) {
        auto RHS = std::move(stack.back());
        stack.pop_back();
        auto LHS = std::move(stack.back());
        stack.pop_back();
        auto binOp = opStack.back();
        opStack.pop_back();
        auto binExpr = std::make_unique<BinaryOpExprAST>(std::move(LHS), std::move(RHS), binOp);
        stack.push_back(std::move(binExpr));
    }
    return std::move(stack.back());
}

std::unique_ptr<IfAST> parseIf(Lexer& lexer, bool onIf) {
    auto startToken = onIf ? KW_IF : KW_ELIF;
    if (lexer.curToken.rawToken != startToken) {
        return logError("Expected " + startToken);
    }
    lexer.consume();
    if (lexer.curToken.rawToken != "(") {
        return logError("Expected (");
    }
    if (auto expr = parseExpr(lexer)) {
        if (lexer.curToken.rawToken != ")") {
            return logError("Expected )");
        }
        lexer.consume();
        if (auto block = parseBlock(lexer)) {
            std::optional<std::unique_ptr<BlockAST> > elseBlock;
            if (lexer.curToken.rawToken == KW_ELIF) {
                auto elseStatement = parseIf(lexer, false);
                if (!elseStatement) {
                    return nullptr;
                }
                std::vector<std::unique_ptr<StatementAST> > elseBlockStatements;
                elseBlockStatements.push_back(std::move(elseStatement));
                elseBlock = std::make_unique<BlockAST>(std::move(elseBlockStatements));
            } else if (lexer.curToken.rawToken == KW_ELSE) {
                lexer.consume();
                elseBlock = parseBlock(lexer);
                if (!elseBlock) {
                    return nullptr;
                }
            }
            return std::make_unique<IfAST>(std::move(expr),
                                           std::move(block),
                                           std::move(elseBlock)
            );
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

std::unique_ptr<WhileAST> parseWhile(Lexer& lexer) {
    if (lexer.curToken.rawToken != KW_WHILE) {
        return logError("Expected while");
    }
    lexer.consume();
    if (lexer.curToken.rawToken != "(") {
        return logError("Expected (");
    }
    if (auto expr = parseExpr(lexer)) {
        if (lexer.curToken.rawToken != ")") {
            return logError("Expected )");
        }
        lexer.consume();
        if (auto block = parseBlock(lexer)) {
            return std::make_unique<WhileAST>(std::move(expr), std::move(block));
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

std::unique_ptr<StatementAST> parseVarOrCall(Lexer& lexer) {
    std::optional<std::unique_ptr<TypeAST> > type;
    if (lexer.curToken.type == TOK_TYPE) {
        type = std::make_unique<TypeAST>(lexer.curToken.rawToken);
        lexer.consume();
    }

    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("Expected variable or function call identifier");
    }
    auto identifier = std::make_unique<IdentifierAST>(lexer.curToken.rawToken);
    lexer.consume();

    if (lexer.curToken.rawToken == "(") {
        if (type.has_value()) {
            return logError("Expected var assigment");
        }
        lexer.consume();
        std::vector<std::unique_ptr<ExprAST> > args;
        while (lexer.curToken.rawToken != ")") {
            auto expr = parseExpr(lexer);
            if (!expr) {
                return nullptr;
            }
            args.push_back(std::move(expr));
            if (lexer.curToken.rawToken == ",") {
                lexer.consume();
            } else if (lexer.curToken.rawToken != ")") {
                return logError("Expected )");
            }
        }
        lexer.consume();
        return std::make_unique<CallExprAST>(std::move(identifier), std::move(args));
    } else if (lexer.curToken.rawToken == "=") {
        lexer.consume();
        auto expr = parseExpr(lexer);
        if (!expr) {
            return nullptr;
        }
        return std::make_unique<VarAST>(std::move(type), std::move(identifier), std::move(expr));
    } else {
        if (!type.has_value()) {
            return logError("Expected call or var assignment");
        } else {
            return logError("Expected var assigment");
        }
    }
}

std::unique_ptr<FuncAST> parseFunc(Lexer& lexer) {
    if (lexer.curToken.rawToken != KW_FUNC) {
        return logError("Expected func");
    }
    lexer.consume();
    if (lexer.curToken.type != TOK_TYPE) {
        return logError("Expected type");
    }
    auto returnType = std::make_unique<TypeAST>(lexer.curToken.rawToken);
    lexer.consume();
    if (!lexer.curToken.type == TOK_IDENTIFIER) {
        return logError("Expected function name identifier");
    }
    auto funcName = std::make_unique<IdentifierAST>(lexer.curToken.rawToken);
    lexer.consume();
    if (lexer.curToken.rawToken != "(") {
        return logError("Expected (");
    }
    lexer.consume();

    std::vector<SigArg> signature;
    while (lexer.curToken.rawToken != ")") {
        if (lexer.curToken.type != TOK_TYPE) {
            return logError("Expected type");
        }
        auto type = std::make_unique<TypeAST>(lexer.curToken.rawToken);
        lexer.consume();
        if (!lexer.curToken.type == TOK_IDENTIFIER) {
            return logError("Expected function argument identifier");
        }
        auto identifier = std::make_unique<IdentifierAST>(lexer.curToken.rawToken);
        lexer.consume();
        signature.push_back({std::move(type), std::move(identifier)});
        if (lexer.curToken.rawToken == ",") {
            lexer.consume();
        } else if (lexer.curToken.rawToken != ")") {
            return logError("Expected )");
        }
    }
    lexer.consume();
    if (auto block = parseBlock(lexer)) {
        return std::make_unique<FuncAST>(std::move(returnType),
                                         std::move(funcName),
                                         std::move(signature),
                                         std::move(block));
    } else {
        return nullptr;
    }
}

std::unique_ptr<StatementAST> parseStatement(Lexer& lexer) {
    if (lexer.curToken.type == TOK_TYPE) {
        return parseVarOrCall(lexer);
    }

    if (lexer.curToken.rawToken == KW_FUNC) {
        return parseFunc(lexer);
    } else if (lexer.curToken.rawToken == KW_IF) {
        return parseIf(lexer);
    } else if (lexer.curToken.rawToken == KW_WHILE) {
        return parseWhile(lexer);
    }

    if (lexer.curToken.type == TOK_IDENTIFIER) {
        return parseVarOrCall(lexer);
    } else {
        return logError("Expected variable or function call identifier");
    }
}


std::unique_ptr<BlockAST> parseBlock(Lexer& lexer, bool topLevel) {
    if (!topLevel) {
        if (lexer.curToken.rawToken != "{") {
            return logError("Expected {");
        }
        lexer.consume();
    }

    std::vector<std::unique_ptr<StatementAST> > statements;
    auto expected = topLevel ? std::string(1, EOF) : "}";
    while (lexer.curToken.rawToken != expected) {
        if (auto statement = parseStatement(lexer)) {
            statements.push_back(std::move(statement));
        } else {
            return nullptr;
        }
    }
    lexer.consume();

    auto block = std::make_unique<BlockAST>(std::move(statements));
    if (DEBUG_AST_PRINT_BLOCK && topLevel) {
        std::cout << block->toString() << std::endl;
    }
    return block;
}
