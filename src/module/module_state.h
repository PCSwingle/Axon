#pragma once

#include <filesystem>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

#include "typedefs.h"

struct DebugInfo;

namespace llvm {
    class AllocaInst;
}

using namespace llvm;

class ModuleConfig;

struct SigArg;
class UnitAST;

struct GeneratedType;
struct GeneratedStruct;
struct GeneratedValue;

class ModuleState {
    AllocaInst* createAlloca(GeneratedType* type, const std::string& name);

public:
    std::unique_ptr<LLVMContext> ctx;
    std::unique_ptr<IRBuilder<> > builder;
    std::unique_ptr<Module> module;
    std::unique_ptr<DataLayout> dl;

    Type* intPtrTy;
    Type* sizeTy;
    // Array pointers are fat pointers consisting of a pointer to the array and the size of the array
    StructType* arrFatPtrTy;

    const ModuleConfig& config;

    explicit ModuleState(const ModuleConfig& config);

    ~ModuleState();

private:
    // main compilation
    std::filesystem::path unitToPath(const std::string& unit);

    std::unordered_map<std::string, std::unique_ptr<UnitAST> > units;
    std::vector<std::string> unitStack;

    std::unordered_map<std::string, std::unique_ptr<Identifier> > globalIdentifiers;

    std::unique_ptr<DebugInfo> buildErrorDebugInfo;
    std::string buildError;

public:
    std::nullptr_t setError(const DebugInfo& debugInfo, const std::string& error);

    void unsetError();

    bool registerUnit(const std::string& unit);

    bool registerGlobalIdentifier(const std::string& unit,
                                  const std::string& identifier,
                                  std::unique_ptr<Identifier> val);

    bool useGlobalIdentifier(const std::string& unit, const std::string& identifier, const std::string& alias);

    bool compileModule();

    bool writeIR();

    // codegen state
    std::unordered_map<std::string, std::unique_ptr<Identifier> > identifiers;
    std::vector<const GeneratedValue*> functionStack;
    std::vector<std::vector<std::string> > scopeStack;

    void enterFunc(const GeneratedValue* function);

    void exitFunc();

    GeneratedType* expectedReturnType();

    void enterScope();

    void exitScope();

    bool registerIdentifier(const std::string& identifier, std::unique_ptr<Identifier> val);

private:
    Identifier* getIdentifier(const std::string& identifier);

public:
    bool registerVar(const std::string& identifier, GeneratedType* type);

    // TODO: change to getidentifier
    GeneratedValue* getVar(const std::string& identifier);

    GeneratedStruct* getStruct(const std::string& identifier);
};
