#include <ranges>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "ast.h"
#include "llvm_utils.h"
#include "module/module_state.h"
#include "module/generated.h"
#include "lexer/lexer.h"

using namespace llvm;

// higher level
std::unique_ptr<GeneratedValue> AssignableAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto maybePointer = codegenPointer(state);
    if (!maybePointer) {
        return nullptr;
    }

    // TODO: this doesn't currently work unless you directly call the function
    if (maybePointer->type->isFunction()) {
        return maybePointer;
    } else {
        Value* val = state.builder->CreateLoad(maybePointer->type->getLLVMType(state),
                                               maybePointer->value,
                                               "pointer_load");
        return std::make_unique<GeneratedValue>(maybePointer->type, val);
    }
}

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
        return state.setError(this->debugInfo, "string literals not implemented yet");
    } else if (rawValue == KW_TRUE) {
        return std::make_unique<GeneratedValue>(GeneratedType::rawGet(KW_BOOL), ConstantInt::getTrue(*state.ctx));
    } else if (rawValue == KW_FALSE) {
        return std::make_unique<GeneratedValue>(GeneratedType::rawGet(KW_BOOL), ConstantInt::getFalse(*state.ctx));
    } else if (rawValue.find('.') != std::string::npos) {
        APFloat apVal(APFloat::IEEEsingle());
        if (impliedType == GeneratedType::rawGet(KW_DOUBLE)) {
            apVal = APFloat(APFloat::IEEEdouble(), rawValue);
        } else if (impliedType == GeneratedType::rawGet(KW_FLOAT)) {
            apVal = APFloat(APFloat::IEEEsingle(), rawValue);
        } else {
            // Default floating type
            impliedType = GeneratedType::rawGet(KW_DOUBLE);
            apVal = APFloat(APFloat::IEEEdouble(), rawValue);
        }
        return std::make_unique<GeneratedValue>(impliedType, ConstantFP::get(impliedType->getLLVMType(state), apVal));
    } else {
        APInt apVal;
        if (impliedType == GeneratedType::rawGet(KW_LONG) || impliedType == GeneratedType::rawGet(KW_ULONG)) {
            apVal = APInt(64, rawValue, 10);
        } else if (impliedType == GeneratedType::rawGet(KW_INT) || impliedType == GeneratedType::rawGet(KW_UINT)) {
            apVal = APInt(32, rawValue, 10);
        } else if (impliedType == GeneratedType::rawGet(KW_BYTE) || impliedType == GeneratedType::rawGet(KW_UBYTE)) {
            apVal = APInt(8, rawValue, 10);
        } else if (impliedType == GeneratedType::rawGet(KW_ISIZE) || impliedType == GeneratedType::rawGet(KW_USIZE)) {
            apVal = APInt(state.sizeTy->getIntegerBitWidth(), rawValue, 10);
        } else {
            // Default int type
            impliedType = GeneratedType::rawGet(KW_INT);
            apVal = APInt(32, rawValue, 10);
        }
        // todo: if number overflows raise error
        return std::make_unique<GeneratedValue>(impliedType,
                                                ConstantInt::getIntegerValue(impliedType->getLLVMType(state), apVal));
    }
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
    if (icmpMap.contains(binOp)) {
        impliedType = nullptr;
    }

    std::unique_ptr<GeneratedValue> L = LHS->codegenValue(state, impliedType);
    std::unique_ptr<GeneratedValue> R = RHS->codegenValue(state, impliedType);
    if (!impliedType && (!L || !R || L->type != R->type)) {
        if (R) {
            auto tryL = LHS->codegenValue(state, R->type);
            if (tryL && tryL->type == R->type) {
                L = std::move(tryL);
            }
        }
        if (L && (!R || L->type != R->type)) {
            auto tryR = RHS->codegenValue(state, L->type);
            if (tryR && tryR->type == L->type) {
                R = std::move(tryR);
            }
        }
    }
    if (!L || !R) {
        return nullptr;
    }
    state.unsetError();

    if (L->type != R->type) {
        return state.setError(this->debugInfo,
                              "Binary expression between two values not the same type; got " + L->type->toString() +
                              " and " + R->type->
                              toString());
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
        type = GeneratedType::rawGet(KW_BOOL);
        val = state.builder->CreateCmp(cmpOp.value(), L->value, R->value, binOp + "_cmpop");
    } else {
        return state.setError(this->debugInfo, "binop " + binOp + " not implemented yet");
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
        return state.setError(this->debugInfo, "unop " + unaryOp + " not implemented yet");
    }
    return std::make_unique<GeneratedValue>(genVal->type, val);
}

std::unique_ptr<GeneratedValue> CallExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto calleeValue = callee->codegenValue(state, nullptr);
    if (!calleeValue) {
        return nullptr;
    }
    if (!calleeValue->type->isFunction()) {
        return state.setError(this->debugInfo, "Type " + calleeValue->type->toString() + " is not callable");
    }

    auto argTypes = calleeValue->type->getArgs();
    if (argTypes.size() != args.size()) {
        return state.setError(this->debugInfo,
                              "Expected " + std::to_string(argTypes.size()) + " arguments, got " +
                              std::to_string(
                                  args.size()) +
                              "arguments");
    }

    std::vector<Value*> argsV;
    for (int i = 0; i < args.size(); i++) {
        auto arg = args[i]->codegenValue(state, argTypes[i]);
        if (!arg) {
            return nullptr;
        }
        if (argTypes[i] != arg->type) {
            return state.setError(this->debugInfo,
                                  "Expected type " + argTypes[i]->toString() + ", got type " + arg->type->toString());
        }
        argsV.push_back(arg->value);
    }

    std::string twine = calleeValue->type->getReturnType()->isVoid() ? "" : "call";
    auto llvmTy = static_cast<FunctionType*>(calleeValue->type->getLLVMType(state));
    auto* val = state.builder->CreateCall(llvmTy,
                                          calleeValue->value,
                                          argsV,
                                          twine);
    return std::make_unique<GeneratedValue>(calleeValue->type->getReturnType(), val);
}

std::unique_ptr<GeneratedValue> ConstructorExprAST::codegenValue(ModuleState& state, GeneratedType* impliedType) {
    auto* genStruct = type->getGenStruct(state);
    if (!genStruct) {
        return state.setError(this->debugInfo,
                              "Attempted to call constructor for undefined or non-struct type " + type->toString());
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
            return state.setError(this->debugInfo,
                                  "struct " + genStruct->type->toString() + " has no field " + fieldName);
        }

        auto fieldValue = fieldExpr->codegenValue(state, std::get<1>(genStruct->fields[fieldIndex.value()]));
        if (!fieldValue) {
            return nullptr;
        }
        auto fieldPointer = structVal->getFieldPointer(state, fieldName);
        if (!fieldPointer) {
            return state.setError(this->debugInfo,
                                  "Could not find field " + fieldName + " on type " + type->toString());
        }
        if (fieldPointer->type != fieldValue->type) {
            return state.setError(this->debugInfo,
                                  "Invalid type for field " + fieldName + "; expected " + fieldPointer->type->toString()
                                  + ", got " +
                                  fieldValue->type->toString());
        }
        state.builder->CreateStore(fieldValue->value, fieldPointer->value);
    }
    for (const auto& [fieldName, _]: genStruct->fields) {
        if (!used.contains(fieldName)) {
            return state.setError(this->debugInfo,
                                  "Field " + fieldName + " required for " + type->toString() + " constructor");
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
            return state.setError(this->debugInfo,
                                  "Mismatched types in array: got both " + genValue->type->toString() + " and " +
                                  genValues[0]->type->
                                  toString());
        }
        baseType = genValue->type;
        genValues.push_back(std::move(genValue));
    }
    if (!baseType) {
        return state.setError(this->debugInfo, "Unable to infer type of array");
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
    auto arrayValue = std::make_unique<GeneratedValue>(baseType->getArrayType(true), arrayFatPointer);

    for (auto&& [i, genValue]: enumerate(genValues)) {
        auto indexValue = std::make_unique<GeneratedValue>(GeneratedType::rawGet(KW_USIZE),
                                                           state.builder->CreateTrunc(
                                                               ConstantInt::get(*state.ctx, APInt(64, i)),
                                                               state.sizeTy));
        auto indexPointer = arrayValue->getArrayPointer(state, indexValue);
        assert(indexPointer);
        state.builder->CreateStore(genValue->value, indexPointer->value);
    }
    return arrayValue;
}

// top level statements
bool ImportAST::codegen(ModuleState& state) {
    if (aliases.size() == 0) {
        state.setError(this->debugInfo, "Import statement with nothing imported found");
        return false;
    }
    return true;
}

bool StructAST::codegen(ModuleState& state) {
    for (const auto& method: methods | std::views::values) {
        if (method->isExtern) {
            state.setError(this->debugInfo, "Structs cannot have extern methods");
            return false;
        }
        if (!method->codegen(state)) {
            return false;
        }
    }
    return true;
}

bool FuncAST::codegen(ModuleState& state) {
    if (!declaration) {
        state.setError(this->debugInfo, "Function not declared (this should not happen!)");
        return false;
    }
    if (!declaration->type->isFunction()) {
        state.setError(this->debugInfo, "Function value not function type (this should not happen!)");
        return false;
    }

    auto* function = static_cast<Function*>(declaration->value);
    for (int i = 0; i < signature.size(); i++) {
        auto arg = function->getArg(i);
        arg->setName(signature[i].identifier);
    }
    if (isExtern) {
        return true;
    }

    // Function block
    if (!block.has_value()) {
        state.setError(this->debugInfo, "No block given for function");
        return false;
    }
    // TODO: store current insert point
    BasicBlock* BB = BasicBlock::Create(*state.ctx, "entry", function);
    state.builder->SetInsertPoint(BB);

    state.enterFunc(declaration.get());
    for (const auto& [type, identifier]: signature) {
        if (!state.registerVar(identifier, type)) {
            state.setError(this->debugInfo,
                           "Duplicate identifier " + identifier + " in signature of function " + funcName);
            return false;
        }
    }

    for (int i = 0; i < signature.size(); i++) {
        auto* genVar = state.getVar(signature[i].identifier);
        if (!genVar->type->isDefined(state)) {
            state.setError(this->debugInfo, "Unknown type " + genVar->type->toString());
            return false;
        }
        auto* arg = function->getArg(i);
        state.builder->CreateStore(arg, genVar->value);
    }
    if (!block->get()->codegen(state)) {
        return false;
    }

    for (auto const& [type, identifier]: signature) {
        state.identifiers.erase(identifier);
    }
    state.exitFunc();

    std::string result;
    raw_string_ostream stream(result);
    if (verifyFunction(*function, &stream)) {
        state.setError(this->debugInfo, "Error verifying function (this should not happen!): " + result);
        return false;
    }
    return true;
}

// statements
bool VarAST::codegen(ModuleState& state) {
    if (type.has_value() && !definition) {
        state.setError(this->debugInfo, "Can only set variable type on definition");
        return false;
    }
    if (definition && varOp != "=") {
        state.setError(this->debugInfo, "Cannot use binary variable assignment operator on variable definition");
    }

    std::unique_ptr<GeneratedValue> varPointer;
    std::unique_ptr<GeneratedValue> value;

    // fml ;)
    if (definition) {
        value = expr->codegenValue(state, type.value_or(nullptr));
        if (!value) {
            return false;
        }

        auto raw = dynamic_cast<VariableExprAST*>(variableExpr.get());
        if (!raw) {
            state.setError(this->debugInfo, "Can only define raw variables");
            return false;
        }
        GeneratedType* varType = type.value_or(value->type);
        if (!varType->isDefined(state)) {
            state.setError(this->debugInfo, "Unknown type " + varType->toString());
            return false;
        }

        if (!state.registerVar(raw->varName, varType)) {
            state.setError(this->debugInfo, "Duplicate identifier " + raw->varName);
            return false;
        }

        varPointer = variableExpr->codegenPointer(state);
        if (!varPointer) {
            return false;
        }
    } else {
        varPointer = variableExpr->codegenPointer(state);
        if (!varPointer) {
            return false;
        }
        if (varOp != "=") {
            auto binOp = varOp.substr(0, varOp.size() - 1);
            expr = std::make_unique<BinaryOpExprAST>(std::move(variableExpr), std::move(expr), binOp);
        }
        value = expr->codegenValue(state, varPointer->type);
        if (!value) {
            return false;
        }
    }

    if (varPointer->type != value->type) {
        state.setError(this->debugInfo,
                       "Wrong type assigned to variable: expected " + varPointer->type->toString() + ", got " +
                       value->type->toString());
        return false;
    }
    state.builder->CreateStore(value->value, varPointer->value);
    return true;
}

bool IfAST::codegen(ModuleState& state) {
    auto val = expr->codegenValue(state, GeneratedType::rawGet(KW_BOOL));
    if (!val) {
        return false;
    }
    if (val->type->isBool()) {
        state.setError(this->debugInfo, "Must use bool type in if statement");
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

    auto val = expr->codegenValue(state, GeneratedType::rawGet(KW_BOOL));
    if (!val) {
        return false;
    }
    if (!val->type->isBool()) {
        state.setError(this->debugInfo, "Must use bool value in while statement");
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
            state.setError(this->debugInfo, "Cannot return a value from a void function");
        }

        auto returnValue = returnExpr->get()->codegenValue(state, returnType);
        if (!returnValue) {
            return false;
        }
        if (returnValue->type != returnType) {
            state.setError(this->debugInfo,
                           "Expected return type of " + returnType->toString() + ", got " + returnValue->type->
                           toString());
            return false;
        }
        state.builder->CreateRet(returnValue->value);
    } else {
        if (!returnType->isVoid()) {
            state.setError(this->debugInfo, "Expected return type of " + returnType->toString() + ", got void");
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
