#include <ranges>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "ast.h"
#include "logging.h"
#include "lexer/lexer.h"
#include "utils.h"

using namespace llvm;


Type* GeneratedType::getLLVMType(ModuleState& state) {
    if (type == KW_INT) {
        return Type::getInt32Ty(*state.ctx);
    } else if (type == KW_LONG) {
        return Type::getInt64Ty(*state.ctx);
    } else if (type == KW_FLOAT) {
        return Type::getFloatTy(*state.ctx);
    } else if (type == KW_DOUBLE) {
        return Type::getDoubleTy(*state.ctx);
    } else if (type == KW_BOOL) {
        return Type::getInt1Ty(*state.ctx);
    } else if (type == KW_VOID) {
        return Type::getVoidTy(*state.ctx);
    } else if (state.getStruct(type)) {
        // TODO: this will have to change a little to allow self and pre referencing
        return PointerType::getUnqual(*state.ctx);
    }

    return logError("type " + type + " not implemented yet");
}

// expr
std::unique_ptr<GeneratedValue> ValueExprAST::codegenValue(ModuleState& state) {
    if (rawValue.front() == '\"' || rawValue.front() == '\'') {
        auto strVal = rawValue.substr(1, rawValue.length() - 2);
        return logError("string literals not implemented yet");
    } else if (rawValue == KW_TRUE) {
        return std::make_unique<GeneratedValue>(GeneratedType::get(KW_BOOL), ConstantInt::getTrue(*state.ctx));
    } else if (rawValue == KW_FALSE) {
        return std::make_unique<GeneratedValue>(GeneratedType::get(KW_BOOL), ConstantInt::getFalse(*state.ctx));
    } else if (rawValue.find('.') != std::string::npos) {
        GeneratedType* type;
        APFloat apVal(APFloat::IEEEsingle());
        if (rawValue.back() == 'L') {
            return logError("cannot use L on a floating point value");
        } else if (rawValue.back() == 'D') {
            type = GeneratedType::get(KW_DOUBLE);
            apVal = APFloat(APFloat::IEEEdouble(), rawValue.substr(0, rawValue.size() - 1));
        } else {
            type = GeneratedType::get(KW_FLOAT);
            apVal = APFloat(APFloat::IEEEsingle(), rawValue);
        }
        return std::make_unique<GeneratedValue>(type, ConstantFP::get(type->getLLVMType(state), apVal));
    } else {
        GeneratedType* type;
        APInt apVal;
        if (rawValue.back() == 'D') {
            return logError("cannot use D on an integer value");
        } else if (rawValue.back() == 'L') {
            type = GeneratedType::get(KW_LONG);
            apVal = APInt(64, rawValue.substr(0, rawValue.size() - 1), 10);
        } else {
            type = GeneratedType::get(KW_INT);
            apVal = APInt(32, rawValue, 10);
        }
        // todo: if number overflows raise error
        return std::make_unique<GeneratedValue>(type, ConstantInt::getIntegerValue(type->getLLVMType(state), apVal));
    }
}

std::unique_ptr<GeneratedValue> VariableExprAST::codegenValue(ModuleState& state) {
    GeneratedVariable* genVar = state.getVar(varName);
    if (!genVar) {
        return logError("undefined variable " + varName);
    }
    auto* varAlloca = genVar->varAlloca;
    Value* val = state.builder->CreateLoad(varAlloca->getAllocatedType(),
                                           varAlloca,
                                           varName);
    return std::make_unique<GeneratedValue>(genVar->type, val);
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

static std::optional<Instruction::BinaryOps> getBinop(const std::string& binop, bool floating) {
    if (floating) {
        if (fbinopMap.contains(binop)) {
            return fbinopMap[binop];
        }
    } else {
        if (ibinopMap.contains(binop)) {
            return ibinopMap[binop];
        }
    }
    return std::optional<Instruction::BinaryOps>();
}

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

static std::optional<CmpInst::Predicate> getCmpop(const std::string& cmpop, bool floating) {
    if (floating) {
        if (fcmpMap.contains(cmpop)) {
            return fcmpMap[cmpop];
        }
    } else {
        if (icmpMap.contains(cmpop)) {
            return icmpMap[cmpop];
        }
    }
    return std::optional<CmpInst::Predicate>();
}

std::unique_ptr<GeneratedValue> BinaryOpExprAST::codegenValue(ModuleState& state) {
    auto L = LHS->codegenValue(state);
    auto R = RHS->codegenValue(state);
    if (!L || !R) {
        return nullptr;
    }

    if (L->type != R->type) {
        return logError("binary expression between two values not the same type");
    }
    bool floating = L->type->isFloating();

    std::optional<Instruction::BinaryOps> op = getBinop(binOp, floating);
    std::optional<CmpInst::Predicate> cmpOp = getCmpop(binOp, floating);
    Value* val;
    if (op.has_value()) {
        val = state.builder->CreateBinOp(op.value(), L->value, R->value, binOp + "_binop");
    } else if (cmpOp.has_value()) {
        val = state.builder->CreateCmp(cmpOp.value(), L->value, R->value, binOp + "_cmpop");
    } else {
        return logError("binop " + binOp + " not implemented yet");
    }
    return std::make_unique<GeneratedValue>(L->type, val);
}


std::unique_ptr<GeneratedValue> UnaryOpExprAST::codegenValue(ModuleState& state) {
    auto genVal = expr->codegenValue(state);
    if (!genVal) {
        return nullptr;
    }

    Value* val;
    if (unaryOp == "-") {
        val = state.builder->CreateNeg(genVal->value, unaryOp + "_unop");
    } else {
        return logError("unop " + unaryOp + " not implemented yet");
    }
    return std::make_unique<GeneratedValue>(genVal->type, val);
}


std::unique_ptr<GeneratedValue> CallExprAST::codegenValue(ModuleState& state) {
    auto* genFunction = state.getFunction(callName);
    if (!genFunction) {
        return logError("function " + callName + " not defined");
    }
    auto* callee = genFunction->function;
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
        if (genFunction->signature[i].type != arg->type) {
            return logError("expected type " + genFunction->signature[i].type + ", got type " + arg->type());
        }
        argsV.push_back(arg->value);
    }

    std::string twine = callee->getReturnType()->isVoidTy() ? "" : "call_" + callName;
    auto* val = state.builder->CreateCall(callee,
                                          argsV,
                                          twine);
    return std::make_unique<GeneratedValue>(genFunction->returnType, val);
}

std::unique_ptr<GeneratedValue> AccessorExprAST::codegenValue(ModuleState& state) {
    return logError("accessor not implemented yet");
}


std::unique_ptr<GeneratedValue> ConstructorExprAST::codegenValue(ModuleState& state) {
    auto* genStruct = state.getStruct(structName);
    if (!genStruct) {
        return logError("attempted to call constructor for undefined struct " + structName);
    }

    // todo: free :)
    auto* structPointer = state.builder->CreateMalloc(state.intPtrTy,
                                                      genStruct->structType,
                                                      ConstantExpr::getSizeOf(genStruct->structType),
                                                      nullptr,
                                                      nullptr,
                                                      structName + "_malloc"
    );
    auto used = std::unordered_set<std::string>();
    for (auto&& [i, field]: std::views::enumerate(genStruct->fields)) {
        auto& [fieldName, fieldType] = field;
        if (!values.contains(fieldName)) {
            return logError("constructor for struct " + structName + " missing field " + fieldName);
        }
        used.insert(fieldName);
        // TODO: validate pointer types
        auto fieldValue = values.at(fieldName)->codegenValue(state);
        if (!fieldValue) {
            return nullptr;
        }
        if (fieldType->getLLVMType(state) != fieldValue->getType()) {
            return logError("invalid type for field " + fieldName);
        }
        auto fieldIndices = std::vector<Value*>{
            ConstantInt::get(*state.ctx, APInt(32, 0)),
            ConstantInt::get(*state.ctx, APInt(32, i))
        };
        auto fieldPointer = state.builder->CreateGEP(genStruct->structType,
                                                     structPointer,
                                                     fieldIndices,
                                                     structName + "_" + fieldName);
        state.builder->CreateStore(fieldValue, fieldPointer);
    }
    for (const auto& fieldName: values | std::views::keys) {
        if (!used.contains(fieldName)) {
            return logError("Unknown field " + fieldName + " in constructor for " + structName);
        }
    }
    return std::make_unique<GeneratedValue>(genStruct->type, structPointer);
}


// statements
bool VarAST::codegen(ModuleState& state) {
    auto val = expr->codegenValue(state);
    if (!val) {
        return false;
    }

    if (type.has_value() && !state.registerVar(identifier, type)) {
        logError("duplicate identifier definition " + identifier);
        return false;
    }
    auto* genVar = state.getVar(identifier);
    if (!genVar) {
        logError("reference to undefined variable " + identifier);
        return false;
    }
    if (genVar->type != val->type) {
        logError(
            "wrong type assigned to variable, expected " + genVar->type + ", got " +
            val->type);
        return false;
    }
    state.builder->CreateStore(val->value, genVar->varAlloca);
    return true;
}

bool StructAST::codegen(ModuleState& state) {
    if (!state.registerStruct(structName, fields)) {
        logError("duplicate identifier definition " + structName);
        return false;
    }
    return true;
}


bool FuncAST::codegen(ModuleState& state) {
    // Create prototype
    Type* returnType = type->getLLVMType(state);
    std::vector<Type*> argTypes;
    for (const auto& sig: signature) {
        argTypes.push_back(sig.type->getLLVMType(state));
    }
    FunctionType* type = FunctionType::get(returnType, argTypes, false);
    if (!state.registerFunction(funcName, type)) {
        logError("duplicate identifier definition " + funcName);
        return false;
    }
    auto* genFunction = state.getFunction(funcName);
    auto* function = genFunction->function;

    for (int i = 0; i < signature.size(); i++) {
        auto arg = function->getArg(i);
        arg->setName(signature[i].identifier);
    }
    if (native) {
        return true;
    }

    // Function block
    if (!block.has_value()) {
        logError("no block given for non native function");
        return false;
    }
    // TODO: store current insert point
    BasicBlock* BB = BasicBlock::Create(*state.ctx, "entry", function);
    state.builder->SetInsertPoint(BB);

    state.enterFunc(genFunction);
    for (int i = 0; i < signature.size(); i++) {
        if (!state.registerVar(signature[i].identifier, signature[i].type)) {
            logError("duplicate identifier definition " + signature[i].identifier);
            return false;
        }
        auto* genVar = state.getVar(signature[i].identifier);
        auto* arg = function->getArg(i);
        state.builder->CreateStore(arg, genVar->varAlloca);
    }
    if (!block->get()->codegen(state)) {
        return false;
    }
    state.exitFunc();

    std::string result;
    raw_string_ostream stream(result);
    if (verifyFunction(*function, &stream)) {
        logError("Error verifying function " + funcName + ": " + result);
        return false;
    }
    return true;
}

bool IfAST::codegen(ModuleState& state) {
    auto val = expr->codegenValue(state);
    if (!val) {
        return false;
    }
    if (val->type->isBool()) {
        logError("must use bool type in if statement");
        return false;
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
    state.builder->CreateCondBr(val->value, thenBB, elseBB);

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

    auto val = expr->codegenValue(state);
    if (!val) {
        return false;
    }
    state.builder->CreateCondBr(val->value, loopBB, postBB);

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
    auto* returnType = state.expectedReturnType();
    if (returnExpr.has_value()) {
        if (returnType->isVoid()) {
            logError("cannot return a value from a void function");
        }

        auto returnValue = returnExpr->get()->codegenValue(state);
        if (!returnValue) {
            return false;
        }
        if (returnValue->type != returnType) {
            logError("expected return type of " + returnType + ", got " + returnValue->type);
            return false;
        }
        state.builder->CreateRet(returnValue->value);
    } else {
        if (!returnType->isVoid()) {
            logError("expected return type of " + returnType + ", got void");
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
