#include <ostream>
#include <iostream>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Passes/CodeGenPassBuilder.h>
#include <llvm/Support/FileSystem.h>

#include "debug_consts.h"
#include "module_state.h"
#include "logging.h"
#include "generated.h"
#include "ast.h"

using namespace llvm;

bool ModuleState::writeIR(const std::string& filename, const bool bitcode) {
    if (DEBUG_CODEGEN_PRINT_MODULE) {
        std::cout << "full ir:" << std::endl;
        module->print(outs(), nullptr);
    }

    std::error_code EC;
    raw_fd_ostream out(filename, EC, sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file for writing: " << EC.message() << "\n";
        return false;
    }
    if (bitcode) {
        WriteBitcodeToFile(*module, out);
    } else {
        module->print(out, nullptr);
    }
    out.close();
    return true;
}

AllocaInst* ModuleState::createAlloca(GeneratedType* type, const std::string& name) {
    auto oldIP = builder->saveIP();
    auto& entry = builder->GetInsertBlock()->getParent()->getEntryBlock();
    builder->SetInsertPoint(&entry, entry.begin());
    auto* newAlloca = builder->CreateAlloca(type->getLLVMType(*this), nullptr, name);
    builder->restoreIP(oldIP);
    return newAlloca;
}

bool ModuleState::enterFunc(const GeneratedFunction* function) {
    functionStack.push_back(function);
    for (int i = 0; i < function->signature.size(); i++) {
        auto [type, identifier] = function->signature[i];
        if (!registerVar(identifier, type)) {
            logError("duplicate identifier definition " + identifier);
            return false;
        }
    }
    return true;
}

void ModuleState::exitFunc() {
    for (auto const& [type, identifier]: functionStack.back()->signature) {
        identifiers.erase(identifier);
    }
    functionStack.pop_back();
}

GeneratedType* ModuleState::expectedReturnType() {
    return functionStack.back()->returnType;
}


void ModuleState::enterScope() {
    scopeStack.push_back(std::vector<std::string>());
}

void ModuleState::exitScope() {
    for (auto const& identifier: scopeStack.back()) {
        identifiers.erase(identifier);
    }
    scopeStack.pop_back();
}

bool ModuleState::registerIdentifier(const std::string& identifier, std::unique_ptr<Identifier> val) {
    if (identifiers.contains(identifier)) {
        return false;
    }
    identifiers.insert_or_assign(identifier, std::move(val));
    scopeStack.back().push_back(identifier);
    return true;
}

bool ModuleState::registerVar(const std::string& identifier, GeneratedType* type) {
    auto* varAlloca = createAlloca(type, identifier);
    return registerIdentifier(identifier, std::make_unique<Identifier>(GeneratedVariable(type, varAlloca)));
}

GeneratedVariable* ModuleState::getVar(const std::string& identifier) {
    if (!identifiers.contains(identifier)) {
        return nullptr;
    }
    auto& val = identifiers.at(identifier);
    auto* varAlloca = std::get_if<GeneratedVariable>(val.get());
    if (!varAlloca) {
        return nullptr;
    }
    return varAlloca;
}

bool ModuleState::registerFunction(const std::string& identifier,
                                   const std::vector<SigArg>& signature,
                                   GeneratedType* returnType,
                                   FunctionType* type) {
    auto* function = Function::Create(type, Function::ExternalLinkage, identifier, module.get());
    return registerIdentifier(identifier,
                              std::make_unique<Identifier>(GeneratedFunction(signature, returnType, function)));
}

GeneratedFunction* ModuleState::getFunction(const std::string& identifier) {
    if (!identifiers.contains(identifier)) {
        return nullptr;
    }
    auto& val = identifiers.at(identifier);
    auto* function = std::get_if<GeneratedFunction>(val.get());
    if (!function) {
        return nullptr;
    }
    return function;
}

bool ModuleState::registerStruct(const std::string& identifier,
                                 std::vector<std::tuple<std::string, GeneratedType*> >& fields) {
    auto elements = std::vector<Type*>();
    for (auto& [fieldName, fieldType]: fields) {
        elements.push_back(fieldType->getLLVMType(*this));
    }
    auto* structType = StructType::get(*ctx, elements);
    return registerIdentifier(identifier,
                              std::make_unique<Identifier>(
                                  GeneratedStruct(GeneratedType::get(identifier),
                                                  fields,
                                                  structType)));
}

GeneratedStruct* ModuleState::getStruct(const std::string& identifier) {
    if (!identifiers.contains(identifier)) {
        return nullptr;
    }
    auto& val = identifiers.at(identifier);
    auto* structIdentifier = std::get_if<GeneratedStruct>(val.get());
    if (!structIdentifier) {
        return nullptr;
    }
    return structIdentifier;
}
