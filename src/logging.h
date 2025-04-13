#pragma once

class AST;

inline nullptr_t logError(const std::string &error) {
    std::cerr << "Parsing error: " << error << std::endl;
    return nullptr;
}
