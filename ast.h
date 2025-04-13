#pragma once
#include <memory>
#include <optional>
#include <sstream>

#include "lexer.h"
#include "logging.h"


// abstracts
class AST {
public:
    virtual ~AST() = default;

    virtual std::string toString() = 0;
};

class ExprAST : public AST {
};

class StatementAST : public AST {
};

// type
class TypeAST : public AST {
    std::string type;

public:
    explicit TypeAST(std::string type): type(std::move(type)) {
    }

    std::string toString() override {
        return "Type(" + type + ")";
    }
};

// expr
class IdentifierExprAST : public ExprAST {
    std::string identifier;

public:
    explicit IdentifierExprAST(std::string identifier): identifier(std::move(identifier)) {
    }

    std::string toString() override {
        return "Identifier(" + identifier + ")";
    }
};

class ValueExprAST : public ExprAST {
    double value;

public:
    explicit ValueExprAST(double value): value(value) {
    }

    std::string toString() override {
        return "Value(" + std::to_string(value) + ")";
    }
};

class BinaryOpExprAST : public ExprAST {
    std::unique_ptr<ExprAST> LHS;
    std::unique_ptr<ExprAST> RHS;
    std::string binOp;

public:
    explicit BinaryOpExprAST(std::unique_ptr<ExprAST> LHS,
                             std::unique_ptr<ExprAST> RHS,
                             std::string binOp): LHS(std::move(LHS)), RHS(std::move(RHS)), binOp(std::move(binOp)) {
    }

    std::string toString() override {
        return "BinOp(" + LHS->toString() + " " + binOp + " " + RHS->toString() + ")";
    }
};

class UnaryOpExprAST : public ExprAST {
    std::unique_ptr<ExprAST> expr;
    std::string unaryOp;

public:
    explicit UnaryOpExprAST(std::unique_ptr<ExprAST> expr, std::string unaryOp): expr(std::move(expr)),
        unaryOp(std::move(unaryOp)) {
    }

    std::string toString() override {
        return "UnOp(" + unaryOp + expr->toString() + ")";
    }
};

// dual expression / statements
class CallExprAST : public ExprAST, public StatementAST {
    std::unique_ptr<IdentifierExprAST> callName;
    std::vector<std::unique_ptr<ExprAST> > args;

public:
    explicit CallExprAST(std::unique_ptr<IdentifierExprAST> callName,
                         std::vector<std::unique_ptr<ExprAST> > args): callName(std::move(callName)),
                                                                       args(std::move(args)) {
    }

    std::string toString() override {
        std::ostringstream result;
        for (size_t i = 0; i < args.size(); i++) {
            result << args[i]->toString();
            if (i != args.size() - 1) {
                result << ", ";
            }
        }
        return "Call(" + callName->toString() + "(" + result.str() + "))";
    }
};

// statements
class VarAST : public StatementAST {
    std::optional<std::unique_ptr<TypeAST> > type;
    std::unique_ptr<IdentifierExprAST> identifier;
    std::unique_ptr<ExprAST> expr;

public:
    explicit VarAST(std::optional<std::unique_ptr<TypeAST> > type,
                    std::unique_ptr<IdentifierExprAST> identifier,
                    std::unique_ptr<ExprAST> expr): type(std::move(type)),
                                                    identifier(std::move(identifier)),
                                                    expr(std::move(expr)) {
    }

    std::string toString() override {
        return "Var(" + (type.has_value() ? type.value()->toString() + " " : "") + identifier->toString() + " = " + expr
               ->toString()
               + ")";
    }
};

class BlockAST;

struct SigArg {
    std::unique_ptr<TypeAST> type;
    std::unique_ptr<IdentifierExprAST> identifier;

    std::string toString() {
        return type->toString() + " " + identifier->toString();
    }
};

class FuncAST : public StatementAST {
    std::unique_ptr<TypeAST> type;
    std::unique_ptr<IdentifierExprAST> funcName;
    std::vector<SigArg> signature;
    std::unique_ptr<BlockAST> block;

public:
    explicit FuncAST(std::unique_ptr<TypeAST> type,
                     std::unique_ptr<IdentifierExprAST> funcName,
                     std::vector<SigArg>
                     signature,
                     std::unique_ptr<BlockAST> block): type(std::move(type)),
                                                       funcName(std::move(funcName)),
                                                       signature(std::move(signature)),
                                                       block(std::move(block)) {
    }

    std::string toString() override;
};

class IfAST : public StatementAST {
    std::unique_ptr<ExprAST> expr;
    std::unique_ptr<BlockAST> block;
    std::optional<std::unique_ptr<BlockAST> > elseBlock;

public:
    explicit IfAST(std::unique_ptr<ExprAST> expr,
                   std::unique_ptr<BlockAST> block,
                   std::optional<std::unique_ptr<BlockAST> > elseBlock
    ): expr(std::move(expr)), block(std::move(block)), elseBlock(std::move(elseBlock)) {
    }

    std::string toString() override;
};

class WhileAST : public StatementAST {
    std::unique_ptr<ExprAST> expr;
    std::unique_ptr<BlockAST> block;

public:
    explicit WhileAST(std::unique_ptr<ExprAST> expr,
                      std::unique_ptr<BlockAST> block
    ): expr(std::move(expr)), block(std::move(block)) {
    }

    std::string toString() override;
};

// block
class BlockAST : public AST {
    std::vector<std::unique_ptr<StatementAST> > statements;

public:
    explicit BlockAST(std::vector<std::unique_ptr<StatementAST> > statements): statements(std::move(statements)) {
    }

    std::string toString() override {
        std::string indent(2, ' ');
        std::ostringstream result;
        for (size_t i = 0; i < statements.size(); i++) {
            result << statements[i]->toString();
            if (i != statements.size() - 1) {
                result << ";\n";
            }
        }

        auto final = indent;
        auto blockStr = result.str();
        size_t pos = 0;
        size_t found;
        while ((found = blockStr.find('\n', pos)) != std::string::npos) {
            final += blockStr.substr(pos, found - pos + 1) + indent;
            pos = found + 1;
        }
        final += blockStr.substr(pos);
        return "Block {\n" + final + "\n}";
    }
};

std::string FuncAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < signature.size(); i++) {
        result << signature[i].toString();
        if (i != signature.size() - 1) {
            result << ", ";
        }
    }
    return "Func(" + type->toString() + " " + funcName->toString() + "(" + result.str() + ")) " + block->toString();
}

std::string IfAST::toString() {
    return "If(" + expr->toString() + ") " + block->toString();
}

std::string WhileAST::toString() {
    return "While(" + expr->toString() + ") " + block->toString();
}

// parsing funcs
std::unique_ptr<BlockAST> parseBlock(Lexer &lexer, bool topLevel = false);

std::unique_ptr<ExprAST> parseExpr(Lexer &lexer);

std::unique_ptr<ExprAST> parseExprNoBinop(Lexer &lexer) {
    // doesn't check for binops; use parseExpr

    // values
    if (lexer.curToken.type == TOK_VALUE) {
        // TODO: accept ints
        double value = strtod(lexer.curToken.rawToken.c_str(), nullptr);
        lexer.consume();
        return std::make_unique<ValueExprAST>(value);
    }

    // identifiers and calls
    if (lexer.curToken.type == TOK_IDENTIFIER) {
        auto identifier = std::make_unique<IdentifierExprAST>(lexer.curToken.rawToken);
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
            return identifier;
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

std::unique_ptr<ExprAST> parseExpr(Lexer &lexer) {
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

        while (opStack.size() > 0 && BINOPS[op] <= BINOPS[opStack.back()]) {
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

std::unique_ptr<IfAST> parseIf(Lexer &lexer, bool onIf = true) {
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

std::unique_ptr<WhileAST> parseWhile(Lexer &lexer) {
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

std::unique_ptr<StatementAST> parseVarOrCall(Lexer &lexer) {
    std::optional<std::unique_ptr<TypeAST> > type;
    if (lexer.curToken.type == TOK_TYPE) {
        type = std::make_unique<TypeAST>(lexer.curToken.rawToken);
        lexer.consume();
    }

    if (lexer.curToken.type != TOK_IDENTIFIER) {
        return logError("Expected variable or function call identifier");
    }
    auto identifier = std::make_unique<IdentifierExprAST>(lexer.curToken.rawToken);
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

std::unique_ptr<FuncAST> parseFunc(Lexer &lexer) {
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
    auto funcName = std::make_unique<IdentifierExprAST>(lexer.curToken.rawToken);
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
        auto identifier = std::make_unique<IdentifierExprAST>(lexer.curToken.rawToken);
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

std::unique_ptr<StatementAST> parseStatement(Lexer &lexer) {
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


std::unique_ptr<BlockAST> parseBlock(Lexer &lexer, bool topLevel) {
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
