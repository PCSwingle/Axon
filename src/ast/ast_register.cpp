#include <llvm/IR/DerivedTypes.h>

#include "ast.h"
#include "generated.h"
#include "logging.h"
#include "module_state.h"

using namespace llvm;

bool ImportAST::preregister(ModuleState& state, const std::string& unit) {
    state.registerUnit(unit);
    return true;
}

bool ImportAST::postregister(ModuleState& state, const std::string& unit) {
    for (const auto& [identifier, alias]: aliases) {
        if (!state.useGlobalIdentifier(unit, identifier, alias)) {
            return false;
        }
    }
    return true;
}

bool FuncAST::preregister(ModuleState& state, const std::string& unit) {
    std::vector<Type*> argTypes;
    for (const auto& [type, identifier]: signature) {
        argTypes.push_back(type->getLLVMType(state));
    }
    FunctionType* type = FunctionType::get(returnType->getLLVMType(state), argTypes, false);
    if (!state.registerGlobalFunction(unit, funcName, signature, returnType, type)) {
        return false;
    }
    return true;
}

bool FuncAST::postregister(ModuleState& state, const std::string& unit) {
    return state.useGlobalIdentifier(unit, funcName, funcName);
}

bool StructAST::preregister(ModuleState& state, const std::string& unit) {
    if (!state.registerGlobalStruct(unit, structName, fields)) {
        return false;
    }
    return true;
}

bool StructAST::postregister(ModuleState& state, const std::string& unit) {
    return state.useGlobalIdentifier(unit, structName, structName);
}

bool UnitAST::preregisterUnit(ModuleState& state) {
    for (const auto& statement: statements) {
        if (!statement->preregister(state, unit)) {
            return false;
        }
    }
    return true;
}
