#include <iostream>
#include <memory>

#include "lexer/lexer.h"
#include "ast.h"
#include "module/generated.h"

GeneratedType* parseType(Lexer& lexer) {
    if (lexer.curToken.type != TOK_TYPE && lexer.curToken.type != TOK_IDENTIFIER) {
        return lexer.expected("type");
    }
    auto type = lexer.curToken.rawToken;
    lexer.consume();

    while (lexer.curToken.rawToken == "~" || (lexer.curToken.rawToken == "[" && lexer.peek(1).rawToken == "]")) {
        if (lexer.curToken.rawToken == "~") {
            type += "~";
            lexer.consume();
        } else if (lexer.curToken.rawToken == "[") {
            type += "[]";
            lexer.consume();
            lexer.consume();
        } else {
            assert(false);
        }
    }

    return GeneratedType::rawGet(type);
}

std::unique_ptr<ExprAST> parseRHSExpr(Lexer& lexer) {
    std::unique_ptr<ExprAST> expr;

    if (lexer.curToken.type == TOK_VALUE) {
        // values
        lexer.pushDebugInfo();
        expr = std::make_unique<ValueExprAST>(lexer.curToken.rawToken);
        lexer.consume();
        expr->setDebugInfo(lexer.popDebugInfo());
    } else if (lexer.curToken.type == TOK_IDENTIFIER) {
        // variables
        lexer.pushDebugInfo();
        auto identifier = lexer.curToken.rawToken;
        lexer.consume();
        expr = std::make_unique<VariableExprAST>(std::move(identifier));
        expr->setDebugInfo(lexer.popDebugInfo());
    } else if (lexer.curToken.rawToken == "(") {
        // parentheses
        // TODO: make this it's own ast node (for debuginfo purposes
        lexer.consume();
        expr = parseExpr(lexer);
        if (expr && lexer.curToken.rawToken != ")") {
            return lexer.expected(")");
        }
        lexer.consume();
    } else if (lexer.curToken.rawToken == "~") {
        // constructor and arrays
        // TODO: this collides with unop. figure it out
        if (lexer.peek(1).rawToken == "[") {
            expr = parseArray(lexer);
        } else {
            expr = parseConstructor(lexer);
        }
    } else if (lexer.curToken.type == TOK_UNOP || lexer.curToken.rawToken == "-") {
        // unary ops
        // special case for - because it's a binop and a unop
        lexer.pushDebugInfo();
        auto unOp = lexer.curToken.rawToken;
        lexer.consume();
        expr = std::make_unique<UnaryOpExprAST>(parseRHSExpr(lexer), unOp);
        expr->setDebugInfo(lexer.popDebugInfo());
    } else {
        return lexer.expected("expression");
    }
    if (!expr) {
        return nullptr;
    }

    // Accessor and calls
    while (lexer.curToken.rawToken == "(" || lexer.curToken.rawToken == "." || lexer.curToken.rawToken == "[") {
        if (lexer.curToken.rawToken == "(") {
            expr = parseCall(lexer, std::move(expr));
            if (!expr) {
                return nullptr;
            }
        } else if (lexer.curToken.rawToken == "." || lexer.curToken.rawToken == "[") {
            expr = parseAccessor(lexer, std::move(expr));
            if (!expr) {
                return nullptr;
            }
        } else {
            assert(false);
        }
    }
    return expr;
}

std::unique_ptr<ExprAST> parseExpr(Lexer& lexer) {
    lexer.pushDebugInfo();
    auto firstExpr = parseRHSExpr(lexer);
    if (!firstExpr) {
        return nullptr;
    }

    std::vector<std::unique_ptr<ExprAST> > stack;
    stack.push_back(std::move(firstExpr));
    std::vector<std::string> opStack;
    while (lexer.curToken.type == TOK_BINOP) {
        auto op = lexer.curToken.rawToken;
        lexer.consume();

        lexer.pushDebugInfo();
        auto expr = parseRHSExpr(lexer);
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
            lexer.popDebugInfo();
            binExpr->setDebugInfo(lexer.popDebugInfo(false));
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
        lexer.popDebugInfo();
        binExpr->setDebugInfo(lexer.popDebugInfo(false));
        stack.push_back(std::move(binExpr));
    }
    lexer.popDebugInfo();
    return std::move(stack.back());
}

std::unique_ptr<IfAST> parseIf(Lexer& lexer, const bool onIf) {
    lexer.pushDebugInfo();

    auto startToken = onIf ? KW_IF : KW_ELIF;
    if (lexer.curToken.rawToken != startToken) {
        return lexer.expected(startToken);
    }
    lexer.consume();
    if (lexer.curToken.rawToken != "(") {
        return lexer.expected("(");
    }
    lexer.consume();

    auto expr = parseExpr(lexer);
    if (!expr) {
        return nullptr;
    }
    if (lexer.curToken.rawToken != ")") {
        return lexer.expected(")");
    }
    lexer.consume();

    auto block = parseBlock(lexer);
    if (!block) {
        return nullptr;
    }

    std::optional<std::unique_ptr<BlockAST> > elseBlock;
    if (lexer.curToken.rawToken == KW_ELIF) {
        auto elseStatement = parseIf(lexer, false);
        if (!elseStatement) {
            return nullptr;
        }
        std::vector<std::unique_ptr<StatementAST> > elseBlockStatements;
        elseBlockStatements.push_back(std::move(elseStatement));
        elseBlock = std::make_unique<BlockAST>(std::move(elseBlockStatements));
        elseBlock.value()->setDebugInfo(lexer.popDebugInfo());
    } else if (lexer.curToken.rawToken == KW_ELSE) {
        lexer.consume();
        elseBlock = parseBlock(lexer);
        if (!elseBlock) {
            return nullptr;
        }
    }

    auto ast = std::make_unique<IfAST>(std::move(expr),
                                       std::move(block),
                                       std::move(elseBlock)
    );
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<WhileAST> parseWhile(Lexer& lexer) {
    lexer.pushDebugInfo();

    if (lexer.curToken.rawToken != KW_WHILE) {
        return lexer.expected("while");
    }
    lexer.consume();
    if (lexer.curToken.rawToken != "(") {
        return lexer.expected("(");
    }
    lexer.consume();

    auto expr = parseExpr(lexer);
    if (!expr) {
        return nullptr;
    }
    if (lexer.curToken.rawToken != ")") {
        return lexer.expected(")");
    }
    lexer.consume();

    auto block = parseBlock(lexer);
    if (!block) {
        return nullptr;
    }

    auto ast = std::make_unique<WhileAST>(std::move(expr), std::move(block));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<ReturnAST> parseReturn(Lexer& lexer) {
    lexer.pushDebugInfo();

    if (lexer.curToken.rawToken != KW_RETURN) {
        return lexer.expected("return");
    }
    lexer.consume();

    std::optional<std::unique_ptr<ExprAST> > expr{};
    if (lexer.curToken.type != TOK_DELIMITER) {
        expr = parseExpr(lexer);
        if (!expr.value()) {
            return nullptr;
        }
    }
    auto ast = std::make_unique<ReturnAST>(std::move(expr));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<VarAST> parseVar(Lexer& lexer) {
    lexer.pushDebugInfo();

    bool definition = false;
    if (lexer.curToken.rawToken == KW_LET) {
        definition = true;
        lexer.consume();
    }

    // TODO: move this into parsevariable function
    lexer.pushDebugInfo();
    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return lexer.expected("variable identifier");
    }
    std::unique_ptr<AssignableAST> variableExpr = std::make_unique<VariableExprAST>(lexer.curToken.rawToken);
    lexer.consume();
    variableExpr->setDebugInfo(lexer.popDebugInfo());
    while (lexer.curToken.rawToken == "." || lexer.curToken.rawToken == "[") {
        variableExpr = parseAccessor(lexer, std::move(variableExpr));
        if (!variableExpr) {
            return nullptr;
        }
    }

    std::optional<GeneratedType*> type;
    if (lexer.curToken.rawToken == ":") {
        lexer.consume();
        type = parseType(lexer);
        if (!type.value()) {
            return nullptr;
        }
    }

    if (lexer.curToken.type != TOK_VAROP) {
        return lexer.expected("variable assignment operator");
    }
    auto varOp = lexer.curToken.rawToken;
    lexer.consume();

    auto expr = parseExpr(lexer);
    if (!expr) {
        return nullptr;
    }

    auto ast = std::make_unique<VarAST>(definition, std::move(variableExpr), type, varOp, std::move(expr));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<CallExprAST> parseCall(Lexer& lexer, std::unique_ptr<ExprAST> callee) {
    lexer.pushDebugInfo();
    if (lexer.curToken.rawToken != "(") {
        return lexer.expected("(");
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
            return lexer.expected(")");
        }
    }
    lexer.consume();

    auto ast = std::make_unique<CallExprAST>(std::move(callee), std::move(args));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

template<std::derived_from<ExprAST> T>
std::unique_ptr<T> parseAccessor(Lexer& lexer, std::unique_ptr<T> expr) {
    if (lexer.curToken.rawToken == ".") {
        lexer.pushDebugInfo();
        lexer.consume();
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return lexer.expected("field identifier");
        }
        auto fieldName = lexer.curToken.rawToken;
        lexer.consume();
        expr = std::make_unique<MemberAccessExprAST>(std::move(expr), std::move(fieldName));
        expr->setDebugInfo(lexer.popDebugInfo());
    } else if (lexer.curToken.rawToken == "[") {
        lexer.pushDebugInfo();
        lexer.consume();
        auto indexExpr = parseExpr(lexer);
        if (!indexExpr) {
            return nullptr;
        }
        if (lexer.curToken.rawToken != "]") {
            return lexer.expected("]");
        }
        lexer.consume();
        expr = std::make_unique<SubscriptExprAST>(std::move(expr), std::move(indexExpr));
        expr->setDebugInfo(lexer.popDebugInfo());
    } else {
        return lexer.expected(". or [");
    }
    return expr;
}

std::unique_ptr<ConstructorExprAST> parseConstructor(Lexer& lexer) {
    lexer.pushDebugInfo();

    if (lexer.curToken.rawToken != "~") {
        return lexer.expected("~");
    }
    lexer.consume();

    auto type = parseType(lexer);
    if (!type) {
        return nullptr;
    }

    if (lexer.curToken.rawToken != "{") {
        return lexer.expected("{");
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
            return lexer.expected("field identifier");
        }
        auto valueName = lexer.curToken.rawToken;
        if (values.contains(valueName)) {
            return lexer.expected("unique field identifier");
        }
        lexer.consume();

        if (lexer.curToken.rawToken != ":") {
            return lexer.expected(":");
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
            return lexer.expected("}");
        }
    }
    lexer.consume();

    auto ast = std::make_unique<ConstructorExprAST>(type, std::move(values));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<ArrayExprAST> parseArray(Lexer& lexer) {
    lexer.pushDebugInfo();

    if (lexer.curToken.rawToken != "~") {
        return lexer.expected("~");
    }
    lexer.consume();

    if (lexer.curToken.rawToken != "[") {
        return lexer.expected("[");
    }
    lexer.consume();

    auto values = std::vector<std::unique_ptr<ExprAST> >();
    while (lexer.curToken.rawToken != "]") {
        // TODO: don't allow bare semicolons?
        if (lexer.curToken.type == TOK_DELIMITER) {
            lexer.consume();
            continue;
        }

        auto valueExpr = parseExpr(lexer);
        if (!valueExpr) {
            return nullptr;
        }
        values.push_back(std::move(valueExpr));

        if (lexer.curToken.rawToken == ",") {
            lexer.consume();
        } else if (lexer.curToken.rawToken != "]") {
            return lexer.expected("]");
        }
    }
    lexer.consume();

    auto ast = std::make_unique<ArrayExprAST>(std::move(values));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<ImportAST> parseImport(Lexer& lexer) {
    lexer.pushDebugInfo();

    if (lexer.curToken.rawToken != KW_FROM) {
        return lexer.expected("from");
    }
    lexer.consume();

    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return lexer.expected("module identifier");
    }
    auto unit = lexer.curToken.rawToken;
    lexer.consume();

    while (lexer.curToken.rawToken != KW_IMPORT) {
        if (lexer.curToken.rawToken != ".") {
            return lexer.expected("import");
        }
        unit += ".";
        lexer.consume();
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return lexer.expected("unit identifier");
        }
        unit += lexer.curToken.rawToken;
        lexer.consume();
    }
    lexer.consume();

    std::unordered_map<std::string, std::string> aliases;
    while (lexer.curToken.type != TOK_DELIMITER) {
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return lexer.expected("importable identifier");
        }
        auto imported = lexer.curToken.rawToken;
        std::string alias;
        lexer.consume();
        if (lexer.curToken.rawToken == KW_AS) {
            lexer.consume();
            if (lexer.curToken.type != TOK_IDENTIFIER) {
                return lexer.expected("alias");
            }
            alias = lexer.curToken.rawToken;
            lexer.consume();
        } else {
            alias = imported;
        }
        aliases[imported] = alias;
    }

    auto ast = std::make_unique<ImportAST>(unit, std::move(aliases));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<FuncAST> parseFunc(Lexer& lexer) {
    lexer.pushDebugInfo();

    bool isExtern = false;
    if (lexer.curToken.rawToken == KW_EXTERN) {
        isExtern = true;
        lexer.consume();
    }
    if (lexer.curToken.rawToken != KW_FUNC) {
        return lexer.expected("func");
    }
    lexer.consume();

    if (!lexer.curToken.type == TOK_IDENTIFIER) {
        return lexer.expected("function identifier");
    }
    auto funcName = lexer.curToken.rawToken;
    lexer.consume();

    if (lexer.curToken.rawToken != "(") {
        return lexer.expected("(");
    }
    lexer.consume();

    std::vector<SigArg> signature;
    bool hasVarArgs = false;
    while (lexer.curToken.rawToken != ")") {
        if (lexer.curToken.rawToken == "." && lexer.peek(1).rawToken == "." && lexer.peek(2).rawToken == ".") {
            hasVarArgs = true;
            lexer.consume();
            lexer.consume();
            lexer.consume();
            if (lexer.curToken.rawToken != ")") {
                return lexer.expected(") (varargs must be final argument)");
            }
            continue;
        }

        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return lexer.expected("argument identifier");
        }
        auto identifier = lexer.curToken.rawToken;
        lexer.consume();
        if (lexer.curToken.rawToken != ":") {
            return lexer.expected(":");
        }
        lexer.consume();
        auto type = parseType(lexer);
        if (!type) {
            return nullptr;
        }

        signature.push_back({std::move(type), identifier});
        if (lexer.curToken.rawToken == ",") {
            lexer.consume();
        } else if (lexer.curToken.rawToken != ")") {
            return lexer.expected(")");
        }
    }
    lexer.consume();

    GeneratedType* returnType;
    if (lexer.curToken.rawToken == ":") {
        lexer.consume();
        returnType = parseType(lexer);
        if (!returnType) {
            return nullptr;
        }
    } else {
        returnType = GeneratedType::rawGet(KW_VOID);
    }

    std::optional<std::unique_ptr<BlockAST> > block;
    if (!isExtern) {
        block = parseBlock(lexer);
        if (!block.value()) {
            return nullptr;
        }
    }

    auto ast = std::make_unique<FuncAST>(
        std::move(signature),
        returnType,
        std::move(block),
        funcName,
        isExtern,
        hasVarArgs
    );
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<StructAST> parseStruct(Lexer& lexer) {
    lexer.pushDebugInfo();

    if (lexer.curToken.rawToken != KW_STRUCT) {
        return lexer.expected("struct");
    }
    lexer.consume();
    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return lexer.expected("struct identifier");
    }
    auto structIdentifier = lexer.curToken.rawToken;
    lexer.consume();
    if (lexer.curToken.rawToken != "{") {
        return lexer.expected("{");
    }
    lexer.consume();

    std::unordered_set<std::string> used;
    std::vector<std::tuple<std::string, GeneratedType*> > fields;
    std::unordered_map<std::string, std::unique_ptr<FuncAST> > methods;
    while (lexer.curToken.rawToken != "}") {
        // TODO: don't allow bare semicolons?
        if (lexer.curToken.type == TOK_DELIMITER) {
            lexer.consume();
            continue;
        }

        if (lexer.curToken.rawToken == KW_FUNC) {
            auto function = parseFunc(lexer);
            if (!function) {
                return nullptr;
            }
            // TODO: somehow put these errors not here
            if (used.contains(function->funcName)) {
                return lexer.expected("unique struct field");
            }
            methods[function->funcName] = std::move(function);
        } else if (lexer.curToken.type == TOK_IDENTIFIER) {
            auto identifier = lexer.curToken.rawToken;
            if (used.contains(identifier)) {
                return lexer.expected("unique struct field");
            }

            lexer.consume();
            if (lexer.curToken.rawToken != ":") {
                return lexer.expected(":");
            }
            lexer.consume();
            auto type = parseType(lexer);
            if (!type) {
                return nullptr;
            }
            fields.push_back(std::make_tuple(identifier, type));
        } else {
            return lexer.expected("struct field identifier");
        }
    }
    lexer.consume();

    auto ast = std::make_unique<StructAST>(structIdentifier, std::move(fields), std::move(methods));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

// var assignments are indistinguishable from expressions until the : or varop.
// this is downstream of not making var declarations expressions.
// since subscripts have expressions in them, we just search for first varop or delimiter.
bool isVarAssignment(Lexer& lexer) {
    int peeking = 0;
    while (true) {
        if (lexer.peek(peeking).type == TOK_EOF || lexer.peek(peeking).type == TOK_DELIMITER) {
            return false;
        } else if (lexer.peek(peeking).type == TOK_VAROP) {
            return true;
        }
        peeking += 1;
    }
}

std::unique_ptr<StatementAST> parseStatement(Lexer& lexer) {
    std::unique_ptr<StatementAST> statement;

    if (lexer.curToken.rawToken == KW_FUNC) {
        statement = parseFunc(lexer);
    } else if (lexer.curToken.rawToken == KW_IF) {
        statement = parseIf(lexer);
    } else if (lexer.curToken.rawToken == KW_WHILE) {
        statement = parseWhile(lexer);
    } else if (lexer.curToken.rawToken == KW_RETURN) {
        statement = parseReturn(lexer);
    } else if (lexer.curToken.rawToken == KW_LET || isVarAssignment(lexer)) {
        statement = parseVar(lexer);
    } else {
        statement = parseExpr(lexer);
    }

    if (!statement) {
        return nullptr;
    } else if (lexer.curToken.type != TOK_DELIMITER) {
        return lexer.expected("delimiter after statement");
    }

    return statement;
}

std::unique_ptr<TopLevelAST> parseTopLevel(Lexer& lexer) {
    lexer.startDebugStatement();
    std::unique_ptr<TopLevelAST> statement;
    if (lexer.curToken.rawToken == KW_FUNC ||
        (lexer.curToken.rawToken == KW_EXTERN &&
         lexer.peek(1).rawToken == KW_FUNC)) {
        statement = parseFunc(lexer);
    } else if (lexer.curToken.rawToken == KW_STRUCT) {
        statement = parseStruct(lexer);
    } else if (lexer.curToken.rawToken == KW_FROM) {
        statement = parseImport(lexer);
    } else {
        return lexer.expected("top level statement");
    }

    if (!statement) {
        return nullptr;
    } else if (lexer.curToken.type != TOK_DELIMITER) {
        return lexer.expected("delimiter after statement");
    }

    return statement;
}

std::unique_ptr<BlockAST> parseBlock(Lexer& lexer) {
    lexer.pushDebugInfo();

    if (lexer.curToken.rawToken != "{") {
        return lexer.expected("{");
    }
    lexer.consume();

    std::vector<std::unique_ptr<StatementAST> > statements;
    while (lexer.curToken.rawToken != "}") {
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

    auto ast = std::make_unique<BlockAST>(std::move(statements));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}

std::unique_ptr<UnitAST> parseUnit(Lexer& lexer, const std::string& unit) {
    lexer.pushDebugInfo();

    std::vector<std::unique_ptr<TopLevelAST> > statements;
    while (lexer.curToken.rawToken != std::string(1, EOF)) {
        // TODO: don't allow bare semicolons?
        if (lexer.curToken.type == TOK_DELIMITER) {
            lexer.consume();
            continue;
        }
        if (auto statement = parseTopLevel(lexer)) {
            statements.push_back(std::move(statement));
        } else {
            return nullptr;
        }
    }

    auto ast = std::make_unique<UnitAST>(unit, std::move(statements));
    ast->setDebugInfo(lexer.popDebugInfo());
    return ast;
}
