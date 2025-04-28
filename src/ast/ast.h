#pragma once
#include <memory>
#include <optional>
#include <vector>

#include "module_state.h"

namespace llvm {
    class Value;
    class Type;
}

class ModuleState;

// abstracts

class AST {
public:
    virtual ~AST() = default;

    virtual std::string toString() = 0;
};

class StatementAST : public AST {
public:
    virtual bool codegen(ModuleState& state) = 0;
};

class ExprAST : public StatementAST {
public:
    bool codegen(ModuleState& state) override {
        if (!codegenValue(state)) {
            return false;
        } else {
            return true;
        }
    }

    virtual llvm::Value* codegenValue(ModuleState& state) = 0;
};

// structs
struct TypeAST {
    std::string type;

    explicit TypeAST(std::string type): type(std::move(type)) {
    }

    std::string toString();

    llvm::Type* getType(ModuleState& state);
};

struct IdentifierAST {
    std::string identifier;

    explicit IdentifierAST(std::string identifier): identifier(std::move(identifier)) {
    }

    std::string toString();
};

// expr
class ValueExprAST : public ExprAST {
    std::string rawValue;

public:
    explicit ValueExprAST(std::string rawValue): rawValue(rawValue) {
    }

    std::string toString() override;

    llvm::Value* codegenValue(ModuleState& state) override;
};

class VariableExprAST : public ExprAST {
    std::unique_ptr<IdentifierAST> varName;

public:
    explicit VariableExprAST(std::unique_ptr<IdentifierAST> varName): varName(std::move(varName)) {
    }

    std::string toString() override;

    llvm::Value* codegenValue(ModuleState& state) override;
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

    llvm::Value* codegenValue(ModuleState& state) override;
};

class UnaryOpExprAST : public ExprAST {
    std::unique_ptr<ExprAST> expr;
    std::string unaryOp;

public:
    explicit UnaryOpExprAST(std::unique_ptr<ExprAST> expr, std::string unaryOp): expr(std::move(expr)),
        unaryOp(std::move(unaryOp)) {
    }

    std::string toString() override;

    llvm::Value* codegenValue(ModuleState& state) override;
};

class CallExprAST : public ExprAST {
    std::unique_ptr<IdentifierAST> callName;
    std::vector<std::unique_ptr<ExprAST> > args;

public:
    explicit CallExprAST(std::unique_ptr<IdentifierAST> callName,
                         std::vector<std::unique_ptr<ExprAST> > args): callName(std::move(callName)),
                                                                       args(std::move(args)) {
    }

    std::string toString() override;

    llvm::Value* codegenValue(ModuleState& state) override;
};

// statements
class VarAST : public StatementAST {
    std::optional<std::unique_ptr<TypeAST> > type;
    std::unique_ptr<IdentifierAST> identifier;
    std::unique_ptr<ExprAST> expr;

public:
    explicit VarAST(std::optional<std::unique_ptr<TypeAST> > type,
                    std::unique_ptr<IdentifierAST> identifier,
                    std::unique_ptr<ExprAST> expr): type(std::move(type)),
                                                    identifier(std::move(identifier)),
                                                    expr(std::move(expr)) {
    }

    std::string toString() override;

    bool codegen(ModuleState& state) override;
};

class BlockAST;

struct SigArg {
    std::unique_ptr<TypeAST> type;
    std::unique_ptr<IdentifierAST> identifier;

    std::string toString();
};

class FuncAST : public StatementAST {
    std::unique_ptr<TypeAST> type;
    std::unique_ptr<IdentifierAST> funcName;
    std::vector<SigArg> signature;
    std::optional<std::unique_ptr<BlockAST> > block;
    bool native;

public:
    explicit FuncAST(std::unique_ptr<TypeAST> type,
                     std::unique_ptr<IdentifierAST> funcName,
                     std::vector<SigArg>
                     signature,
                     std::optional<std::unique_ptr<BlockAST> > block,
                     bool native): type(std::move(type)),
                                   funcName(std::move(funcName)),
                                   signature(std::move(signature)),
                                   block(std::move(block)),
                                   native(native) {
    }

    std::string toString() override;

    bool codegen(ModuleState& state) override;
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

    bool codegen(ModuleState& state) override;
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

    bool codegen(ModuleState& state) override;
};

class ReturnAST : public StatementAST {
    std::optional<std::unique_ptr<ExprAST> > returnExpr;

public:
    explicit ReturnAST(std::optional<std::unique_ptr<ExprAST> > returnExpr
    ): returnExpr(std::move(returnExpr)) {
    }

    std::string toString() override;

    bool codegen(ModuleState& state) override;
};

// block
class BlockAST : public AST {
    std::vector<std::unique_ptr<StatementAST> > statements;

public:
    explicit BlockAST(std::vector<std::unique_ptr<StatementAST> > statements): statements(std::move(statements)) {
    }

    std::string toString() override;

    bool codegen(ModuleState& state);
};

// parsing funcs
class Lexer;

std::unique_ptr<ExprAST> parseExpr(Lexer& lexer);

std::unique_ptr<ExprAST> parseExprNoBinop(Lexer& lexer);

std::unique_ptr<IfAST> parseIf(Lexer& lexer, bool onIf = true);

std::unique_ptr<WhileAST> parseWhile(Lexer& lexer);

std::unique_ptr<StatementAST> parseVarOrCall(Lexer& lexer);

std::unique_ptr<FuncAST> parseFunc(Lexer& lexer);

std::unique_ptr<StatementAST> parseStatement(Lexer& lexer);

std::unique_ptr<BlockAST> parseBlock(Lexer& lexer, bool topLevel = false);
