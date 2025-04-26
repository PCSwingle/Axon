#pragma once
#include <map>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

using namespace llvm;

class ModuleState {
public:
    // TOOD: should context be static?
    std::unique_ptr<LLVMContext> ctx;
    std::unique_ptr<IRBuilder<> > builder;
    std::unique_ptr<Module> module;
    std::map<std::string, Value*> values;

    ModuleState() {
        ctx = std::make_unique<LLVMContext>();
        module = std::make_unique<Module>("axon main module", *ctx);
        builder = std::make_unique<IRBuilder<> >(*ctx);
    }

    bool writeIR(const std::string& filename, bool bitcode = true);
};
