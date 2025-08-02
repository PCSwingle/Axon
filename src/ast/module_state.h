#pragma once
#include <variant>
#include <logging.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include "ast.h"

using namespace llvm;

struct GeneratedVariable {
    GeneratedType* type;
    AllocaInst* varAlloca;

    explicit GeneratedVariable(GeneratedType* type, AllocaInst* varAlloca): type(type), varAlloca(varAlloca) {
    }
};

struct GeneratedFunction {
    std::vector<SigArg> signature;
    GeneratedType* returnType;
    Function* function;

    explicit GeneratedFunction(const std::vector<SigArg>& signature,
                               GeneratedType* returnType,
                               Function* function): signature(signature),
                                                    returnType(returnType),
                                                    function(function) {
    }
};

struct GeneratedStruct {
    GeneratedType* type;
    std::vector<std::tuple<std::string, GeneratedType*> > fields;
    StructType* structType;

    explicit GeneratedStruct(GeneratedType* type,
                             std::vector<std::tuple<std::string, GeneratedType*> > fields,
                             StructType* structType
    ): type(type), fields(move(fields)), structType(structType) {
    }
};

typedef std::variant<GeneratedVariable, GeneratedFunction, GeneratedStruct> Identifier;

class ModuleState {
    AllocaInst* createAlloca(GeneratedType* type, const std::string& name);

public:
    // TODO: should context be static?
    std::unique_ptr<LLVMContext> ctx;
    std::unique_ptr<IRBuilder<> > builder;
    std::unique_ptr<Module> module;
    std::unique_ptr<DataLayout> dl;
    Type* intPtrTy;
    std::unordered_map<std::string, std::unique_ptr<Identifier> > identifiers;

    std::vector<const GeneratedFunction*> functionStack;
    std::vector<std::vector<std::string> > scopeStack;

    ModuleState() {
        ctx = std::make_unique<LLVMContext>();
        module = std::make_unique<Module>("axon main module", *ctx);
        builder = std::make_unique<IRBuilder<> >(*ctx);
        // TODO: set target triple here
        dl = std::make_unique<DataLayout>();
        intPtrTy = dl->getIntPtrType(*ctx);
        scopeStack.push_back(std::vector<std::string>());
    }

    bool writeIR(const std::string& filename, bool bitcode = true);

    bool enterFunc(const GeneratedFunction* function);

    void exitFunc();

    GeneratedType* expectedReturnType();

    void enterScope();

    void exitScope();

    bool registerIdentifier(const std::string& identifier, std::unique_ptr<Identifier> val);

    bool registerVar(const std::string& identifier, GeneratedType* type);

    GeneratedVariable* getVar(std::string& identifier);

    bool registerFunction(const std::string& identifier,
                          const std::vector<SigArg>& signature,
                          GeneratedType* returnType,
                          FunctionType* type);

    GeneratedFunction* getFunction(const std::string& identifier);

    bool registerStruct(const std::string& identifier,
                        std::vector<std::tuple<std::string, GeneratedType*> >& fields);

    GeneratedStruct* getStruct(const std::string& identifier);
};
