#include <llvm/IR/DerivedTypes.h>

#include "ast.h"
#include "generated.h"
#include "logging.h"
#include "module_state.h"

using namespace llvm;

bool StructAST::preregister(ModuleState& state) {
    if (!state.registerStruct(structName, fields)) {
        logError("duplicate identifier definition " + structName);
        return false;
    }
    return true;
}

bool FuncAST::preregister(ModuleState& state) {
    std::vector<Type*> argTypes;
    for (const auto& [type, identifier]: signature) {
        argTypes.push_back(type->getLLVMType(state));
    }
    FunctionType* type = FunctionType::get(returnType->getLLVMType(state), argTypes, false);
    if (!state.registerFunction(funcName, signature, returnType, type)) {
        logError("duplicate function definition " + funcName);
        return false;
    }
    return true;
}


bool UnitAST::registerUnit(ModuleState& state) {
    for (const auto& statement: statements) {
        if (!statement->preregister(state)) {
            return false;
        }
    }
    return true;
}
