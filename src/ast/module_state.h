#pragma once
#include <logging.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include "ast.h"

using namespace llvm;

struct TypeAST;

class ModuleState {
    AllocaInst* createAlloca(TypeAST* type, std::string& name);

public:
    // TOOD: should context be static?
    std::unique_ptr<LLVMContext> ctx;
    std::unique_ptr<IRBuilder<> > builder;
    std::unique_ptr<Module> module;
    std::unordered_map<std::string, AllocaInst*> vars;

    std::vector<std::vector<std::string> > scopeStack;

    ModuleState() {
        ctx = std::make_unique<LLVMContext>();
        module = std::make_unique<Module>("axon main module", *ctx);
        builder = std::make_unique<IRBuilder<> >(*ctx);
    }

    bool writeIR(const std::string& filename, bool bitcode = true);

    void enterScope() {
        scopeStack.push_back(std::vector<std::string>());
    }

    void exitScope() {
        for (auto const& var: scopeStack.back()) {
            vars.erase(var);
        }
        scopeStack.pop_back();
    }

    bool addVar(std::string var, TypeAST* type) {
        if (vars.find(var) != vars.end()) {
            logError("duplicate variable definition " + var);
            return false;
        }
        auto* alloca = createAlloca(type, var);
        vars[var] = alloca;
        scopeStack.back().push_back(var);
        return true;
    }
};
