#include <map>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "ast.h"
#include "logging.h"
#include "lexer/lexer.h"
#include "utils.h"

using namespace llvm;

// TODO: pass this through codegen instead of being global (maybe bundle together?)
static std::unique_ptr<LLVMContext> Context;
static std::unique_ptr<IRBuilder<> > Builder;
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value*> NamedValues;

Type* TypeAST::getType() {
    if (type == "int") {
        return Type::getInt32Ty(*Context);
    } else if (type == "long") {
        return Type::getInt64Ty(*Context);
    } else if (type == "double") {
        return Type::getDoubleTy(*Context);
    }

    return logError("type " + type + " currently not supported");
}

// expr
Value* ValueExprAST::codegenValue() {
    if (rawValue == KW_TRUE) {
        return ConstantInt::getTrue(*Context);
    } else if (rawValue == KW_FALSE) {
        return ConstantInt::getFalse(*Context);
    } else if (rawValue.find('.') != std::string::npos) {
        return ConstantFP::get(Type::getDoubleTy(*Context), APFloat(APFloat::IEEEdouble(), rawValue));
    } else {
        return ConstantInt::getIntegerValue(Type::getInt64Ty(*Context), APInt(64, rawValue, 10));
    }
}

llvm::Value* VariableExprAST::codegenValue() {
    return logError("variables not supported... yet...");
}


Value* BinaryOpExprAST::codegenValue() {
    auto L = LHS->codegenValue();
    auto R = RHS->codegenValue();
    if (!L || !R) {
        return nullptr;
    }

    if (binOp == "+") {
        return Builder->CreateAdd(L, R, "add_binop");
    } else if (binOp == "-") {
        return Builder->CreateSub(L, R, "sub_binop");
    } else if (binOp == "*") {
        return Builder->CreateMul(L, R, "mul_binop");
    } else if (binOp == "/") {
        return Builder->CreateSDiv(L, R, "div_binop");
    } else if (binOp == "%") {
        return Builder->CreateSRem(L, R, "mod_binop");
    }

    return logError("binop " + binOp + " currently not supported");
}


Value* UnaryOpExprAST::codegenValue() {
    auto ex = expr->codegenValue();
    if (!ex) {
        return nullptr;
    }

    if (unaryOp == "-") {
        return Builder->CreateNeg(ex, "neg_binop");
    }

    return logError("unop " + unaryOp + " currently not supported");
}


Value* CallExprAST::codegenValue() {
    Function* callee = TheModule->getFunction(callName->identifier);
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
        auto arg = args[i]->codegenValue();
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
    return Builder->CreateCall(callee, argsV, "call_" + callName->identifier);
}

// statements
bool VarAST::codegen() {
    logError("variables not supported... yet...");
    return false;
}


bool FuncAST::codegen() {
    // Create prototype
    Type* returnType = type->getType();
    std::vector<Type*> argTypes;
    for (const auto& sig: signature) {
        argTypes.push_back(sig.type->getType());
    }
    FunctionType* FT = FunctionType::get(returnType, argTypes, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, funcName->identifier, TheModule.get());
    NamedValues.clear();
    for (int i = 0; i < signature.size(); i++) {
        auto Arg = F->getArg(i);
        Arg->setName(signature[i].identifier->identifier);
        NamedValues[signature[i].identifier->identifier] = Arg;
    }

    // Function block
    BasicBlock* BB = BasicBlock::Create(*Context, "entry", F);
    Builder->SetInsertPoint(BB);
    if (!block->codegen()) {
        return false;
    }
    verifyFunction(*F);
    return true;
}

bool IfAST::codegen() {
    logError("if not supported... yet...");
    return false;
}

bool WhileAST::codegen() {
    logError("while not supported... yet...");
    return false;
}

// other
bool BlockAST::codegen() {
    for (const auto& statement: statements) {
        if (!statement->codegen()) {
            return false;
        }
    }
    return true;
}
