#include <ostream>
#include <iostream>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Passes/CodeGenPassBuilder.h>
#include <llvm/Support/FileSystem.h>

#include "module_state.h"
#include "logging.h"
#include "generated.h"
#include "module_config.h"
#include "ast/ast.h"
#include "utils.h"
#include "lexer/lexer.h"

using namespace llvm;

ModuleState::ModuleState(const ModuleConfig& config): config(config) {
    ctx = std::make_unique<LLVMContext>();
    module = std::make_unique<Module>("axon main module", *ctx);
    builder = std::make_unique<IRBuilder<> >(*ctx);
    // TODO: set target triple here
    dl = std::make_unique<DataLayout>();
    intPtrTy = dl->getIntPtrType(*ctx);
    // Note: should size_t be different from intptr_t?
    sizeTy = intPtrTy;
    scopeStack.push_back(std::vector<std::string>());
}

ModuleState::~ModuleState() = default;

std::filesystem::path ModuleState::unitToPath(const std::string& unit) {
    auto path = config.moduleRoot();
    for (const auto&& [i, segment]: enumerate(split(unit, "."))) {
        if (i == 0) {
            assert(segment == config.name && "external modules not implemented yet");
        } else {
            path /= segment;
        }
    }
    path.replace_extension(".ax");
    return path;
}

std::string ModuleState::mergeGlobalIdentifier(const std::string& unit, const std::string& identifier) {
    return unit + "." + identifier;
}

void ModuleState::registerUnit(const std::string& unit) {
    if (!units.contains(unit)) {
        units[unit] = nullptr;
        unitStack.push_back(unit);
    }
}

bool ModuleState::registerGlobalIdentifier(const std::string& unit,
                                           const std::string& identifier,
                                           std::unique_ptr<Identifier> val) {
    auto globalIdentifier = mergeGlobalIdentifier(unit, identifier);
    if (globalIdentifiers.contains(globalIdentifier)) {
        logError("duplicate global identifier definition " + globalIdentifier);
        return false;
    }
    globalIdentifiers.insert_or_assign(globalIdentifier, std::move(val));
    return true;
}

bool ModuleState::useGlobalIdentifier(const std::string& unit,
                                      const std::string& identifier,
                                      const std::string& alias) {
    auto globalIdentifier = mergeGlobalIdentifier(unit, identifier);
    if (!globalIdentifiers.contains(globalIdentifier)) {
        logError("unknown global identifier " + globalIdentifier);
        return false;
    }
    return registerIdentifier(alias, std::make_unique<Identifier>(*globalIdentifiers.at(globalIdentifier)));
}


bool ModuleState::compileModule() {
    registerUnit(config.main);
    while (unitStack.size() > 0) {
        auto curUnit = unitStack.back();
        unitStack.pop_back();
        assert(units.contains(curUnit) && units[curUnit] == nullptr && "tried to process unit twice");

        auto curFile = unitToPath(curUnit);
        if (!is_regular_file(curFile)) {
            logError("file " + curFile.string() + " does not exist");
            return false;
        }

        std::string text = readFile(curFile);
        Lexer lexer(text);
        auto unitAst = parseUnit(lexer, curUnit);
        if (!unitAst) {
            return false;
        }
        unitAst->preregisterUnit(*this);
        units[curUnit] = std::move(unitAst);
    }
    for (const auto& unitAst: units | std::views::values) {
        if (!unitAst->codegen(*this)) {
            return false;
        }
    }
    return true;
}

bool ModuleState::writeIR() {
    raw_fd_ostream* out;
    if (config.outputFile.has_value()) {
        std::error_code EC;
        out = new raw_fd_ostream(config.outputFile.value().string(), EC, sys::fs::OF_None);
        if (EC) {
            std::cerr << "Could not open file for writing: " << EC.message() << "\n";
            return false;
        }
    } else {
        out = &outs();
    }

    if (config.outputLL) {
        module->print(*out, nullptr);
    } else {
        WriteBitcodeToFile(*module, *out);
    }

    if (config.outputFile.has_value()) {
        out->close();
        delete out;
    }
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
        logError("duplicate identifier definition " + identifier);
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

Identifier* ModuleState::getIdentifier(const std::string& identifier) {
    if (identifiers.contains(identifier)) {
        return identifiers.at(identifier).get();
    } else if (globalIdentifiers.contains(identifier)) {
        return globalIdentifiers.at(identifier).get();
    } else {
        return nullptr;
    }
}

GeneratedVariable* ModuleState::getVar(const std::string& identifier) {
    auto val = getIdentifier(identifier);
    if (!val) {
        return nullptr;
    }
    auto* varAlloca = std::get_if<GeneratedVariable>(val);
    if (!varAlloca) {
        return nullptr;
    }
    return varAlloca;
}

bool ModuleState::registerGlobalFunction(const std::string& unit,
                                         const std::string& identifier,
                                         const std::vector<SigArg>& signature,
                                         GeneratedType* returnType,
                                         FunctionType* type,
                                         const std::optional<std::string>& customTwine) {
    auto* function = Function::Create(type,
                                      Function::ExternalLinkage,
                                      customTwine.value_or(mergeGlobalIdentifier(unit, identifier)),
                                      module.get());
    return registerGlobalIdentifier(unit,
                                    identifier,
                                    std::make_unique<Identifier>(GeneratedFunction(signature, returnType, function)));
}

GeneratedFunction* ModuleState::getFunction(const std::string& identifier) {
    auto val = getIdentifier(identifier);
    if (!val) {
        return nullptr;
    }
    auto* function = std::get_if<GeneratedFunction>(val);
    if (!function) {
        return nullptr;
    }
    return function;
}

bool ModuleState::registerGlobalStruct(const std::string& unit,
                                       const std::string& identifier,
                                       std::vector<std::tuple<std::string, GeneratedType*> >& fields) {
    auto elements = std::vector<Type*>();
    for (auto& [fieldName, fieldType]: fields) {
        elements.push_back(fieldType->getLLVMType(*this));
    }
    auto* structType = StructType::create(*ctx, elements, mergeGlobalIdentifier(unit, identifier));
    return registerGlobalIdentifier(unit,
                                    identifier,
                                    std::make_unique<Identifier>(
                                        GeneratedStruct(
                                            GeneratedType::get(identifier),
                                            fields,
                                            structType)));
}

GeneratedStruct* ModuleState::getStruct(const std::string& identifier) {
    auto val = getIdentifier(identifier);
    if (!val) {
        return nullptr;
    }
    auto* structIdentifier = std::get_if<GeneratedStruct>(val);
    if (!structIdentifier) {
        return nullptr;
    }
    return structIdentifier;
}
