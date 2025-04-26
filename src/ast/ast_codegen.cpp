#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "ast.h"
#include "logging.h"
#include "lexer/lexer.h"
#include "utils.h"

using namespace llvm;


Type* TypeAST::getType(ModuleState& state) {
    if (type == "int") {
        return Type::getInt32Ty(*state.ctx);
    } else if (type == "long") {
        return Type::getInt64Ty(*state.ctx);
    } else if (type == "double") {
        return Type::getDoubleTy(*state.ctx);
    }

    return logError("type " + type + " currently not supported");
}

// expr
Value* ValueExprAST::codegenValue(ModuleState& state) {
    if (rawValue == KW_TRUE) {
        return ConstantInt::getTrue(*state.ctx);
    } else if (rawValue == KW_FALSE) {
        return ConstantInt::getFalse(*state.ctx);
    } else if (rawValue.find('.') != std::string::npos) {
        return ConstantFP::get(Type::getDoubleTy(*state.ctx), APFloat(APFloat::IEEEdouble(), rawValue));
    } else {
        return ConstantInt::getIntegerValue(Type::getInt64Ty(*state.ctx), APInt(64, rawValue, 10));
    }
}

Value* VariableExprAST::codegenValue(ModuleState& state) {
    Value* value = state.values[varName->identifier];
    if (!value) {
        return logError("undefined variable " + varName->identifier);
    }
    return value;
}


Value* BinaryOpExprAST::codegenValue(ModuleState& state) {
    auto L = LHS->codegenValue(state);
    auto R = RHS->codegenValue(state);
    if (!L || !R) {
        return nullptr;
    }

    if (binOp == "+") {
        return state.builder->CreateAdd(L, R, "add_binop");
        return state.builder->CreateAdd(L, R, "add_binop");
    } else if (binOp == "-") {
        return state.builder->CreateSub(L, R, "sub_binop");
    } else if (binOp == "*") {
        return state.builder->CreateMul(L, R, "mul_binop");
    } else if (binOp == "/") {
        return state.builder->CreateSDiv(L, R, "div_binop");
    } else if (binOp == "%") {
        return state.builder->CreateSRem(L, R, "mod_binop");
    }

    return logError("binop " + binOp + " currently not supported");
}


Value* UnaryOpExprAST::codegenValue(ModuleState& state) {
    auto ex = expr->codegenValue(state);
    if (!ex) {
        return nullptr;
    }

    if (unaryOp == "-") {
        return state.builder->CreateNeg(ex, "neg_binop");
    }

    return logError("unop " + unaryOp + " currently not supported");
}


Value* CallExprAST::codegenValue(ModuleState& state) {
    Function* callee = state.module->getFunction(callName->identifier);
    if (!callee) {
        return logError("function " + callName->identifier + " not defined");
    }
    if (callee->arg_size() != args.size()) {
        return logError(
            "expected " + std::to_string(callee->arg_size()) + " arguments, got " + std::to_string(args.size()) +
            "arguments");
    }

    std::vector<Value*> argsV;
    for (int i = 0; i < args.size(); i++) {
        auto arg = args[i]->codegenValue(state);
        if (!arg) {
            return nullptr;
        }
        if (callee->getArg(i)->getType() != arg->getType()) {
            return logError(
                "expected type " + typeToString(callee->getArg(i)->getType()) + ", got type " + typeToString(
                    arg->getType()));
        }
        argsV.push_back(arg);
    }
    return state.builder->CreateCall(callee, argsV, "call_" + callName->identifier);
}

// statements
bool VarAST::codegen(ModuleState& state) {
    logError("variables not supported... yet...");
    return false;
}


bool FuncAST::codegen(ModuleState& state) {
    // Create prototype
    Type* returnType = type->getType(state);
    std::vector<Type*> argTypes;
    for (const auto& sig: signature) {
        argTypes.push_back(sig.type->getType(state));
    }
    FunctionType* FT = FunctionType::get(returnType, argTypes, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, funcName->identifier, state.module.get());
    state.values.clear();
    for (int i = 0; i < signature.size(); i++) {
        auto Arg = F->getArg(i);
        Arg->setName(signature[i].identifier->identifier);
        state.values[signature[i].identifier->identifier] = Arg;
    }

    // Function block
    BasicBlock* BB = BasicBlock::Create(*state.ctx, "entry", F);
    state.builder->SetInsertPoint(BB);
    if (!block->codegen(state)) {
        return false;
    }
    verifyFunction(*F);
    return true;
}

bool IfAST::codegen(ModuleState& state) {
    logError("if not supported... yet...");
    return false;
}

bool WhileAST::codegen(ModuleState& state) {
    logError("while not supported... yet...");
    return false;
}

bool ReturnAST::codegen(ModuleState& state) {
    if (returnExpr.has_value()) {
        Value* returnValue = returnExpr->get()->codegenValue(state);
        if (!returnValue) {
            return false;
        }
        state.builder->CreateRet(returnValue);
    } else {
        state.builder->CreateRetVoid();
    }
    return true;
}


// other
bool BlockAST::codegen(ModuleState& state) {
    for (const auto& statement: statements) {
        if (!statement->codegen(state)) {
            return false;
        }
    }
    return true;
}
