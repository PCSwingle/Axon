#include "ast/ast.h"
#include "lexer/lexer.h"
#include "module/generated.h"
#include "module/module_state.h"

std::unique_ptr<GeneratedValue> VariableExprAST::codegenPointer(ModuleState& state) {
    auto* genVar = state.getVar(varName);
    if (!genVar) {
        return state.setError(this->debugInfo, "Undefined variable " + varName);
    }
    return std::make_unique<GeneratedValue>(genVar->type, genVar->value);
}

std::unique_ptr<GeneratedValue> MemberAccessExprAST::codegenPointer(ModuleState& state) {
    auto structVal = structExpr->codegenValue(state, nullptr);
    if (!structVal) {
        return nullptr;
    }
    auto fieldPointer = structVal->getFieldPointer(state, fieldName);
    if (!fieldPointer) {
        return state.setError(this->debugInfo,
                              "Could not find field " + fieldName + " on type " + structVal->type->toString());
    }
    return fieldPointer;
}

std::unique_ptr<GeneratedValue> SubscriptExprAST::codegenPointer(ModuleState& state) {
    auto arrayVal = arrayExpr->codegenValue(state, nullptr);
    if (!arrayVal) {
        return nullptr;
    }
    auto indexVal = indexExpr->codegenValue(state, GeneratedType::rawGet(KW_USIZE));
    if (!indexVal) {
        return nullptr;
    }
    if (indexVal->type != GeneratedType::rawGet(KW_USIZE)) {
        return state.setError(this->debugInfo,
                              "Arrays must be indexed with usize type, got " + indexVal->type->toString());
    }
    auto indexPointer = arrayVal->getArrayPointer(state, indexVal);
    if (!indexPointer) {
        return state.setError(this->debugInfo, "Cannot subscript type " + arrayVal->type->toString());
    }
    return indexPointer;
}
