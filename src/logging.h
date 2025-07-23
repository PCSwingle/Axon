#pragma once
#include <iostream>

inline nullptr_t logError(const std::string& error) {
    std::cerr << "Parsing error: " << error << std::endl;
    return nullptr;
}

inline void logWarning(const std::string& warning) {
    std::cerr << "Warning: " << warning << std::endl;
}
