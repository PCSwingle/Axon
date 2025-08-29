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

std::shared_ptr<GeneratedValue> FuncAST::declare(const ModuleState& state, const std::string& twine) {
    std::vector<GeneratedType*> args{};
    std::vector<Type*> argTypes;
    for (const auto& [type, identifier]: signature) {
        args.push_back(type);
        argTypes.push_back(type->getLLVMType(state));
    }
    auto functionType = TypeBacker(std::make_tuple(args, returnType), false);
    FunctionType* type = FunctionType::get(returnType->getLLVMType(state), argTypes, hasVarArgs);
    auto* function = Function::Create(type,
                                      Function::ExternalLinkage,
                                      twine,
                                      state.module.get());
    auto genFunction = std::make_shared<GeneratedValue>(GeneratedType::get(functionType), function);
    declaration = genFunction;
    return genFunction;
}

bool FuncAST::preregister(ModuleState& state, const std::string& unit) {
    std::string twine;
    if (isExtern || (funcName == "main" && unit == state.config.main)) {
        twine = funcName;
    } else {
        twine = unit + "." + funcName;
    }

    auto genFunction = declare(state, twine);
    if (!state.registerGlobalIdentifier(unit,
                                        funcName,
                                        std::make_unique<Identifier>(std::move(*genFunction)))) {
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
    std::unordered_map<std::string, std::shared_ptr<GeneratedValue> > generatedMethods;
    for (const auto& [methodName, method]: methods) {
        generatedMethods[methodName] = method->declare(state, unit + "." + structName + "." + methodName);
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
                                                GeneratedType::get(TypeBacker(structName, true)),
                                                std::move(fields),
                                                std::move(generatedMethods),
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
