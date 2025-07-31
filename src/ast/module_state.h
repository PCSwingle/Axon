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

    void enterScope() {
        scopeStack.push_back(std::vector<std::string>());
    }

    void exitScope() {
        for (auto const& identifier: scopeStack.back()) {
            identifiers.erase(identifier);
        }
        scopeStack.pop_back();
    }

    bool registerIdentifier(const std::string& identifier, Identifier val) {
        if (identifiers.find(identifier) != identifiers.end()) {
            return false;
        }
        identifiers.insert_or_assign(identifier, val);
        scopeStack.back().push_back(identifier);
        return true;
    }

    // TODO: return the identifier instance instead
    AllocaInst* registerVar(std::string& identifier, TypeAST* type) {
        auto* varAlloca = createAlloca(type, identifier);
        if (!registerIdentifier(identifier, VariableIdentifier(varAlloca))) {
            return nullptr;
        }
        return varAlloca;
    }

    AllocaInst* getVar(std::string& identifier) {
        if (identifiers.find(identifier) == identifiers.end()) {
            return nullptr;
        }
        auto& val = identifiers.at(identifier);
        auto* varAlloca = std::get_if<VariableIdentifier>(&val);
        if (!varAlloca) {
            return nullptr;
        }
        return varAlloca->varAlloca;
    }

    Function* registerFunction(std::string& identifier, FunctionType* type) {
        auto* function = Function::Create(type, Function::ExternalLinkage, identifier, module.get());
        if (!registerIdentifier(identifier, FunctionIdentifier(function))) {
            return nullptr;
        }
        return function;
    }

    Function* getFunction(std::string& identifier) {
        if (identifiers.find(identifier) == identifiers.end()) {
            return nullptr;
        }
        auto& val = identifiers.at(identifier);
        auto* function = std::get_if<FunctionIdentifier>(&val);
        if (!function) {
            return nullptr;
        }
        return function->function;
    }

    StructType* registerStruct(std::string& identifier,
                               std::vector<std::tuple<std::string, std::unique_ptr<TypeAST> > > fields) {
        auto* elements = new std::vector<Type*>();
        for (auto [fieldName, fieldType]: fields) {
            elements->push_back(fieldType->getType(*this));
        }
        auto* structType = StructType::get(*ctx, elements);

        if (!registerIdentifier(identifier, StructIdentifier(structType, fields))) {
            return nullptr;
        }
        return structType;
    }

    StructType* getStruct(std::string& identifier) {
        if (identifiers.find(identifier) == identifiers.end()) {
            return nullptr;
        }
        auto& val = identifiers.at(identifier);
        auto* structIdentifier = std::get_if<StructIdentifier>(&val);
        if (!structIdentifier) {
            return nullptr;
        }
        return structIdentifier->structType;
    }
};
