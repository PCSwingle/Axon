#include "ast/ast.h"
#include "logging.h"
#include "module/generated.h"
#include "module/module_state.h"

std::unique_ptr<GeneratedValue> VariableExprAST::codegenPointer(ModuleState& state) {
    auto* genVar = state.getVar(varName);
    if (!genVar) {
        return logError("undefined variable " + varName);
    }
    return std::make_unique<GeneratedValue>(genVar->type, genVar->value);
}

std::unique_ptr<GeneratedValue> MemberAccessExprAST::codegenPointer(ModuleState& state) {
    auto structVal = structExpr->codegenValue(state);
    if (!structVal) {
        return nullptr;
    }
    auto fieldPointer = structVal->getFieldPointer(state, fieldName);
    if (!fieldPointer) {
        return nullptr;
    }
    return fieldPointer;
}

std::unique_ptr<GeneratedValue> SubscriptExprAST::codegenPointer(ModuleState& state) {
    return logError("subscript not yet implemented");
}
