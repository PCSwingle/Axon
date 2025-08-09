#pragma once
#include <variant>

#include "llvm/IR/IRBuilder.h"
#include <llvm/IR/Module.h>

#include "generated.h"
#include "build/module_config.h"

namespace llvm {
    class AllocaInst;
}

using namespace llvm;

struct SigArg;

struct GeneratedType;
struct GeneratedStruct;
struct GeneratedFunction;
struct GeneratedVariable;

class ModuleState {
    AllocaInst* createAlloca(GeneratedType* type, const std::string& name);

public:
    std::unique_ptr<LLVMContext> ctx;
    std::unique_ptr<IRBuilder<> > builder;
    std::unique_ptr<Module> module;
    std::unique_ptr<DataLayout> dl;
    Type* intPtrTy;

    const ModuleConfig& config;

    explicit ModuleState(const ModuleConfig& config): config(config) {
        ctx = std::make_unique<LLVMContext>();
        module = std::make_unique<Module>("axon main module", *ctx);
        builder = std::make_unique<IRBuilder<> >(*ctx);
        // TODO: set target triple here
        dl = std::make_unique<DataLayout>();
        intPtrTy = dl->getIntPtrType(*ctx);
        scopeStack.push_back(std::vector<std::string>());
    }

private:
    // main compilation
    std::filesystem::path unitToPath(const std::string& unit);

    std::unordered_map<std::string, std::unique_ptr<UnitAST> > units;
    std::vector<std::string> unitStack;

    std::unordered_map<std::string, std::unique_ptr<Identifier> > globalIdentifiers;

    static std::string mergeGlobalIdentifier(const std::string& unit, const std::string& identifier);

public:
    void registerUnit(const std::string& unit);

    bool registerGlobalIdentifier(const std::string& unit,
                                  const std::string& identifier,
                                  std::unique_ptr<Identifier> val);

    bool useGlobalIdentifier(const std::string& unit, const std::string& identifier, const std::string& alias);

    bool compileModule();

    bool writeIR(const std::filesystem::path& outputFile, bool bitcode = true);

    // codegen state
    std::unordered_map<std::string, std::unique_ptr<Identifier> > identifiers;
    std::vector<const GeneratedFunction*> functionStack;
    std::vector<std::vector<std::string> > scopeStack;

    bool enterFunc(const GeneratedFunction* function);

    void exitFunc();

    GeneratedType* expectedReturnType();

    void enterScope();

    void exitScope();

    bool registerIdentifier(const std::string& identifier, std::unique_ptr<Identifier> val);

private:
    Identifier* getIdentifier(const std::string& identifier);

public:
    bool registerVar(const std::string& identifier, GeneratedType* type);

    GeneratedVariable* getVar(const std::string& identifier);

    bool registerGlobalFunction(const std::string& unit,
                                const std::string& identifier,
                                const std::vector<SigArg>& signature,
                                GeneratedType* returnType,
                                FunctionType* type);

    GeneratedFunction* getFunction(const std::string& identifier);

    bool registerGlobalStruct(const std::string& unit,
                              const std::string& identifier,
                              std::vector<std::tuple<std::string, GeneratedType*> >& fields);

    GeneratedStruct* getStruct(const std::string& identifier);
};
