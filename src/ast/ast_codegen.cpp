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
    } else if (type == "float") {
        return Type::getFloatTy(*state.ctx);
    } else if (type == "double") {
        return Type::getDoubleTy(*state.ctx);
    } else if (type == "bool") {
        return Type::getInt1Ty(*state.ctx);
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
    if (state.vars.find(varName->identifier) == state.vars.end()) {
        return logError("undefined variable " + varName->identifier);
    }
    AllocaInst* varAlloca = state.vars[varName->identifier];
    Value* val = state.builder->CreateLoad(varAlloca->getAllocatedType(), varAlloca, varName->identifier);
    return val;
}


static std::unordered_map<std::string, Instruction::BinaryOps> ibinopMap{
    {"+", Instruction::Add},
    {"-", Instruction::Sub},
    {"*", Instruction::Mul},
    {"/", Instruction::SDiv},
    {"%", Instruction::SRem},
};
static std::unordered_map<std::string, Instruction::BinaryOps> fbinopMap{
    {"+", Instruction::FAdd},
    {"-", Instruction::FSub},
    {"*", Instruction::FMul},
    {"/", Instruction::FDiv},
    {"%", Instruction::FRem},
};

static std::unordered_map<std::string, CmpInst::Predicate> icmpMap{
    {"==", CmpInst::ICMP_EQ},
    {"!=", CmpInst::ICMP_NE},
    {"<", CmpInst::ICMP_SLE},
    {">", CmpInst::ICMP_SGT},
    {"<=", CmpInst::ICMP_SLE},
    {">=", CmpInst::ICMP_SGE}
};
static std::unordered_map<std::string, CmpInst::Predicate> fcmpMap{
    {"==", CmpInst::FCMP_OEQ},
    {"!=", CmpInst::FCMP_ONE},
    {"<", CmpInst::FCMP_OLE},
    {">", CmpInst::FCMP_OGT},
    {"<=", CmpInst::FCMP_OLE},
    {">=", CmpInst::FCMP_OGE}
};

static std::optional<Instruction::BinaryOps> getBinop(const std::string& binop, bool floating) {
    if (floating) {
        if (fbinopMap.find(binop) != fbinopMap.end()) {
            return fbinopMap[binop];
        }
    } else {
        if (ibinopMap.find(binop) != ibinopMap.end()) {
            return ibinopMap[binop];
        }
    }
    return std::optional<Instruction::BinaryOps>();
}

static std::optional<CmpInst::Predicate> getCmpop(const std::string& cmpop, bool floating) {
    if (floating) {
        if (fcmpMap.find(cmpop) != fcmpMap.end()) {
            return fcmpMap[cmpop];
        }
    } else {
        if (icmpMap.find(cmpop) != icmpMap.end()) {
            return icmpMap[cmpop];
        }
    }
    return std::optional<CmpInst::Predicate>();
}

Value* BinaryOpExprAST::codegenValue(ModuleState& state) {
    auto L = LHS->codegenValue(state);
    auto R = RHS->codegenValue(state);
    if (!L || !R) {
        return nullptr;
    }

    if (L->getType() != R->getType()) {
        return logError("binary expression between two values not the same type");
    }
    bool floating = L->getType() == Type::getDoubleTy(*state.ctx) || L->getType() == Type::getFloatTy(*state.ctx);

    std::optional<Instruction::BinaryOps> op = getBinop(binOp, floating);
    std::optional<CmpInst::Predicate> cmpOp = getCmpop(binOp, floating);
    if (op.has_value()) {
        return state.builder->CreateBinOp(op.value(), L, R, binOp + "_binop");
    } else if (cmpOp.has_value()) {
        return state.builder->CreateCmp(cmpOp.value(), L, R, binOp + "_cmpop");
    } else {
        return logError("binop " + binOp + " currently not supported");
    }
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
    if (type.has_value() && !state.addVar(identifier->identifier, type->get())) {
        return false;
    }
    if (state.vars.find(identifier->identifier) == state.vars.end()) {
        logError("reference to undefined variable " + identifier->identifier);
        return false;
    }
    auto val = expr->codegenValue(state);
    if (!val) {
        return false;
    }

    AllocaInst* varAlloca = state.vars[identifier->identifier];
    if (varAlloca->getAllocatedType() != val->getType()) {
        logError("wrong type assigned to variable");
        return false;
    }
    state.builder->CreateStore(val, varAlloca);
    return true;
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
    for (int i = 0; i < signature.size(); i++) {
        auto arg = F->getArg(i);
        arg->setName(signature[i].identifier->identifier);
    }
    if (native) {
        return true;
    }

    // Function block
    if (!block.has_value()) {
        logError("no block given for non native function");
        return false;
    }
    // todo: store current insert point
    BasicBlock* BB = BasicBlock::Create(*state.ctx, "entry", F);
    state.builder->SetInsertPoint(BB);

    state.enterScope();
    for (int i = 0; i < signature.size(); i++) {
        if (!state.addVar(signature[i].identifier->identifier, signature[i].type.get())) {
            return false;
        }
        auto* arg = F->getArg(i);
        auto* varAlloca = state.vars[signature[i].identifier->identifier];
        state.builder->CreateStore(arg, varAlloca);
    }
    if (!block->get()->codegen(state)) {
        return false;
    }
    state.exitScope();

    verifyFunction(*F);
    return true;
}

bool IfAST::codegen(ModuleState& state) {
    auto* val = expr->codegenValue(state);
    if (!val) {
        return false;
    }
    if (val->getType() != Type::getInt1Ty(*state.ctx)) {
        logError("must use bool type in if statement");
    }

    Function* func = state.builder->GetInsertBlock()->getParent();
    BasicBlock* thenBB = BasicBlock::Create(*state.ctx, "then");
    BasicBlock* elseBB;
    BasicBlock* mergeBB = BasicBlock::Create(*state.ctx, "postif");
    if (elseBlock.has_value()) {
        elseBB = BasicBlock::Create(*state.ctx, "else");
    } else {
        elseBB = mergeBB;
    }
    state.builder->CreateCondBr(val, thenBB, elseBB);

    func->insert(func->end(), thenBB);
    state.builder->SetInsertPoint(thenBB);
    if (!block->codegen(state)) {
        return false;
    }
    state.builder->CreateBr(mergeBB);

    if (elseBlock.has_value()) {
        func->insert(func->end(), elseBB);
        state.builder->SetInsertPoint(elseBB);
        if (!elseBlock.value()->codegen(state)) {
            return false;
        }
        state.builder->CreateBr(mergeBB);
    }

    func->insert(func->end(), mergeBB);
    state.builder->SetInsertPoint(mergeBB);

    return true;
}

bool WhileAST::codegen(ModuleState& state) {
    Function* func = state.builder->GetInsertBlock()->getParent();
    BasicBlock* condBB = BasicBlock::Create(*state.ctx, "cond");
    BasicBlock* loopBB = BasicBlock::Create(*state.ctx, "loop");
    BasicBlock* postBB = BasicBlock::Create(*state.ctx, "postloop");

    func->insert(func->end(), condBB);
    state.builder->CreateBr(condBB);
    state.builder->SetInsertPoint(condBB);

    Value* val = expr->codegenValue(state);
    if (!val) {
        return false;
    }
    state.builder->CreateCondBr(val, loopBB, postBB);

    func->insert(func->end(), loopBB);
    state.builder->SetInsertPoint(loopBB);
    if (!block->codegen(state)) {
        return false;
    }
    state.builder->CreateBr(condBB);

    func->insert(func->end(), postBB);
    state.builder->SetInsertPoint(postBB);
    return true;
}

bool ReturnAST::codegen(ModuleState& state) {
    auto returnType = state.builder->GetInsertBlock()->getParent()->getReturnType();
    if (returnExpr.has_value()) {
        Value* returnValue = returnExpr->get()->codegenValue(state);
        if (!returnValue) {
            return false;
        }
        if (returnValue->getType() != returnType) {
            logError("returned invalid type");
            return false;
        }
        state.builder->CreateRet(returnValue);
    } else {
        if (!returnType->isVoidTy()) {
            logError("returned invalid type");
            return false;
        }
        state.builder->CreateRetVoid();
    }
    return true;
}


// other
bool BlockAST::codegen(ModuleState& state) {
    state.enterScope();
    for (const auto& statement: statements) {
        if (!statement->codegen(state)) {
            return false;
        }
    }
    state.exitScope();
    return true;
}
