#pragma once
#include <variant>

namespace llvm {
    class Value;
    class Type;
}

using namespace llvm;

class ModuleState;

struct GeneratedType;

std::vector<Value*> createFieldIndices(const ModuleState& state, const int index);

std::string typeToString(const Type* type);

CallInst* createMalloc(ModuleState& state, Value* allocSize, const std::string& name);
