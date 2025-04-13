#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <lexer/lexer.h>


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

    std::string toString() override;
};

// expr
class IdentifierExprAST : public ExprAST {
    std::string identifier;

public:
    explicit IdentifierExprAST(std::string identifier): identifier(std::move(identifier)) {
    }

    std::string toString() override;
};

class ValueExprAST : public ExprAST {
    double value;

public:
    explicit ValueExprAST(double value): value(value) {
    }

    std::string toString() override;
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

    std::string toString() override;
};

class UnaryOpExprAST : public ExprAST {
    std::unique_ptr<ExprAST> expr;
    std::string unaryOp;

public:
    explicit UnaryOpExprAST(std::unique_ptr<ExprAST> expr, std::string unaryOp): expr(std::move(expr)),
        unaryOp(std::move(unaryOp)) {
    }

    std::string toString() override;
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

    std::string toString() override;
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

    std::string toString() override;
};

class BlockAST;

struct SigArg {
    std::unique_ptr<TypeAST> type;
    std::unique_ptr<IdentifierExprAST> identifier;

    std::string toString();
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

    std::string toString() override;
};

// parsing funcs

std::unique_ptr<ExprAST> parseExpr(Lexer &lexer);

std::unique_ptr<ExprAST> parseExprNoBinop(Lexer &lexer);

std::unique_ptr<IfAST> parseIf(Lexer &lexer, bool onIf = true);

std::unique_ptr<WhileAST> parseWhile(Lexer &lexer);

std::unique_ptr<StatementAST> parseVarOrCall(Lexer &lexer);

std::unique_ptr<FuncAST> parseFunc(Lexer &lexer);

std::unique_ptr<StatementAST> parseStatement(Lexer &lexer);

std::unique_ptr<BlockAST> parseBlock(Lexer &lexer, bool topLevel = false);
