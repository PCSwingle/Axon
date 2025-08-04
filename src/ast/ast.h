#pragma once
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

namespace llvm {
    class Value;
    class Type;
}

using namespace llvm;

struct GeneratedType;
struct GeneratedValue;

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
    bool codegen(ModuleState& state) override;

    virtual std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) = 0;
};

// expr
class ValueExprAST : public ExprAST {
    std::string rawValue;

public:
    explicit ValueExprAST(std::string rawValue): rawValue(std::move(rawValue)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) override;
};

class VariableExprAST : public ExprAST {
    std::string varName;

public:
    explicit VariableExprAST(std::string varName): varName(std::move(varName)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) override;
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

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) override;
};

class UnaryOpExprAST : public ExprAST {
    std::unique_ptr<ExprAST> expr;
    std::string unaryOp;

public:
    explicit UnaryOpExprAST(std::unique_ptr<ExprAST> expr, std::string unaryOp): expr(std::move(expr)),
        unaryOp(std::move(unaryOp)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) override;
};

class CallExprAST : public ExprAST {
    std::string callName;
    std::vector<std::unique_ptr<ExprAST> > args;

public:
    explicit CallExprAST(std::string callName,
                         std::vector<std::unique_ptr<ExprAST> > args): callName(std::move(callName)),
                                                                       args(std::move(args)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) override;
};

class AccessorExprAST : public ExprAST {
    std::unique_ptr<ExprAST> structExpr;
    std::string fieldName;

public:
    explicit AccessorExprAST(std::unique_ptr<ExprAST> structExpr,
                             std::string fieldName): structExpr(std::move(structExpr)),
                                                     fieldName(std::move(fieldName)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) override;
};

class ConstructorExprAST : public ExprAST {
    std::string structName;
    std::unordered_map<std::string, std::unique_ptr<ExprAST> > values;

public:
    explicit ConstructorExprAST(std::string structName,
                                std::unordered_map<std::string, std::unique_ptr<ExprAST> >
                                values): structName(std::move(structName)), values(std::move(values)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state) override;
};

// statements

class VarAST : public StatementAST {
    bool definition;
    std::string identifier;
    std::vector<std::string> fieldNames;
    std::optional<GeneratedType*> type;
    std::unique_ptr<ExprAST> expr;

public:
    explicit VarAST(const bool definition,
                    std::string identifier,
                    std::vector<std::string> fieldNames,
                    std::optional<GeneratedType*> type,
                    std::unique_ptr<ExprAST> expr): definition(definition),
                                                    identifier(std::move(identifier)),
                                                    fieldNames(std::move(fieldNames)),
                                                    type(std::move(type)),
                                                    expr(std::move(expr)) {
    }

    std::string toString() override;

    bool codegen(ModuleState& state) override;
};

class BlockAST;

struct SigArg {
    GeneratedType* type;
    std::string identifier;

    std::string toString();
};

class FuncAST : public StatementAST {
    std::string funcName;
    std::vector<SigArg> signature;
    GeneratedType* returnType;
    std::optional<std::unique_ptr<BlockAST> > block;
    bool native;

public:
    explicit FuncAST(
        std::string funcName,
        std::vector<SigArg>
        signature,
        GeneratedType* returnType,
        std::optional<std::unique_ptr<BlockAST> > block,
        const bool native): funcName(std::move(funcName)),
                            signature(std::move(signature)),
                            returnType(returnType),
                            block(std::move(block)),
                            native(native) {
    }

    std::string toString() override;

    bool codegen(ModuleState& state) override;
};

class StructAST : public StatementAST {
    std::string structName;
    std::vector<std::tuple<std::string, GeneratedType*> > fields;

public:
    explicit StructAST(std::string structName,
                       std::vector<std::tuple<std::string, GeneratedType*> > fields): structName(
            std::move(structName)),
        fields(std::move(fields)) {
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

std::unique_ptr<IfAST> parseIf(Lexer& lexer, bool onIf = true);

std::unique_ptr<WhileAST> parseWhile(Lexer& lexer);

std::unique_ptr<VarAST> parseVar(Lexer& lexer);

std::unique_ptr<CallExprAST> parseCall(Lexer& lexer);

std::unique_ptr<ConstructorExprAST> parseConstructor(Lexer& lexer);

std::unique_ptr<FuncAST> parseFunc(Lexer& lexer);

std::unique_ptr<StructAST> parseStruct(Lexer& lexer);

std::unique_ptr<StatementAST> parseStatement(Lexer& lexer);

std::unique_ptr<BlockAST> parseBlock(Lexer& lexer, bool topLevel = false);
