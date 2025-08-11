#pragma once

namespace llvm {
    class Value;
    class Type;
}

using namespace llvm;

class ModuleState;

std::vector<Value*> createFieldIndices(const ModuleState& state, const int index);

std::string typeToString(const Type* type);

CallInst* createMalloc(ModuleState& state, Value* size, const std::string& name);
