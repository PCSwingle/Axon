#include <sstream>

#include "ast.h"

std::string TypeAST::toString() {
    return "Type(" + type + ")";
}

std::string IdentifierExprAST::toString() {
    return "Identifier(" + identifier + ")";
}

std::string ValueExprAST::toString() {
    return "Value(" + std::to_string(value) + ")";
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
    return "Call(" + callName->toString() + "(" + result.str() + "))";
}

std::string VarAST::toString() {
    return "Var(" + (type.has_value() ? type.value()->toString() + " " : "") + identifier->toString() + " = " + expr
           ->toString()
           + ")";
}

std::string SigArg::toString() {
    return type->toString() + " " + identifier->toString();
}

std::string FuncAST::toString() {
    std::ostringstream result;
    for (size_t i = 0; i < signature.size(); i++) {
        result << signature[i].toString();
        if (i != signature.size() - 1) {
            result << ", ";
        }
    }
    return "Func(" + type->toString() + " " + funcName->toString() + "(" + result.str() + ")) " + block->toString();
}

std::string IfAST::toString() {
    return "If(" + expr->toString() + ") " + block->toString();
}

std::string WhileAST::toString() {
    return "While(" + expr->toString() + ") " + block->toString();
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
