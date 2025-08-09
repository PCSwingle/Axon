#pragma once

#include <cassert>
#include <string>
#include <unordered_map>

#include "ast/ast.h"
#include "lexer/lexer.h"


namespace llvm {
    class Function;
    class AllocaInst;
    class Type;
    class Value;
    class StructType;
}

using namespace llvm;

class ModuleState;

struct GeneratedVariable;
struct GeneratedFunction;
struct GeneratedStruct;

typedef std::variant<GeneratedVariable, GeneratedFunction, GeneratedStruct> Identifier;

/// Similar to LLVM, types are pointers to singletons that aren't freed until program end (flyweights).
/// Every individual type is a pointer to the same object.
/// Note: types are identified solely by the identifier used in their unit, so types between units
/// are not guaranteed equal.
struct GeneratedType {
private:
    static std::unordered_map<std::string, GeneratedType*> registeredTypes;

    std::string type;

    explicit GeneratedType(std::string type): type(std::move(type)) {
    }

public:
    static GeneratedType* get(const std::string& type);

    static void free();

    std::string toString();

    bool isBool();

    bool isVoid();

    bool isPrimitive();

    bool isFloating();

    GeneratedStruct* getGenStruct(ModuleState& state);

    Type* getLLVMType(ModuleState& state);
};

struct GeneratedValue {
    GeneratedType* type;
    Value* value;

    explicit GeneratedValue(GeneratedType* type, Value* value): type(type), value(value) {
        assert(type);
        assert(value);
    }

    std::unique_ptr<GeneratedValue> getFieldPointer(ModuleState& state, const std::string& fieldName);
};

struct GeneratedVariable {
    GeneratedType* type;
    AllocaInst* varAlloca;

    explicit GeneratedVariable(GeneratedType* type, AllocaInst* varAlloca): type(type), varAlloca(varAlloca) {
    }

    // TODO: should this whole class even exist?
    std::unique_ptr<GeneratedValue> toValue();
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

    std::optional<int> getFieldIndex(const std::string& fieldName);
};

