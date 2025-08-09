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
