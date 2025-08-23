#include <llvm/IR/DerivedTypes.h>

#include "ast.h"
#include "module/generated.h"
#include "module/module_config.h"
#include "module/module_state.h"

using namespace llvm;

bool ImportAST::preregister(ModuleState& state, const std::string& unit) {
    if (!state.registerUnit(this->unit)) {
        state.setError(this->debugInfo, "Could not import unit " + unit);
        return false;
    }
    return true;
}

bool ImportAST::postregister(ModuleState& state, const std::string& unit) {
    for (const auto& [identifier, alias]: aliases) {
        if (!state.useGlobalIdentifier(this->unit, identifier, alias)) {
            state.setError(this->debugInfo, "Duplicate identifier " + identifier);
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

    std::optional<std::string> customTwine{};
    if (native || (funcName == "main" && unit == state.config.main)) {
        customTwine = funcName;
    }

    auto* function = Function::Create(type,
                                      Function::ExternalLinkage,
                                      customTwine.value_or(unit + "." + funcName),
                                      state.module.get());
    std::vector<GeneratedType*> args{};
    for (const auto& arg: signature) {
        args.push_back(arg.type);
    }
    auto functionType = std::make_tuple(args, returnType);
    if (!state.registerGlobalIdentifier(unit,
                                        funcName,
                                        std::make_unique<Identifier>(
                                            GeneratedValue(GeneratedType::get(functionType), function)))) {
        state.setError(this->debugInfo, "Duplicate identifier " + funcName);
        return false;
    }
    return true;
}

bool FuncAST::postregister(ModuleState& state, const std::string& unit) {
    if (!state.useGlobalIdentifier(unit, funcName, funcName)) {
        state.setError(this->debugInfo, "Duplicate identifier " + funcName);
        return false;
    }
    return true;
}

bool StructAST::preregister(ModuleState& state, const std::string& unit) {
    std::unordered_map<std::string, GeneratedValue*> generatedMethods;
    for (const auto& [methodName, method]: methods) {
        if (!method->preregister(state, unit)) {
            return false;
        }
        generatedMethods[methodName] = state.getVar(method->funcName);
    }

    auto elements = std::vector<Type*>();
    for (auto& [fieldName, fieldType]: fields) {
        elements.push_back(fieldType->getLLVMType(state));
    }
    // TODO: I don't think llvm does padding / alignment, so we have to do it ourselves
    auto* structType = StructType::create(*state.ctx, elements, unit + "." + structName);

    if (!state.registerGlobalIdentifier(unit,
                                        structName,
                                        std::make_unique<Identifier>(
                                            GeneratedStruct(
                                                GeneratedType::get(structName),
                                                fields,
                                                generatedMethods,
                                                structType)))) {
        state.setError(this->debugInfo, "Duplicate identifier " + structName);
        return false;
    }
    return true;
}

bool StructAST::postregister(ModuleState& state, const std::string& unit) {
    if (!state.useGlobalIdentifier(unit, structName, structName)) {
        state.setError(this->debugInfo, "Duplicate identifier " + structName);
        return false;
    }
    return true;
}

bool UnitAST::preregisterUnit(ModuleState& state) {
    for (const auto& statement: statements) {
        if (!statement->preregister(state, unit)) {
            return false;
        }
    }
    return true;
}
