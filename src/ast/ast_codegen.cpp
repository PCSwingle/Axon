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
    } else if (type == "void") {
        return Type::getVoidTy(*state.ctx);
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
        Type* type;
        APFloat apVal(APFloat::IEEEsingle());
        if (rawValue.back() == 'L') {
            return logError("cannot use L on a floating point value");
        } else if (rawValue.back() == 'D') {
            type = Type::getDoubleTy(*state.ctx);
            apVal = APFloat(APFloat::IEEEdouble(), rawValue.substr(0, rawValue.size() - 1));
        } else {
            type = Type::getFloatTy(*state.ctx);
            apVal = APFloat(APFloat::IEEEsingle(), rawValue);
        }
        return ConstantFP::get(type, apVal);
    } else {
        Type* type;
        APInt apVal;
        if (rawValue.back() == 'D') {
            return logError("cannot use D on an integer value");
        } else if (rawValue.back() == 'L') {
            type = Type::getInt64Ty(*state.ctx);
            apVal = APInt(64, rawValue.substr(0, rawValue.size() - 1), 10);
        } else {
            type = Type::getInt32Ty(*state.ctx);
            apVal = APInt(32, rawValue, 10);
        }
        // todo: check what happens if number is too large
        return ConstantInt::getIntegerValue(type, apVal);
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

    std::string twine = callee->getReturnType()->isVoidTy() ? "" : "call_" + callName->identifier;
    return state.builder->CreateCall(callee,
                                     argsV,
                                     twine);
}

// statements
bool VarAST::codegen(ModuleState& state) {
    logError("variables not supported... yet...");
    return false;
}


bool FuncAST::codegen(ModuleState& state) {
    if (state.module->getFunction(funcName->identifier)) {
        logError("cannot redefine function " + funcName->identifier);
        return false;
    }

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
    if (native) {
        return true;
    }

    // Function block
    if (!block.has_value()) {
        logError("no block given for non native function");
        return false;
    }
    BasicBlock* BB = BasicBlock::Create(*state.ctx, "entry", F);
    state.builder->SetInsertPoint(BB);
    if (!block->get()->codegen(state)) {
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
