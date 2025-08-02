#pragma once

#include <cassert>
#include <string>
#include <unordered_map>
#include <ranges>

#include "module_state.h"
#include "lexer/lexer.h"


namespace llvm {
    class Function;
    class AllocaInst;
    class Type;
    class Value;
    class StructType;
}

using namespace llvm;

struct SigArg;

class ModuleState;

struct GeneratedStruct;

/// Similar to LLVM, types are pointers to singletons that aren't freed until program end (flyweights).
/// Every individual type is a pointer to the same object.
struct GeneratedType {
private:
    static inline std::unordered_map<std::string, GeneratedType*> registeredTypes{};

    // TODO: with modules / imports this will need to become more complex
    std::string type;

    explicit GeneratedType(std::string type): type(std::move(type)) {
    }

public:
    static GeneratedType* get(const std::string& type) {
        if (!registeredTypes.contains(type)) {
            auto* generatedType = new GeneratedType(type);
            registeredTypes.insert_or_assign(type, generatedType);
        }
        return registeredTypes.at(type);
    }

    static void free() {
        for (auto type: registeredTypes | std::views::values) {
            delete type;
        }
    }

    std::string toString();

    bool isBool() {
        return type == KW_BOOL;
    }

    bool isVoid() {
        return type == KW_VOID;
    }

    bool isPrimitive() {
        return TYPES.contains(type);
    }

    bool isFloating() {
        return type == KW_FLOAT || type == KW_DOUBLE;
    }

    GeneratedStruct* getGenStruct(ModuleState& state) {
        return state.getStruct(type);
    }

    Type* getLLVMType(ModuleState& state);
};

struct GeneratedValue {
    GeneratedType* type;
    Value* value;

    explicit GeneratedValue(GeneratedType* type, Value* value): type(type), value(value) {
        assert(type);
        assert(value);
    }
};

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
