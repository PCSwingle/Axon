#include <iostream>
#include <memory>

#include "lexer/lexer.h"
#include "logging.h"
#include "debug_consts.h"

#include "ast.h"

std::unique_ptr<ExprAST> _parseExprNoBinopNoAccessor(Lexer& lexer) {
    // doesn't check for binops; use parseExpr

    // values
    if (lexer.curToken.type == TOK_VALUE) {
        auto value = lexer.curToken.rawToken;
        lexer.consume();
        return std::make_unique<ValueExprAST>(value);
    }

    // variables and calls
    if (lexer.curToken.type == TOK_IDENTIFIER) {
        if (lexer.peek(1).rawToken == "(") {
            return parseCall(lexer);
        } else {
            auto identifier = lexer.curToken.rawToken;
            lexer.consume();
            return std::make_unique<VariableExprAST>(std::move(identifier));
        }
    }

    // parentheses
    if (lexer.curToken.rawToken == "(") {
        lexer.consume();
        if (auto expr = parseExpr(lexer)) {
            if (lexer.curToken.rawToken != ")") {
                return logError("Expected closing ) for parenthetical expression");
            }
            lexer.consume();
            return expr;
        } else {
            return nullptr;
        }
    }

    // constructor
    // TODO: this collides with unop. change it to something else
    if (lexer.curToken.rawToken == "~") {
        return parseConstructor(lexer);
    }

    // unary ops
    // special case for - because it's a binop and a unop
    if (lexer.curToken.type == TOK_UNOP || lexer.curToken.rawToken == "-") {
        auto unOp = lexer.curToken.rawToken;
        lexer.consume();
        if (auto expr = _parseExprNoBinopNoAccessor(lexer)) {
            return std::make_unique<UnaryOpExprAST>(std::move(expr), unOp);
        } else {
            return nullptr;
        }
    }

    return logError("Expected expression, got " + lexer.curToken.rawToken);
}

std::unique_ptr<ExprAST> _parseExprNoBinop(Lexer& lexer) {
    if (auto expr = _parseExprNoBinopNoAccessor(lexer)) {
        while (lexer.curToken.rawToken == ".") {
            lexer.consume();
            if (lexer.curToken.type != TOK_IDENTIFIER) {
                return logError("expected field name for accessor, got " + lexer.curToken.rawToken);
            }
            auto fieldName = lexer.curToken.rawToken;
            lexer.consume();
            expr = std::make_unique<AccessorExprAST>(std::move(expr), std::move(fieldName));
        }
        return expr;
    } else {
        return nullptr;
    }
}

std::unique_ptr<ExprAST> parseExpr(Lexer& lexer) {
    // Parses an expression _with_ binop checking

    auto firstExpr = _parseExprNoBinop(lexer);
    if (!firstExpr) {
        return nullptr;
    }

    std::vector<std::unique_ptr<ExprAST> > stack;
    stack.push_back(std::move(firstExpr));
    std::vector<std::string> opStack;
    while (lexer.curToken.type == TOK_BINOP) {
        auto op = lexer.curToken.rawToken;
        lexer.consume();

        auto expr = _parseExprNoBinop(lexer);
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
    lexer.consume();
    if (auto expr = parseExpr(lexer)) {
        if (lexer.curToken.rawToken != ")") {
            return logError("Expected ) for if statement");
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
    lexer.consume();
    if (auto expr = parseExpr(lexer)) {
        if (lexer.curToken.rawToken != ")") {
            return logError("Expected ) for while statement");
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

std::unique_ptr<ReturnAST> parseReturn(Lexer& lexer) {
    if (lexer.curToken.rawToken != KW_RETURN) {
        return logError("expected return");
    }
    lexer.consume();

    if (lexer.curToken.type != TOK_DELIMITER) {
        auto expr = parseExpr(lexer);
        if (!expr) {
            return nullptr;
        }
        return std::make_unique<ReturnAST>(std::move(expr));
    } else {
        return std::make_unique<ReturnAST>(std::optional<std::unique_ptr<ExprAST> >{});
    }
}

std::unique_ptr<VarAST> parseVar(Lexer& lexer) {
    std::optional<std::unique_ptr<TypeAST> > type;
    if (lexer.curToken.type == TOK_TYPE || (lexer.curToken.type == TOK_IDENTIFIER && lexer.peek(1).type ==
                                            TOK_IDENTIFIER)) {
        type = std::make_unique<TypeAST>(lexer.curToken.rawToken);
        lexer.consume();
    }

    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("Expected var assignment");
    }
    auto identifier = lexer.curToken.rawToken;
    lexer.consume();

    if (lexer.curToken.type != TOK_VAROP) {
        return logError("Expected variable assignment operator, got " + lexer.curToken.rawToken);
    }
    auto varOp = lexer.curToken.rawToken;
    lexer.consume();

    auto expr = parseExpr(lexer);
    if (!expr) {
        return nullptr;
    }
    if (varOp != "=") {
        auto binOp = varOp.substr(0, varOp.size() - 1);
        auto variableExpr = std::make_unique<VariableExprAST>(identifier);
        expr = std::make_unique<BinaryOpExprAST>(std::move(variableExpr), std::move(expr), binOp);
    }
    return std::make_unique<VarAST>(std::move(type), identifier, std::move(expr));
}

std::unique_ptr<CallExprAST> parseCall(Lexer& lexer) {
    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("Expected variable or function call identifier");
    }
    auto identifier = lexer.curToken.rawToken;
    lexer.consume();

    if (lexer.curToken.rawToken != "(") {
        return logError("Expected opening ( for function call");
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
            return logError("Expected closing ) for function call");
        }
    }
    lexer.consume();
    return std::make_unique<CallExprAST>(identifier, std::move(args));
}

std::unique_ptr<ConstructorExprAST> parseConstructor(Lexer& lexer) {
    if (lexer.curToken.rawToken != "~") {
        return logError("expected ~ for constructor, got " + lexer.curToken.rawToken);
    }
    lexer.consume();

    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("expected struct name for constructor, got " + lexer.curToken.rawToken);
    }
    auto structName = lexer.curToken.rawToken;
    lexer.consume();

    if (lexer.curToken.rawToken != "{") {
        return logError("expected opening { for constructor, got " + lexer.curToken.rawToken);
    }
    lexer.consume();

    auto values = std::unordered_map<std::string, std::unique_ptr<ExprAST> >();
    while (lexer.curToken.rawToken != "}") {
        // TODO: don't allow bare semicolons?
        if (lexer.curToken.type == TOK_DELIMITER) {
            lexer.consume();
            continue;
        }
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("expected constructor field name, got " + lexer.curToken.rawToken);
        }
        auto valueName = lexer.curToken.rawToken;
        if (values.contains(valueName)) {
            return logError("found duplicate field name " + valueName + " in constructor");
        }
        lexer.consume();

        if (lexer.curToken.rawToken != ":") {
            return logError("expected : after field name in constructor, got " + lexer.curToken.rawToken);
        }
        lexer.consume();

        auto valueExpr = parseExpr(lexer);
        if (!valueExpr) {
            return nullptr;
        }
        values.insert_or_assign(valueName, std::move(valueExpr));

        if (lexer.curToken.rawToken == ",") {
            lexer.consume();
        } else if (lexer.curToken.rawToken != "}") {
            return logError("Expected closing } for constructor, got " + lexer.curToken.rawToken);
        }
    }
    lexer.consume();

    return std::make_unique<ConstructorExprAST>(structName, std::move(values));
}

std::unique_ptr<FuncAST> parseFunc(Lexer& lexer) {
    bool native = false;
    if (lexer.curToken.rawToken == KW_NATIVE) {
        native = true;
        lexer.consume();
    }
    if (lexer.curToken.rawToken != KW_FUNC) {
        return logError("Expected func");
    }
    lexer.consume();

    if (!lexer.curToken.type == TOK_IDENTIFIER) {
        return logError("Expected function name identifier");
    }
    auto funcName = lexer.curToken.rawToken;
    lexer.consume();
    if (lexer.curToken.rawToken != "(") {
        return logError("Expected (");
    }
    lexer.consume();

    std::vector<SigArg> signature;
    while (lexer.curToken.rawToken != ")") {
        if (lexer.curToken.type != TOK_TYPE && lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("Expected type");
        }
        auto type = std::make_unique<TypeAST>(lexer.curToken.rawToken);
        lexer.consume();
        if (!lexer.curToken.type == TOK_IDENTIFIER) {
            return logError("Expected function argument identifier");
        }
        auto identifier = lexer.curToken.rawToken;
        lexer.consume();
        signature.push_back({std::move(type), identifier});
        if (lexer.curToken.rawToken == ",") {
            lexer.consume();
        } else if (lexer.curToken.rawToken != ")") {
            return logError("Expected )");
        }
    }
    lexer.consume();

    std::unique_ptr<TypeAST> returnType;
    if (lexer.curToken.rawToken == ":") {
        lexer.consume();
        if (lexer.curToken.type != TOK_TYPE && lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("Expected function return type");
        }
        returnType = std::make_unique<TypeAST>(lexer.curToken.rawToken);
        lexer.consume();
    } else {
        returnType = std::make_unique<TypeAST>("void");
    }

    std::optional<std::unique_ptr<BlockAST> > block;
    if (!native) {
        block = parseBlock(lexer);
        if (!block.value()) {
            return nullptr;
        }
    }
    return std::make_unique<FuncAST>(std::move(returnType),
                                     funcName,
                                     std::move(signature),
                                     std::move(block),
                                     native);
}

std::unique_ptr<StructAST> parseStruct(Lexer& lexer) {
    if (lexer.curToken.rawToken != KW_STRUCT) {
        return logError("expected struct keyword, got " + lexer.curToken.rawToken);
    }
    lexer.consume();
    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("expected struct identifier, got " + lexer.curToken.rawToken);
    }
    auto structIdentifier = lexer.curToken.rawToken;
    lexer.consume();
    if (lexer.curToken.rawToken != "{") {
        return logError("expected opening { for struct, got " + lexer.curToken.rawToken);
    }
    lexer.consume();

    std::vector<std::tuple<std::string, std::unique_ptr<TypeAST> > > fields;
    while (lexer.curToken.rawToken != "}") {
        // TODO: don't allow bare semicolons?
        if (lexer.curToken.type == TOK_DELIMITER) {
            lexer.consume();
            continue;
        }
        if (lexer.curToken.type != TOK_TYPE && lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("expected struct field type, got " + lexer.curToken.rawToken);
        }
        auto type = std::make_unique<TypeAST>(lexer.curToken.rawToken);
        lexer.consume();
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("expected struct field identifier, got " + lexer.curToken.rawToken);
        }
        auto identifier = lexer.curToken.rawToken;
        fields.push_back(std::make_tuple(identifier, std::move(type)));
        lexer.consume();
    }
    lexer.consume();

    return std::make_unique<StructAST>(structIdentifier, std::move(fields));
}

std::unique_ptr<StatementAST> parseStatement(Lexer& lexer) {
    std::unique_ptr<StatementAST> statement;

    if (lexer.curToken.rawToken == KW_FUNC || lexer.curToken.rawToken == KW_NATIVE) {
        statement = parseFunc(lexer);
    } else if (lexer.curToken.rawToken == KW_STRUCT) {
        statement = parseStruct(lexer);
    } else if (lexer.curToken.rawToken == KW_IF) {
        statement = parseIf(lexer);
    } else if (lexer.curToken.rawToken == KW_WHILE) {
        statement = parseWhile(lexer);
    } else if (lexer.curToken.rawToken == KW_RETURN) {
        statement = parseReturn(lexer);
    } else if (lexer.curToken.type == TOK_TYPE ||
               (lexer.curToken.type == TOK_IDENTIFIER &&
                lexer.peek(1).type == TOK_IDENTIFIER) ||
               (lexer.curToken.type == TOK_IDENTIFIER &&
                lexer.peek(1).type == TOK_VAROP)) {
        statement = parseVar(lexer);
    } else {
        statement = parseExpr(lexer);
    }

    if (!statement) {
        return nullptr;
    } else if (lexer.curToken.type != TOK_DELIMITER) {
        return logError("Expected delimiter after statement, got " + lexer.curToken.rawToken);
    }

    return statement;
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
        // TODO: don't allow bare semicolons?
        if (lexer.curToken.type == TOK_DELIMITER) {
            lexer.consume();
            continue;
        }
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
