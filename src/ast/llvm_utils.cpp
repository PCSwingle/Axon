#include <vector>

#include <llvm/IR/Constants.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>

#include "llvm_utils.h"
#include "module/module_state.h"

using namespace llvm;

std::vector<Value*> createFieldIndices(const ModuleState& state, const int index) {
    return std::vector<Value*>{
        ConstantInt::get(*state.ctx, APInt(32, 0)),
        ConstantInt::get(*state.ctx, APInt(32, index))
    };
}

std::string typeToString(const Type* type) {
    std::string S;
    raw_string_ostream OS(S);
    type->print(OS);
    return OS.str();
}

// This was (mostly) copied from IRBuidler's CreateMallocCall function
CallInst* createMalloc(ModuleState& state, Value* allocSize, const std::string& name) {
    assert(allocSize->getType() == state.intPtrTy && "malloc size is wrong type");

    // TODO: free!!!!
    // TODO: insert function in state and store it there (w/ module option for nomalloc / nostdlib)
    // void* malloc(size_t amount);
    auto mallocFunc = state.module->getOrInsertFunction("malloc", PointerType::getUnqual(*state.ctx), state.intPtrTy);
    auto mallocCall = state.builder->CreateCall(mallocFunc, allocSize, name + "_malloc");
    mallocCall->setTailCall();
    if (Function* F = dyn_cast<Function>(mallocFunc.getCallee())) {
        mallocCall->setCallingConv(F->getCallingConv());
        F->setReturnDoesNotAlias();
    }

    assert(!mallocCall->getType()->isVoidTy() && "Malloc has void return type");
    return mallocCall;
}
