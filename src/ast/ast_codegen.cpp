#include <ranges>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "ast.h"
#include "llvm_utils.h"
#include "logging.h"
#include "module/module_state.h"
#include "module/generated.h"
#include "lexer/lexer.h"

using namespace llvm;

// expr
bool ExprAST::codegen(ModuleState& state) {
    if (!codegenValue(state, nullptr)) {
        return false;
    } else {
        return true;
    }
}

std::unique_ptr<GeneratedValue> ValueExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    if (rawValue.front() == '\"' || rawValue.front() == '\'') {
        auto strVal = rawValue.substr(1, rawValue.length() - 2);
        return logError("string literals not implemented yet");
    } else if (rawValue == KW_TRUE) {
        return std::make_unique<GeneratedValue>(GeneratedType::get(KW_BOOL), ConstantInt::getTrue(*state.ctx));
    } else if (rawValue == KW_FALSE) {
        return std::make_unique<GeneratedValue>(GeneratedType::get(KW_BOOL), ConstantInt::getFalse(*state.ctx));
    } else if (rawValue.find('.') != std::string::npos) {
        APFloat apVal(APFloat::IEEEsingle());
        if (impliedType == GeneratedType::get(KW_F64)) {
            apVal = APFloat(APFloat::IEEEdouble(), rawValue);
        } else if (impliedType == GeneratedType::get(KW_F32)) {
            apVal = APFloat(APFloat::IEEEsingle(), rawValue);
        } else {
            // Default floating type
            impliedType = GeneratedType::get(KW_F64);
            apVal = APFloat(APFloat::IEEEdouble(), rawValue);
        }
        return std::make_unique<GeneratedValue>(impliedType, ConstantFP::get(impliedType->getLLVMType(state), apVal));
    } else {
        APInt apVal;
        if (impliedType == GeneratedType::get(KW_I64) || impliedType == GeneratedType::get(KW_U64)) {
            apVal = APInt(64, rawValue, 10);
        } else if (impliedType == GeneratedType::get(KW_I32) || impliedType == GeneratedType::get(KW_U32)) {
            apVal = APInt(32, rawValue, 10);
        } else if (impliedType == GeneratedType::get(KW_I8) || impliedType == GeneratedType::get(KW_U8)) {
            apVal = APInt(8, rawValue, 10);
        } else if (impliedType == GeneratedType::get(KW_ISIZE) || impliedType == GeneratedType::get(KW_USIZE)) {
            apVal = APInt(state.sizeTy->getIntegerBitWidth(), rawValue, 10);
        } else {
            // Default int type
            impliedType = GeneratedType::get(KW_I32);
            apVal = APInt(32, rawValue, 10);
        }
        // todo: if number overflows raise error
        return std::make_unique<GeneratedValue>(impliedType,
                                                ConstantInt::getIntegerValue(impliedType->getLLVMType(state), apVal));
    }
}

std::unique_ptr<GeneratedValue> VariableExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto pointer = codegenPointer(state);
    if (!pointer) {
        return nullptr;
    }
    Value* val = state.builder->CreateLoad(pointer->type->getLLVMType(state),
                                           pointer->value,
                                           varName);
    return std::make_unique<GeneratedValue>(pointer->type, val);
}


static std::unordered_map<std::string, Instruction::BinaryOps> ibinopMap{
    {"+", Instruction::Add},
    {"-", Instruction::Sub},
    {"*", Instruction::Mul},
    {"/", Instruction::SDiv},
    {"%", Instruction::SRem},
};
static std::unordered_map<std::string, Instruction::BinaryOps> ubinopMap{
    {"+", Instruction::Add},
    {"-", Instruction::Sub},
    {"*", Instruction::Mul},
    {"/", Instruction::UDiv},
    {"%", Instruction::URem},
};
static std::unordered_map<std::string, Instruction::BinaryOps> fbinopMap{
    {"+", Instruction::FAdd},
    {"-", Instruction::FSub},
    {"*", Instruction::FMul},
    {"/", Instruction::FDiv},
    {"%", Instruction::FRem},
};

static std::optional<Instruction::BinaryOps> getBinop(const std::string& binop,
                                                      const bool isSigned,
                                                      const bool isFloating) {
    if (isFloating) {
        if (fbinopMap.contains(binop)) {
            return fbinopMap[binop];
        }
    } else if (isSigned) {
        if (ibinopMap.contains(binop)) {
            return ibinopMap[binop];
        }
    } else {
        if (ubinopMap.contains(binop)) {
            return ubinopMap[binop];
        }
    }
    return std::optional<Instruction::BinaryOps>();
}

static std::unordered_map<std::string, CmpInst::Predicate> icmpMap{
    {"==", CmpInst::ICMP_EQ},
    {"!=", CmpInst::ICMP_NE},
    {"<", CmpInst::ICMP_SLT},
    {">", CmpInst::ICMP_SGT},
    {"<=", CmpInst::ICMP_SLE},
    {">=", CmpInst::ICMP_SGE}
};
static std::unordered_map<std::string, CmpInst::Predicate> ucmpMap{
    {"==", CmpInst::ICMP_EQ},
    {"!=", CmpInst::ICMP_NE},
    {"<", CmpInst::ICMP_ULT},
    {">", CmpInst::ICMP_UGT},
    {"<=", CmpInst::ICMP_ULE},
    {">=", CmpInst::ICMP_UGE}
};
static std::unordered_map<std::string, CmpInst::Predicate> fcmpMap{
    {"==", CmpInst::FCMP_OEQ},
    {"!=", CmpInst::FCMP_ONE},
    {"<", CmpInst::FCMP_OLT},
    {">", CmpInst::FCMP_OGT},
    {"<=", CmpInst::FCMP_OLE},
    {">=", CmpInst::FCMP_OGE}
};

static std::optional<CmpInst::Predicate> getCmpop(const std::string& cmpop,
                                                  const bool isSigned,
                                                  const bool isFloating) {
    if (isFloating) {
        if (fcmpMap.contains(cmpop)) {
            return fcmpMap[cmpop];
        }
    } else if (isSigned) {
        if (icmpMap.contains(cmpop)) {
            return icmpMap[cmpop];
        }
    } else {
        if (ucmpMap.contains(cmpop)) {
            return ucmpMap[cmpop];
        }
    }
    return std::optional<CmpInst::Predicate>();
}

std::unique_ptr<GeneratedValue> BinaryOpExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    // TODO: Missing out on an implicit cast here that would allow us to do `let x = 2 + <var_with_type>`.
    // Once semantic analysis and codegen are split up this will be possible.
    if (icmpMap.contains(binOp)) {
        impliedType = nullptr;
    }
    auto L = LHS->codegenValue(state, impliedType);
    auto R = RHS->codegenValue(state, impliedType);
    if (!L || !R) {
        return nullptr;
    }

    if (L->type != R->type) {
        return logError(
            "binary expression between two values not the same type; got " + L->type->toString() + " and " + R->type->
            toString() + ". Expression: " + this->toString());
    }
    bool isSigned = L->type->isSigned();
    bool isFloating = L->type->isFloating();

    // TODO: clean all of this up (with cmpop check up there as well)
    std::optional<Instruction::BinaryOps> op = getBinop(binOp, isSigned, isFloating);
    std::optional<CmpInst::Predicate> cmpOp = getCmpop(binOp, isSigned, isFloating);
    GeneratedType* type;
    Value* val;
    if (op.has_value()) {
        type = L->type;
        val = state.builder->CreateBinOp(op.value(), L->value, R->value, binOp + "_binop");
    } else if (cmpOp.has_value()) {
        type = GeneratedType::get(KW_BOOL);
        val = state.builder->CreateCmp(cmpOp.value(), L->value, R->value, binOp + "_cmpop");
    } else {
        return logError("binop " + binOp + " not implemented yet");
    }
    return std::make_unique<GeneratedValue>(type, val);
}


std::unique_ptr<GeneratedValue> UnaryOpExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto genVal = expr->codegenValue(state, impliedType);
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


std::unique_ptr<GeneratedValue> CallExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
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
        auto arg = args[i]->codegenValue(state, genFunction->signature[i].type);
        if (!arg) {
            return nullptr;
        }
        if (genFunction->signature[i].type != arg->type) {
            return logError(
                "expected type " + genFunction->signature[i].type->toString() + ", got type " + arg->type->
                toString() + ". Call: " + toString());
        }
        argsV.push_back(arg->value);
    }

    std::string twine = callee->getReturnType()->isVoidTy() ? "" : "call_" + callName;
    auto* val = state.builder->CreateCall(callee,
                                          argsV,
                                          twine);
    return std::make_unique<GeneratedValue>(genFunction->returnType, val);
}

std::unique_ptr<GeneratedValue> MemberAccessExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto pointer = codegenPointer(state);
    if (!pointer) {
        return nullptr;
    }
    auto val = state.builder->CreateLoad(pointer->type->getLLVMType(state),
                                         pointer->value,
                                         "member_access_" + fieldName);
    return std::make_unique<GeneratedValue>(pointer->type, val);
}

std::unique_ptr<GeneratedValue> SubscriptExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto pointer = codegenPointer(state);
    if (!pointer) {
        return nullptr;
    }
    auto val = state.builder->CreateLoad(pointer->type->getLLVMType(state), pointer->value, "array_access");
    return std::make_unique<GeneratedValue>(pointer->type, val);
}

std::unique_ptr<GeneratedValue> ConstructorExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto* genStruct = type->getGenStruct(state);
    if (!genStruct) {
        return logError("attempted to call constructor for undefined or non-struct type " + type->toString());
    }

    auto* structPointer = createMalloc(state,
                                       state.builder->CreateTrunc(ConstantExpr::getSizeOf(genStruct->structType),
                                                                  state.sizeTy),
                                       type->toString());
    auto structVal = std::make_unique<GeneratedValue>(genStruct->type, structPointer);

    auto used = std::unordered_set<std::string>();
    for (auto& [fieldName, fieldExpr]: values) {
        used.insert(fieldName);
        auto fieldIndex = genStruct->getFieldIndex(fieldName);
        if (!fieldIndex.has_value()) {
            return logError("struct " + genStruct->type->toString() + " has no field " + fieldName);
        }

        auto fieldValue = fieldExpr->codegenValue(state, std::get<1>(genStruct->fields[fieldIndex.value()]));
        if (!fieldValue) {
            return nullptr;
        }
        auto fieldPointer = structVal->getFieldPointer(state, fieldName);
        if (!fieldPointer) {
            return nullptr;
        }
        if (fieldPointer->type != fieldValue->type) {
            return logError(
                "invalid type for field " + fieldName + "; expected " + fieldPointer->type->toString() + ", got " +
                fieldValue->type->toString());
        }
        state.builder->CreateStore(fieldValue->value, fieldPointer->value);
    }
    for (const auto& [fieldName, _]: genStruct->fields) {
        if (!used.contains(fieldName)) {
            return logError("Field " + fieldName + " required for " + type->toString() + " constructor");
        }
    }
    return structVal;
}

std::unique_ptr<GeneratedValue> ArrayExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    std::vector<std::unique_ptr<GeneratedValue> > genValues;
    GeneratedType* baseType = impliedType ? impliedType->getArrayBase() : nullptr;
    for (const auto& expr: values) {
        auto genValue = expr->codegenValue(state, baseType);
        if (!genValue) {
            return nullptr;
        }
        if (baseType && genValue->type != baseType) {
            return logError(
                "mismatched types in array: got both " + genValue->type->toString() + " and " + genValues[0]->type->
                toString());
        }
        baseType = genValue->type;
        genValues.push_back(std::move(genValue));
    }
    if (!baseType) {
        return logError("unable to infer type of array " + toString());
    }

    auto* typeSize = state.builder->
            CreateTrunc(ConstantExpr::getSizeOf(baseType->getLLVMType(state)), state.sizeTy);
    auto* allocSize = state.builder->CreateMul(ConstantInt::get(state.sizeTy, genValues.size()), typeSize);

    // TODO: figure out if we need to align anything ever? I don't think we do as long as we ensure structs are aligned
    auto* arrayPointer = createMalloc(state, allocSize, "array");
    auto* fatPointerStruct = ConstantStruct::get(state.arrFatPtrTy,
                                                 std::vector<Constant*>{
                                                     UndefValue::get(PointerType::getUnqual(*state.ctx)),
                                                     ConstantInt::get(state.sizeTy, APInt(64, values.size()))
                                                 }
    );
    auto* arrayFatPointer = state.builder->CreateInsertValue(fatPointerStruct,
                                                             arrayPointer,
                                                             std::vector<unsigned>{0},
                                                             "arr_ptr_insert");
    auto arrayValue = std::make_unique<GeneratedValue>(baseType->toArray(), arrayFatPointer);

    for (auto&& [i, genValue]: enumerate(genValues)) {
        auto indexValue = std::make_unique<GeneratedValue>(GeneratedType::get(KW_USIZE),
                                                           state.builder->CreateTrunc(
                                                               ConstantInt::get(*state.ctx, APInt(64, i)),
                                                               state.sizeTy));
        auto indexPointer = arrayValue->getArrayPointer(state, indexValue);
        state.builder->CreateStore(genValue->value, indexPointer->value);
    }
    return arrayValue;
}

// top level statements
bool ImportAST::codegen(ModuleState& state) {
    return true;
}

bool StructAST::codegen(ModuleState& state) {
    return true;
}

bool FuncAST::codegen(ModuleState& state) {
    auto* genFunction = state.getFunction(funcName);
    if (!genFunction) {
        logError("function " + funcName + " not registered (somehow)");
        return false;
    }

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
        auto* genVar = state.getVar(signature[i].identifier);
        if (!genVar->type->isDefined(state)) {
            logError("unknown type " + genVar->type->toString());
            return false;
        }
        auto* arg = function->getArg(i);
        state.builder->CreateStore(arg, genVar->value);
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

// statements
bool VarAST::codegen(ModuleState& state) {
    std::unique_ptr<GeneratedValue> varPointer;
    std::unique_ptr<GeneratedValue> value;

    // fml ;)
    assert(!(definition && varOp != "="));
    if (definition) {
        value = expr->codegenValue(state, type.value_or(nullptr));

        auto raw = dynamic_cast<VariableExprAST*>(variableExpr.get());
        if (!raw) {
            logError("can only define raw variables");
            return false;
        }
        GeneratedType* varType = type.value_or(value->type);
        if (!varType->isDefined(state)) {
            logError("unknown type " + varType->toString());
            return false;
        }

        if (!state.registerVar(raw->varName, varType)) {
            return false;
        }

        varPointer = variableExpr->codegenPointer(state);
    } else {
        varPointer = variableExpr->codegenPointer(state);
        if (varOp != "=") {
            auto binOp = varOp.substr(0, varOp.size() - 1);
            expr = std::make_unique<BinaryOpExprAST>(std::move(variableExpr), std::move(expr), binOp);
        }
        value = expr->codegenValue(state, varPointer->type);
    }

    if (!varPointer || !value) {
        return false;
    }

    if (varPointer->type != value->type) {
        logError(
            "wrong type assigned to variable: expected " + varPointer->type->toString() + ", got " +
            value->type->toString());
        return false;
    }
    state.builder->CreateStore(value->value, varPointer->value);
    return true;
}

bool IfAST::codegen(ModuleState& state) {
    auto val = expr->codegenValue(state, GeneratedType::get(KW_BOOL));
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

    auto val = expr->codegenValue(state, GeneratedType::get(KW_BOOL));
    if (!val) {
        return false;
    }
    if (!val->type->isBool()) {
        logError("must use bool value in while statement");
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

        auto returnValue = returnExpr->get()->codegenValue(state, returnType);
        if (!returnValue) {
            return false;
        }
        if (returnValue->type != returnType) {
            logError("expected return type of " + returnType->toString() + ", got " + returnValue->type->toString());
            return false;
        }
        state.builder->CreateRet(returnValue->value);
    } else {
        if (!returnType->isVoid()) {
            logError("expected return type of " + returnType->toString() + ", got void");
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

bool UnitAST::codegen(ModuleState& state) {
    state.enterScope();
    for (const auto& statement: statements) {
        if (!statement->postregister(state, unit)) {
            return false;
        }
    }

    for (const auto& statement: statements) {
        if (!statement->codegen(state)) {
            return false;
        }
    }
    state.exitScope();
    return true;
}
