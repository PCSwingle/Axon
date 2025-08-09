#include <sstream>
#include <llvm/Support/raw_ostream.h>

#include "ast.h"
#include "module/generated.h"

std::string GeneratedType::toString() {
    return type;
}

std::string ValueExprAST::toString() {
    return "Value(" + rawValue + ")";
}

std::string VariableExprAST::toString() {
    return "Variable(" + varName + ")";
}

std::string BinaryOpExprAST::toString() {
    return "BinOp(" + LHS->toString() + " " + binOp + " " + RHS->toString() + ")";
}

std::string UnaryOpExprAST::toString() {
    return "UnOp(" + unaryOp + expr->toString() + ")";
}

std::string CallExprAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < args.size(); i++) {
        result << args[i]->toString();
        if (i != args.size() - 1) {
            result << ", ";
        }
    }
    return "Call(" + callName + "(" + result.str() + "))";
}

std::string AccessorExprAST::toString() {
    return "(" + structExpr->toString() + ")." + fieldName;
}

std::string ConstructorExprAST::toString() {
    std::ostringstream result;
    for (auto& [fieldName, fieldValue]: values) {
        result << fieldName << ": " << fieldValue->toString() << ", ";
    }
    auto s = result.str();
    s.pop_back();
    s.pop_back();
    return "~" + structName + " { " + s + "}";
}

std::string ImportAST::toString() {
    return "import " + unit;
}

std::string SigArg::toString() {
    return type->toString() + " " + identifier;
}

std::string FuncAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < signature.size(); i++) {
        result << signature[i].toString();
        if (i != signature.size() - 1) {
            result << ", ";
        }
    }
    auto sig = "Func(" + returnType->toString() + " " + funcName + "(" + result.str() + "))";
    if (native) {
        return "native " + sig;
    } else {
        return sig + " " + block->get()->toString();
    }
}

std::string StructAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < fields.size(); i++) {
        result << std::get<0>(fields[i]) << ": " << std::get<1>(fields[i])->toString();
        if (i != fields.size() - 1) {
            result << ", ";
        }
    }
    return "Struct " + structName + " {" + structName + ", " + result.str() + "}";
}

std::string VarAST::toString() {
    return "Var(" + (type.has_value() ? type.value()->toString() + " " : "") + identifier + " = " + expr
           ->toString()
           + ")";
}

std::string IfAST::toString() {
    return "If(" + expr->toString() + ") " + block->toString();
}

std::string WhileAST::toString() {
    return "While(" + expr->toString() + ") " + block->toString();
}

std::string ReturnAST::toString() {
    return "Return(" + (returnExpr.has_value() ? returnExpr->get()->toString() : "") + ")";
}


std::string BlockAST::toString() {
    std::string indent(2, ' ');
    std::ostringstream result;
    for (size_t i = 0; i < statements.size(); i++) {
        result << statements[i]->toString();
        if (i != statements.size() - 1) {
            result << ";\n";
        }
    }

    auto final = indent;
    auto blockStr = result.str();
    size_t pos = 0;
    size_t found;
    while ((found = blockStr.find('\n', pos)) != std::string::npos) {
        final += blockStr.substr(pos, found - pos + 1) + indent;
        pos = found + 1;
    }
    final += blockStr.substr(pos);
    return "Block {\n" + final + "\n}";
}

std::string UnitAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < statements.size(); i++) {
        result << statements[i]->toString();
    }
    return result.str();
}
