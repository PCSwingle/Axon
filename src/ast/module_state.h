#pragma once
#include <variant>
#include <logging.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include "ast.h"

using namespace llvm;

struct TypeAST;

struct VariableIdentifier {
    AllocaInst* varAlloca;

    explicit VariableIdentifier(AllocaInst* varAlloca): varAlloca(varAlloca) {
    }
};

struct FunctionIdentifier {
    Function* function;

    explicit FunctionIdentifier(Function* function): function(function) {
    }
};

struct StructIdentifier {
    StructType* structType;
    std::vector<std::tuple<std::string, std::unique_ptr<TypeAST> > > fields;

    explicit StructIdentifier(StructType* structType,
                              std::vector<std::tuple<std::string, std::unique_ptr<TypeAST> > >
                              fields): structType(structType),
                                       fields(move(fields)) {
    }
};

typedef std::variant<VariableIdentifier, FunctionIdentifier, StructIdentifier> Identifier;

class ModuleState {
    AllocaInst* createAlloca(TypeAST* type, std::string& name);

public:
    // TODO: should context be static?
    std::unique_ptr<LLVMContext> ctx;
    std::unique_ptr<IRBuilder<> > builder;
    std::unique_ptr<Module> module;
    std::unordered_map<std::string, Identifier> identifiers;

    std::vector<std::vector<std::string> > scopeStack;

    ModuleState() {
        ctx = std::make_unique<LLVMContext>();
        module = std::make_unique<Module>("axon main module", *ctx);
        builder = std::make_unique<IRBuilder<> >(*ctx);
        scopeStack.push_back(std::vector<std::string>());
    }

    bool writeIR(const std::string& filename, bool bitcode = true);

    void enterScope();

    void exitScope();

    bool registerIdentifier(const std::string& identifier, Identifier val);

    AllocaInst* registerVar(std::string& identifier, TypeAST* type);

    AllocaInst* getVar(std::string& identifier);

    Function* registerFunction(std::string& identifier, FunctionType* type);

    Function* getFunction(std::string& identifier);

    StructType* registerStruct(std::string& identifier,
                               std::vector<std::tuple<std::string, std::unique_ptr<TypeAST> > > fields);

    StructType* getStruct(std::string& identifier);
};
