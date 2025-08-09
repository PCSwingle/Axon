#include <llvm/IR/DerivedTypes.h>

#include "ast.h"
#include "generated.h"
#include "logging.h"
#include "module_state.h"
#include "llvm_utils.h"


std::unordered_map<std::string, GeneratedType*> GeneratedType::registeredTypes{};

GeneratedType* GeneratedType::get(const std::string& type) {
    if (!registeredTypes.contains(type)) {
        auto* generatedType = new GeneratedType(type);
        registeredTypes.insert_or_assign(type, generatedType);
    }
    return registeredTypes.at(type);
}

void GeneratedType::free() {
    for (auto type: registeredTypes | std::views::values) {
        delete type;
    }
}

bool GeneratedType::isBool() {
    return type == KW_BOOL;
}

bool GeneratedType::isVoid() {
    return type == KW_VOID;
}

bool GeneratedType::isPrimitive() {
    return TYPES.contains(type);
}

bool GeneratedType::isFloating() {
    return type == KW_FLOAT || type == KW_DOUBLE;
}

GeneratedStruct* GeneratedType::getGenStruct(ModuleState& state) {
    return state.getStruct(type);
}

Type* GeneratedType::getLLVMType(ModuleState& state) {
    if (type == KW_INT) {
        return Type::getInt32Ty(*state.ctx);
    } else if (type == KW_LONG) {
        return Type::getInt64Ty(*state.ctx);
    } else if (type == KW_FLOAT) {
        return Type::getFloatTy(*state.ctx);
    } else if (type == KW_DOUBLE) {
        return Type::getDoubleTy(*state.ctx);
    } else if (type == KW_BOOL) {
        return Type::getInt1Ty(*state.ctx);
    } else if (type == KW_VOID) {
        return Type::getVoidTy(*state.ctx);
    } else if (TYPES.contains(type)) {
        logError("type " + type + " not implemented yet");
        assert(false);
    } else {
        // Checking if the struct actually exists here would be a massive PITA
        // for such marginally low value, so we just assume it's a pointer.
        // Figuring out structs without pointers is going to be piss awful given
        // we can't allow recursive types.
        return PointerType::getUnqual(*state.ctx);
    }
}

std::unique_ptr<GeneratedValue> GeneratedValue::getFieldPointer(ModuleState& state, const std::string& fieldName) {
    auto genStruct = type->getGenStruct(state);
    if (!genStruct) {
        return logError("type " + type->toString() + " is not a struct or does not exist");
    }
    auto fieldIndex = genStruct->getFieldIndex(fieldName);
    if (!fieldIndex.has_value()) {
        return logError("struct " + genStruct->type->toString() + " has no field " + fieldName);
    }
    auto fieldType = std::get<1>(genStruct->fields[fieldIndex.value()]);
    auto fieldIndices = createFieldIndices(state, fieldIndex.value());
    auto fieldPointer = state.builder->CreateGEP(genStruct->structType,
                                                 value,
                                                 fieldIndices,
                                                 genStruct->type->toString() + "_" + fieldName);
    return std::make_unique<GeneratedValue>(fieldType, fieldPointer);
}

std::unique_ptr<GeneratedValue> GeneratedVariable::toValue() {
    return std::make_unique<GeneratedValue>(type, varAlloca);
}


std::optional<int> GeneratedStruct::getFieldIndex(const std::string& fieldName) {
    for (auto&& [i, field]: std::views::enumerate(fields)) {
        if (std::get<0>(field) == fieldName) {
            return i;
        }
    }
    return std::nullopt;
}
