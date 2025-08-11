#include <iostream>
#include <memory>

#include "lexer/lexer.h"
#include "logging.h"
#include "debug_consts.h"

#include "ast.h"
#include "module/generated.h"

GeneratedType* parseType(Lexer& lexer) {
    if (lexer.curToken.type != TOK_TYPE && lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("Expected type, got " + lexer.curToken.rawToken);
    }
    auto type = lexer.curToken.rawToken;
    lexer.consume();

    while (lexer.curToken.rawToken == "[" && lexer.peek(1).rawToken == "]") {
        type += "[]";
        lexer.consume();
        lexer.consume();
    }

    return GeneratedType::get(type);
}

std::unique_ptr<ExprAST> _parseExprNoBinopNoAccessor(Lexer& lexer) {
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

    // constructor and arrays
    // TODO: this collides with unop. figure it out
    if (lexer.curToken.rawToken == "~") {
        if (lexer.peek(1).rawToken == "[") {
            return parseArray(lexer);
        } else {
            return parseConstructor(lexer);
        }
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

std::unique_ptr<IfAST> parseIf(Lexer& lexer, const bool onIf) {
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
    bool definition = false;
    if (lexer.curToken.rawToken == KW_LET) {
        definition = true;
        lexer.consume();
    }

    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("Expected var assignment");
    }
    auto identifier = lexer.curToken.rawToken;
    lexer.consume();

    std::vector<std::string> fieldNames{};
    while (lexer.curToken.rawToken == ".") {
        lexer.consume();
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("Expected field name after accessor");
        }
        fieldNames.push_back(lexer.curToken.rawToken);
        lexer.consume();
    }

    std::optional<GeneratedType*> type;
    if (lexer.curToken.rawToken == ":") {
        lexer.consume();
        if (!definition) {
            return logError("Cannot only set variable type on definition");
        }
        type = parseType(lexer);
        if (!type.value()) {
            return nullptr;
        }
    }

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
        // TODO: clean up accessor (somehow?) between it, here, and variablexprast
        std::unique_ptr<ExprAST> varPointer = std::make_unique<VariableExprAST>(identifier);
        for (const auto& fieldName: fieldNames) {
            varPointer = std::make_unique<AccessorExprAST>(std::move(varPointer), fieldName);
        }
        expr = std::make_unique<BinaryOpExprAST>(std::move(varPointer), std::move(expr), binOp);
    }
    return std::make_unique<VarAST>(definition, identifier, std::move(fieldNames), type, std::move(expr));
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

    auto type = parseType(lexer);
    if (!type) {
        return nullptr;
    }

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

    return std::make_unique<ConstructorExprAST>(type, std::move(values));
}

std::unique_ptr<ArrayExprAST> parseArray(Lexer& lexer) {
    if (lexer.curToken.rawToken != "~") {
        return logError("expected ~ for array, got " + lexer.curToken.rawToken);
    }
    lexer.consume();

    if (lexer.curToken.rawToken != "[") {
        return logError("expected opening [ for array, got " + lexer.curToken.rawToken);
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
            return logError("Expected closing ] for array, got " + lexer.curToken.rawToken);
        }
    }
    lexer.consume();

    return std::make_unique<ArrayExprAST>(std::move(values));
}

std::unique_ptr<ImportAST> parseImport(Lexer& lexer) {
    if (lexer.curToken.rawToken != KW_FROM) {
        return logError("expected from, got " + lexer.curToken.rawToken);
    }
    lexer.consume();

    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("expected import unit, got " + lexer.curToken.rawToken);
    }
    auto unit = lexer.curToken.rawToken;
    lexer.consume();

    while (lexer.curToken.rawToken != KW_IMPORT) {
        if (lexer.curToken.rawToken != ".") {
            return logError("expected import, got " + lexer.curToken.rawToken);
        }
        unit += ".";
        lexer.consume();
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("expected import unit, got " + lexer.curToken.rawToken);
        }
        unit += lexer.curToken.rawToken;
        lexer.consume();
    }
    lexer.consume();

    std::unordered_map<std::string, std::string> aliases;
    while (lexer.curToken.type != TOK_DELIMITER) {
        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("expected import identifier, got " + lexer.curToken.rawToken);
        }
        auto imported = lexer.curToken.rawToken;
        std::string alias;
        lexer.consume();
        if (lexer.curToken.rawToken == KW_AS) {
            lexer.consume();
            if (lexer.curToken.type != TOK_IDENTIFIER) {
                return logError("expected import alias, got " + lexer.curToken.rawToken);
            }
            alias = lexer.curToken.rawToken;
            lexer.consume();
        } else {
            alias = imported;
        }
        aliases[imported] = alias;
    }
    if (aliases.size() == 0) {
        return logError("import statement with nothing imported found");
    }
    return std::make_unique<ImportAST>(unit, std::move(aliases));
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
        if (!lexer.curToken.type == TOK_IDENTIFIER) {
            return logError("Expected function argument identifier");
        }
        auto identifier = lexer.curToken.rawToken;
        lexer.consume();
        if (lexer.curToken.rawToken != ":") {
            return logError("Expected : after variable name");
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
            return logError("Expected )");
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
        returnType = GeneratedType::get(KW_VOID);
    }

    std::optional<std::unique_ptr<BlockAST> > block;
    if (!native) {
        block = parseBlock(lexer);
        if (!block.value()) {
            return nullptr;
        }
    }
    return std::make_unique<FuncAST>(
        funcName,
        std::move(signature),
        returnType,
        std::move(block),
        native
    );
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

    std::vector<std::tuple<std::string, GeneratedType*> > fields;
    while (lexer.curToken.rawToken != "}") {
        // TODO: don't allow bare semicolons?
        if (lexer.curToken.type == TOK_DELIMITER) {
            lexer.consume();
            continue;
        }

        if (lexer.curToken.type != TOK_IDENTIFIER) {
            return logError("expected struct field identifier, got " + lexer.curToken.rawToken);
        }
        auto identifier = lexer.curToken.rawToken;
        lexer.consume();
        if (lexer.curToken.rawToken != ":") {
            return logError("Expected : after variable name");
        }
        lexer.consume();
        auto type = parseType(lexer);
        if (!type) {
            return nullptr;
        }

        fields.push_back(std::make_tuple(identifier, type));
    }
    lexer.consume();

    return std::make_unique<StructAST>(structIdentifier, std::move(fields));
}

// var assignments are indistinguishable from expressions until the : or varop.
// this is downstream of not making var declarations expressions.
bool isVarAssignment(Lexer& lexer) {
    int peeking = 0;
    while (lexer.peek(peeking).type == TOK_IDENTIFIER) {
        if (lexer.peek(peeking + 1).rawToken == ":" || lexer.peek(peeking + 1).type == TOK_VAROP) {
            return true;
        } else if (lexer.peek(peeking + 1).rawToken != ".") {
            return false;
        }
        peeking += 2;
    }
    return false;
}

std::unique_ptr<StatementAST> parseStatement(Lexer& lexer) {
    std::unique_ptr<StatementAST> statement;

    if (lexer.curToken.rawToken == KW_FUNC || lexer.curToken.rawToken == KW_NATIVE) {
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
        return logError("Expected delimiter after statement, got " + lexer.curToken.rawToken);
    }

    return statement;
}

std::unique_ptr<TopLevelAST> parseTopLevel(Lexer& lexer) {
    std::unique_ptr<TopLevelAST> statement;

    if (lexer.curToken.rawToken == KW_FUNC || lexer.curToken.rawToken == KW_NATIVE) {
        statement = parseFunc(lexer);
    } else if (lexer.curToken.rawToken == KW_STRUCT) {
        statement = parseStruct(lexer);
    } else if (lexer.curToken.rawToken == KW_FROM) {
        statement = parseImport(lexer);
    } else {
        return logError("Expected top level statement, got " + lexer.curToken.rawToken);
    }

    if (!statement) {
        return nullptr;
    } else if (lexer.curToken.type != TOK_DELIMITER) {
        return logError("Expected delimiter after statement, got " + lexer.curToken.rawToken);
    }

    return statement;
}

std::unique_ptr<BlockAST> parseBlock(Lexer& lexer) {
    if (lexer.curToken.rawToken != "{") {
        return logError("Expected {");
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

    return std::make_unique<BlockAST>(std::move(statements));
}

std::unique_ptr<UnitAST> parseUnit(Lexer& lexer, const std::string& unit) {
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

    auto unitAst = std::make_unique<UnitAST>(unit, std::move(statements));
    if constexpr (DEBUG_AST_PRINT_UNIT) {
        std::cout << unitAst->toString() << std::endl;
    }
    return unitAst;
}
