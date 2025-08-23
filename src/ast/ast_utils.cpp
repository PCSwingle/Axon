#include <sstream>
#include <llvm/Support/raw_ostream.h>

#include "ast.h"
#include "module/generated.h"

std::string GeneratedType::toString() {
    if (isBase()) {
        return std::get<std::string>(type);
    } else if (isArray()) {
        return std::get<GeneratedType*>(type)->toString() + "[]";
    } else if (isFunction()) {
        std::ostringstream result;
        result << "((";
        auto ty = std::get<FunctionTypeBacker>(type);
        auto args = std::get<0>(ty);
        for (const auto&& [i, arg]: std::views::enumerate(args)) {
            result << arg->toString();
            if (i != args.size() - 1) {
                result << ",";
            }
        }
        result << ") -> " << std::get<1>(ty)->toString() << ")";
        return result.str();
    }
    assert(false);
}

std::string ValueExprAST::toString() {
    return rawValue;
}

std::string VariableExprAST::toString() {
    return varName;
}

std::string BinaryOpExprAST::toString() {
    return LHS->toString() + " " + binOp + " " + RHS->toString();
}

std::string UnaryOpExprAST::toString() {
    return unaryOp + expr->toString();
}

std::string CallExprAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < args.size(); i++) {
        result << args[i]->toString();
        if (i != args.size() - 1) {
            result << ", ";
        }
    }
    return callee->toString() + "(" + result.str() + ")";
}

std::string MemberAccessExprAST::toString() {
    return structExpr->toString() + "." + fieldName;
}

std::string SubscriptExprAST::toString() {
    return arrayExpr->toString() + "[" + indexExpr->toString() + "]";
}

std::string ConstructorExprAST::toString() {
    std::ostringstream result;
    for (const auto& [fieldName, fieldValue]: values) {
        result << fieldName << ": " << fieldValue->toString() << ", ";
    }
    auto s = result.str();
    // TODO: don't crash on 0 fields :)
    s.pop_back();
    s.pop_back();
    return "~" + type->toString() + "{" + s + "}";
}

std::string ArrayExprAST::toString() {
    std::ostringstream result;
    for (const auto& value: values) {
        result << value->toString() << ", ";
    }
    auto s = result.str();
    // TODO: don't crash on 0 values :)
    s.pop_back();
    s.pop_back();
    return "~[" + s + "]";
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
    auto sig = "func " + funcName + "(" + result.str() + "): " + returnType->toString();
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
    return "struct " + structName + " {" + result.str() + "}";
}

std::string VarAST::toString() {
    return (definition ? "let " : "") +
           variableExpr->toString() +
           (type.has_value() ? ": " + type.value()->toString() : "") +
           " = " + expr->toString();
}

std::string IfAST::toString() {
    return "if (" + expr->toString() + ") " + block->toString();
}

std::string WhileAST::toString() {
    return "while (" + expr->toString() + ") " + block->toString();
}

std::string ReturnAST::toString() {
    return "return" + (returnExpr.has_value() ? " " + returnExpr->get()->toString() : "");
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
    return "{\n" + final + "\n}";
}

std::string UnitAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < statements.size(); i++) {
        result << statements[i]->toString();
    }
    return result.str();
}
