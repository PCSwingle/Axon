#include <llvm/IR/DerivedTypes.h>

#include "generated.h"
#include "ast/ast.h"
#include "logging.h"
#include "module/module_state.h"
#include "ast/llvm_utils.h"
#include "lexer/lexer.h"

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
    return type == KW_F32 || type == KW_F64;
}

bool GeneratedType::isSigned() {
    return type == KW_I64 || type == KW_I32 || type == KW_I8 || type == KW_ISIZE;
}

bool GeneratedType::isNumber() {
    return type == KW_I64 || type == KW_U64 || type == KW_I32 || type == KW_U32 ||
           type == KW_I8 || type == KW_U8 || type == KW_ISIZE || type == KW_USIZE;
}

bool GeneratedType::isArray() {
    return type.ends_with("[]");
}

bool GeneratedType::isDefined(ModuleState& state) {
    return isArray()
               ? getArrayBase()->isDefined(state)
               : isPrimitive() || getGenStruct(state);
}


GeneratedType* GeneratedType::getArrayBase() {
    return isArray() ? get(type.substr(0, type.length() - 2)) : nullptr;
}

GeneratedType* GeneratedType::toArray() {
    return get(type + "[]");
}

GeneratedStruct* GeneratedType::getGenStruct(ModuleState& state) {
    return state.getStruct(type);
}

Type* GeneratedType::getLLVMType(const ModuleState& state) {
    if (type == KW_I8 || type == KW_U8) {
        return Type::getInt8Ty(*state.ctx);
    } else if (type == KW_I32 || type == KW_U32) {
        return Type::getInt32Ty(*state.ctx);
    } else if (type == KW_I64 || type == KW_U64) {
        return Type::getInt64Ty(*state.ctx);
    } else if (type == KW_USIZE || type == KW_ISIZE) {
        return state.sizeTy;
    } else if (type == KW_F32) {
        return Type::getFloatTy(*state.ctx);
    } else if (type == KW_F64) {
        return Type::getDoubleTy(*state.ctx);
    } else if (type == KW_BOOL) {
        return Type::getInt1Ty(*state.ctx);
    } else if (type == KW_VOID) {
        return Type::getVoidTy(*state.ctx);
    } else if (TYPES.contains(type)) {
        logError("type " + type + " not implemented yet");
        assert(false);
    } else if (isArray()) {
        return state.arrFatPtrTy;
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
    // TODO: make this CreateInBoundsGEP (or CreateStructGep?)
    auto fieldPointer = state.builder->CreateGEP(genStruct->structType,
                                                 value,
                                                 fieldIndices,
                                                 genStruct->type->toString() + "_" + fieldName);
    return std::make_unique<GeneratedValue>(fieldType, fieldPointer);
}

std::unique_ptr<GeneratedValue> GeneratedValue::getArrayPointer(ModuleState& state,
                                                                const std::unique_ptr<GeneratedValue>& index) {
    auto baseType = type->getArrayBase();
    if (!baseType) {
        return logError("type " + type->toString() + " is not an array");
    }
    assert(type->getLLVMType(state) == state.arrFatPtrTy);

    auto basePtr = state.builder->CreateExtractValue(value, std::vector<unsigned>{0}, "arr_ptr_extract");
    auto baseInt = state.builder->CreatePtrToInt(basePtr, state.sizeTy, "arr_base_int");
    auto indexOffset = state.builder->CreateMul(
        state.builder->CreateTrunc(ConstantExpr::getSizeOf(baseType->getLLVMType(state)), state.sizeTy),
        index->value,
        "ix_offset");
    auto indexInt = state.builder->CreateAdd(baseInt, indexOffset, "ix_int");
    auto indexPtr = state.builder->CreateIntToPtr(indexInt, PointerType::getUnqual(*state.ctx), "ix_ptr");
    return std::make_unique<GeneratedValue>(baseType, indexPtr);
}

std::optional<int> GeneratedStruct::getFieldIndex(const std::string& fieldName) {
    for (auto&& [i, field]: std::views::enumerate(fields)) {
        if (std::get<0>(field) == fieldName) {
            return i;
        }
    }
    return std::nullopt;
}
