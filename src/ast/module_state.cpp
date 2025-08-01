#include <ostream>
#include <iostream>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Passes/CodeGenPassBuilder.h>
#include <llvm/Support/FileSystem.h>

#include "debug_consts.h"
#include "module_state.h"

using namespace llvm;

bool ModuleState::writeIR(const std::string& filename, bool bitcode) {
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

AllocaInst* ModuleState::createAlloca(TypeAST* type, std::string& name) {
    auto oldIP = builder->saveIP();
    auto& entry = builder->GetInsertBlock()->getParent()->getEntryBlock();
    builder->SetInsertPoint(&entry, entry.begin());
    auto* newAlloca = builder->CreateAlloca(type->getType(*this), nullptr, name);
    builder->restoreIP(oldIP);
    return newAlloca;
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
    if (identifiers.find(identifier) != identifiers.end()) {
        return false;
    }
    identifiers.insert_or_assign(identifier, std::move(val));
    scopeStack.back().push_back(identifier);
    return true;
}

bool ModuleState::registerVar(std::string& identifier, TypeAST* type) {
    auto* varAlloca = createAlloca(type, identifier);
    return registerIdentifier(identifier, std::make_unique<Identifier>(VariableIdentifier(varAlloca)));
}

VariableIdentifier* ModuleState::getVar(std::string& identifier) {
    if (identifiers.find(identifier) == identifiers.end()) {
        return nullptr;
    }
    auto& val = identifiers.at(identifier);
    auto* varAlloca = std::get_if<VariableIdentifier>(val.get());
    if (!varAlloca) {
        return nullptr;
    }
    return varAlloca;
}

bool ModuleState::registerFunction(std::string& identifier, FunctionType* type) {
    auto* function = Function::Create(type, Function::ExternalLinkage, identifier, module.get());
    return registerIdentifier(identifier, std::make_unique<Identifier>(FunctionIdentifier(function)));
}

FunctionIdentifier* ModuleState::getFunction(std::string& identifier) {
    if (identifiers.find(identifier) == identifiers.end()) {
        return nullptr;
    }
    auto& val = identifiers.at(identifier);
    auto* function = std::get_if<FunctionIdentifier>(val.get());
    if (!function) {
        return nullptr;
    }
    return function;
}

bool ModuleState::registerStruct(std::string& identifier,
                                 std::vector<std::tuple<std::string, TypeAST*> >& fields) {
    auto elements = std::vector<Type*>();
    for (auto& [fieldName, fieldType]: fields) {
        elements.push_back(fieldType->getType(*this));
    }
    auto* structType = StructType::get(*ctx, elements);
    return registerIdentifier(identifier, std::make_unique<Identifier>(StructIdentifier(structType, fields)));
}

StructIdentifier* ModuleState::getStruct(std::string& identifier) {
    if (identifiers.find(identifier) == identifiers.end()) {
        return nullptr;
    }
    auto& val = identifiers.at(identifier);
    auto* structIdentifier = std::get_if<StructIdentifier>(val.get());
    if (!structIdentifier) {
        return nullptr;
    }
    return structIdentifier;
}
