#pragma once

#include <cassert>
#include <string>
#include <unordered_map>

#include "typedefs.h"
#include "utils.h"

namespace llvm {
    class Function;
    class AllocaInst;
    class Type;
    class Value;
    class StructType;
}

using namespace llvm;

class ModuleState;

struct SigArg;

/// Similar to LLVM, types are pointers to singletons that aren't freed until program end (flyweights).
/// Every individual type is a pointer to the same object.
/// Note: types are identified solely by the identifier used in their unit, so types between units
/// are not guaranteed equal.
struct GeneratedType {
private:
    static std::unordered_map<TypeBacker, GeneratedType*, TypeBackerHash> registeredTypes;

    TypeBacker type;

    explicit GeneratedType(TypeBacker type): type(std::move(type)) {
    }

public:
    static GeneratedType* rawGet(const std::string& rawType);

    static GeneratedType* get(const TypeBacker& type);

    static void free();

    std::string toString();

    bool isBase();

    bool isBool();

    bool isVoid();

    bool isPrimitive();

    bool isFloating();

    bool isSigned();

    bool isNumber();

    bool isArray();

    GeneratedType* getArrayBase();

    GeneratedType* getArrayType();

    bool isFunction();

    std::vector<GeneratedType*> getArgs();

    GeneratedType* getReturnType();

    bool isDefined(ModuleState& state);

    GeneratedStruct* getGenStruct(ModuleState& state);

    Type* getLLVMType(const ModuleState& state);
};

struct GeneratedValue {
    // TODO: add name
    GeneratedType* type;
    Value* value;

    explicit GeneratedValue(GeneratedType* type, Value* value): type(type), value(value) {
        assert(type);
        assert(value);
    }

    std::unique_ptr<GeneratedValue> getFieldPointer(ModuleState& state, const std::string& fieldName);

    std::unique_ptr<GeneratedValue> getArrayPointer(ModuleState& state, const std::unique_ptr<GeneratedValue>& index);
};

struct GeneratedStruct {
    GeneratedType* type;
    std::vector<std::tuple<std::string, GeneratedType*> > fields;
    std::unordered_map<std::string, GeneratedValue*> methods;
    StructType* structType;

    explicit GeneratedStruct(GeneratedType* type,
                             std::vector<std::tuple<std::string, GeneratedType*> > fields,
                             std::unordered_map<std::string, GeneratedValue*> methods,
                             StructType* structType
    ): type(type), fields(std::move(fields)), methods(std::move(methods)), structType(structType) {
    }

    std::optional<int> getFieldIndex(const std::string& fieldName);
};

