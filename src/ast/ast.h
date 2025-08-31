#pragma once

#include <memory>
#include <optional>
#include <ranges>
#include <vector>
#include <unordered_map>

namespace llvm {
    class Value;
    class Type;
}

using namespace llvm;

struct GeneratedType;
struct GeneratedValue;

class ModuleState;

class BlockAST;

struct DebugInfo {
    int statementStartToken;

    int startToken;
    int endToken;
};

// abstracts
class AST {
public:
    virtual ~AST() = default;

    virtual std::string toString() = 0;

    virtual bool codegen(ModuleState& state) = 0;

    DebugInfo debugInfo;

    void setDebugInfo(const DebugInfo& debugInfo) {
        this->debugInfo = debugInfo;
    }
};

class TopLevelAST : virtual public AST {
public:
    // Registers this statement on an the global, importable level
    virtual bool preregister(ModuleState& state, const std::string& unit) = 0;

    // Registers this statement locally at start of unit compilation to allow antecedent referencing
    virtual bool postregister(ModuleState& state, const std::string& unit) = 0;
};

class StatementAST : virtual public AST {
};

class ExprAST : virtual public StatementAST {
public:
    bool codegen(ModuleState& state) override;

    // impliedType is necessary for empty arrays and number values, but we don't do any checking on it
    // (it is purely for implicit casts, which should also be relatively rare)
    virtual std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) = 0;
};

class AssignableAST : virtual public ExprAST {
public:
    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) override;

    virtual std::unique_ptr<GeneratedValue> codegenPointer(ModuleState& state) = 0;
};

// expr
class ValueExprAST : public ExprAST {
    std::string rawValue;

public:
    explicit ValueExprAST(std::string rawValue): rawValue(std::move(rawValue)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) override;
};

// TODO: rename this (since it encapsulates functions as well
class VariableExprAST : public AssignableAST {
public:
    std::string varName;

    explicit VariableExprAST(std::string varName): varName(std::move(varName)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenPointer(ModuleState& state) override;
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

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) override;
};

class UnaryOpExprAST : public ExprAST {
    std::unique_ptr<ExprAST> expr;
    std::string unaryOp;

public:
    explicit UnaryOpExprAST(std::unique_ptr<ExprAST> expr, std::string unaryOp): expr(std::move(expr)),
        unaryOp(std::move(unaryOp)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) override;
};

class CallExprAST : public ExprAST {
    std::unique_ptr<ExprAST> callee;
    std::vector<std::unique_ptr<ExprAST> > args;

public:
    explicit CallExprAST(std::unique_ptr<ExprAST> callee,
                         std::vector<std::unique_ptr<ExprAST> > args): callee(std::move(callee)),
                                                                       args(std::move(args)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) override;
};

class MemberAccessExprAST : public AssignableAST {
    std::unique_ptr<ExprAST> structExpr;
    std::string fieldName;

public:
    explicit MemberAccessExprAST(std::unique_ptr<ExprAST> structExpr,
                                 std::string fieldName): structExpr(std::move(structExpr)),
                                                         fieldName(std::move(fieldName)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenPointer(ModuleState& state) override;
};

class SubscriptExprAST : public AssignableAST {
    std::unique_ptr<ExprAST> arrayExpr;
    std::unique_ptr<ExprAST> indexExpr;

public:
    explicit SubscriptExprAST(std::unique_ptr<ExprAST> arrayExpr,
                              std::unique_ptr<ExprAST> indexExpr): arrayExpr(std::move(arrayExpr)),
                                                                   indexExpr(std::move(indexExpr)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenPointer(ModuleState& state) override;
};

class ConstructorExprAST : public ExprAST {
    GeneratedType* type;
    std::unordered_map<std::string, std::unique_ptr<ExprAST> > values;

public:
    explicit ConstructorExprAST(GeneratedType* type,
                                std::unordered_map<std::string, std::unique_ptr<ExprAST> >
                                values): type(std::move(type)), values(std::move(values)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) override;
};

class ArrayExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST> > values;

public:
    explicit ArrayExprAST(std::vector<std::unique_ptr<ExprAST> > values): values(std::move(values)) {
    }

    std::string toString() override;

    std::unique_ptr<GeneratedValue> codegenValue(ModuleState& state, GeneratedType* impliedType) override;
};

// top level
class ImportAST : public TopLevelAST {
    std::string unit;
    // identifier -> alias
    std::unordered_map<std::string, std::string> aliases;

public:
    explicit ImportAST(std::string unit,
                       std::unordered_map<std::string, std::string> aliases): unit(std::move(
                                                                                  unit)),
                                                                              aliases(std::move(aliases)) {
    }

    std::string toString() override;

    bool preregister(ModuleState& state, const std::string& unit) override;

    bool postregister(ModuleState& state, const std::string& unit) override;

    bool codegen(ModuleState& state) override;
};

struct SigArg {
    GeneratedType* type;
    std::string identifier;

    std::string toString();
};

class FuncAST : public TopLevelAST, public StatementAST {
    std::vector<SigArg> signature;
    GeneratedType* returnType;
    std::optional<std::unique_ptr<BlockAST> > block;

    std::shared_ptr<GeneratedValue> declaration = nullptr;

public:
    std::string funcName;
    bool isExtern;
    // Only allowed for extern functions!
    bool hasVarArgs;

    explicit FuncAST(
        std::vector<SigArg> signature,
        GeneratedType* returnType,
        std::optional<std::unique_ptr<BlockAST> > block,
        std::string funcName,
        const bool isExtern,
        const bool hasVarArgs): signature(std::move(signature)),
                                returnType(returnType),
                                block(std::move(block)),
                                funcName(std::move(funcName)),
                                isExtern(isExtern),
                                hasVarArgs(hasVarArgs) {
    }

    std::string toString() override;

    // function twine is _only_ used to interface with external libraries. Otherwise, twine should simply be unique.
    // used to create an llvm function declaration, but NOT put into scope.
    std::shared_ptr<GeneratedValue> declare(const ModuleState& state, const std::string& twine);

    bool preregister(ModuleState& state, const std::string& unit) override;

    bool postregister(ModuleState& state, const std::string& unit) override;

    bool codegen(ModuleState& state) override;
};

class StructAST : public TopLevelAST {
    std::string structName;
    std::vector<std::tuple<std::string, GeneratedType*> > fields;
    std::unordered_map<std::string, std::unique_ptr<FuncAST> > methods;

public:
    explicit StructAST(std::string structName,
                       std::vector<std::tuple<std::string, GeneratedType*> > fields,
                       std::unordered_map<std::string, std::unique_ptr<FuncAST> > methods
    ): structName(std::move(structName)), fields(std::move(fields)), methods(std::move(methods)) {
    }

    std::string toString() override;

    bool preregister(ModuleState& state, const std::string& unit) override;

    bool postregister(ModuleState& state, const std::string& unit) override;

    bool codegen(ModuleState& state) override;
};

// statements
class VarAST : public TopLevelAST, public StatementAST {
    bool definition;
    std::unique_ptr<AssignableAST> variableExpr;
    std::optional<GeneratedType*> type;
    std::string varOp;
    std::unique_ptr<ExprAST> expr;

public:
    explicit VarAST(const bool definition,
                    std::unique_ptr<AssignableAST> variableExpr,
                    std::optional<GeneratedType*> type,
                    std::string varOp,
                    std::unique_ptr<ExprAST> expr): definition(definition),
                                                    variableExpr(std::move(variableExpr)),
                                                    type(std::move(type)),
                                                    varOp(std::move(varOp)),
                                                    expr(std::move(expr)) {
    }

    std::string toString() override;

    bool preregister(ModuleState& state, const std::string& unit) override;

    bool postregister(ModuleState& state, const std::string& unit) override;

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

// other
class BlockAST : public AST {
    std::vector<std::unique_ptr<StatementAST> > statements;

public:
    explicit BlockAST(std::vector<std::unique_ptr<StatementAST> > statements): statements(std::move(statements)) {
    }

    std::string toString() override;

    bool codegen(ModuleState& state) override;
};

class UnitAST : public AST {
    std::string unit;
    std::vector<std::unique_ptr<TopLevelAST> > statements;

public:
    explicit UnitAST(const std::string& unit, std::vector<std::unique_ptr<TopLevelAST> > statements): unit(unit),
        statements(std::move(statements)) {
    }

    std::string toString() override;

    bool preregisterUnit(ModuleState& state);

    bool codegen(ModuleState& state) override;
};

// parsing funcs
class Lexer;

GeneratedType* parseType(Lexer& lexer);

std::unique_ptr<ExprAST> parseExpr(Lexer& lexer);

std::unique_ptr<IfAST> parseIf(Lexer& lexer, bool onIf = true);

std::unique_ptr<WhileAST> parseWhile(Lexer& lexer);

std::unique_ptr<VarAST> parseVar(Lexer& lexer);

std::unique_ptr<CallExprAST> parseCall(Lexer& lexer, std::unique_ptr<ExprAST> callee);

template<std::derived_from<ExprAST> T>
std::unique_ptr<T> parseAccessor(Lexer& lexer, std::unique_ptr<T> expr);

std::unique_ptr<ConstructorExprAST> parseConstructor(Lexer& lexer);

std::unique_ptr<ArrayExprAST> parseArray(Lexer& lexer);

std::unique_ptr<ImportAST> parseImport(Lexer& lexer);

std::unique_ptr<FuncAST> parseFunc(Lexer& lexer);

std::unique_ptr<StructAST> parseStruct(Lexer& lexer);

std::unique_ptr<StatementAST> parseStatement(Lexer& lexer);

std::unique_ptr<TopLevelAST> parseTopLevel(Lexer& lexer);

std::unique_ptr<BlockAST> parseBlock(Lexer& lexer);

std::unique_ptr<UnitAST> parseUnit(Lexer& lexer, const std::string& unit);
