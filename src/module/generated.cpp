#include <llvm/IR/DerivedTypes.h>

#include "generated.h"
#include "ast/ast.h"
#include "logging.h"
#include "module/module_state.h"
#include "ast/llvm_utils.h"
#include "lexer/lexer.h"

bool TypeBacker::operator==(const TypeBacker& other) const {
    return this->backer == other.backer && this->owned == other.owned;
}

size_t std::hash<TypeBacker>::operator()(const TypeBacker& type) const noexcept {
    size_t seed = std::hash<std::variant<std::string, GeneratedType*, FunctionTypeBacker> >()(type.backer);
    seed = combineHash(seed, std::hash<bool>()(type.owned));
    return seed;
}

std::unordered_map<TypeBacker, GeneratedType*> GeneratedType::registeredTypes{};

GeneratedType* GeneratedType::rawGet(std::string rawType) {
    bool owned;
    if (rawType.ends_with("~")) {
        owned = true;
        rawType.pop_back();
    } else {
        owned = false;
    }

    std::variant<std::string, GeneratedType*, FunctionTypeBacker> backer;
    if (rawType.ends_with("[]")) {
        backer = rawGet(rawType.substr(0, rawType.length() - 2));
    } else {
        backer = rawType;
    }
    return get(TypeBacker(backer, owned));
}

GeneratedType* GeneratedType::get(const TypeBacker& type) {
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

bool GeneratedType::isBase() {
    return std::holds_alternative<std::string>(type.backer);
}

bool GeneratedType::isBool() {
    if (!isBase()) {
        return false;
    }
    auto ty = std::get<std::string>(type.backer);
    return ty == KW_BOOL;
}

bool GeneratedType::isVoid() {
    if (!isBase()) {
        return false;
    }
    auto ty = std::get<std::string>(type.backer);
    return ty == KW_VOID;
}

bool GeneratedType::isPrimitive() {
    return isBase() && TYPES.contains(std::get<std::string>(type.backer));
}

bool GeneratedType::isFloating() {
    if (!isBase()) {
        return false;
    }
    auto ty = std::get<std::string>(type.backer);
    return ty == KW_FLOAT || ty == KW_DOUBLE;
}

bool GeneratedType::isSigned() {
    if (!isBase()) {
        return false;
    }
    auto ty = std::get<std::string>(type.backer);
    return ty == KW_LONG || ty == KW_INT || ty == KW_BYTE || ty == KW_ISIZE;
}

bool GeneratedType::isNumber() {
    if (!isBase()) {
        return false;
    }
    auto ty = std::get<std::string>(type.backer);
    return ty == KW_LONG || ty == KW_ULONG || ty == KW_INT || ty == KW_UINT ||
           ty == KW_BYTE || ty == KW_UBYTE || ty == KW_ISIZE || ty == KW_USIZE;
}

bool GeneratedType::isArray() {
    return std::holds_alternative<GeneratedType*>(type.backer);
}

GeneratedType* GeneratedType::getArrayBase() {
    return isArray() ? std::get<GeneratedType*>(type.backer) : nullptr;
}

GeneratedType* GeneratedType::getArrayType(const bool owned) {
    return get(TypeBacker(this, owned));
}

bool GeneratedType::isFunction() {
    return std::holds_alternative<FunctionTypeBacker>(type.backer);
}

std::vector<GeneratedType*> GeneratedType::getArgs() {
    if (!isFunction()) {
        return std::vector<GeneratedType*>();
    }
    return std::get<0>(std::get<FunctionTypeBacker>(type.backer));
}

GeneratedType* GeneratedType::getReturnType() {
    return isFunction() ? std::get<1>(std::get<FunctionTypeBacker>(type.backer)) : nullptr;
}

bool GeneratedType::isDefined(ModuleState& state) {
    if (isArray()) {
        return getArrayBase()->isDefined(state);
    } else if (isFunction()) {
        for (const auto& arg: getArgs()) {
            if (!arg->isDefined(state)) {
                return false;
            }
        }
        return getReturnType()->isDefined(state);
    } else if (isPrimitive()) {
        return true;
    } else {
        return getGenStruct(state);
    }
}

GeneratedStruct* GeneratedType::getGenStruct(ModuleState& state) {
    return isBase() ? state.getStruct(std::get<std::string>(type.backer)) : nullptr;
}

Type* GeneratedType::getLLVMType(const ModuleState& state) {
    if (isArray()) {
        return state.arrFatPtrTy;
    } else if (isFunction()) {
        std::vector<Type*> argTypes{};
        for (const auto& arg: getArgs()) {
            argTypes.push_back(arg->getLLVMType(state));
        }
        return FunctionType::get(getReturnType()->getLLVMType(state), argTypes, false);
    }

    assert(isBase());
    auto ty = std::get<std::string>(type.backer);
    if (ty == KW_BYTE || ty == KW_UBYTE) {
        return Type::getInt8Ty(*state.ctx);
    } else if (ty == KW_INT || ty == KW_UINT) {
        return Type::getInt32Ty(*state.ctx);
    } else if (ty == KW_LONG || ty == KW_ULONG) {
        return Type::getInt64Ty(*state.ctx);
    } else if (ty == KW_USIZE || ty == KW_ISIZE) {
        return state.sizeTy;
    } else if (ty == KW_FLOAT) {
        return Type::getFloatTy(*state.ctx);
    } else if (ty == KW_DOUBLE) {
        return Type::getDoubleTy(*state.ctx);
    } else if (ty == KW_BOOL) {
        return Type::getInt1Ty(*state.ctx);
    } else if (ty == KW_VOID) {
        return Type::getVoidTy(*state.ctx);
    } else if (TYPES.contains(ty)) {
        logError("type " + ty + " not implemented yet");
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
        return nullptr;
    }

    if (genStruct->methods.contains(fieldName)) {
        return std::make_unique<GeneratedValue>(*genStruct->methods.at(fieldName));
    }

    auto fieldIndex = genStruct->getFieldIndex(fieldName);
    if (!fieldIndex.has_value()) {
        return nullptr;
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
        return nullptr;
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
