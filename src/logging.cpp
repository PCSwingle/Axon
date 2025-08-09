#include <iostream>

#include "logging.h"

std::nullptr_t logError(const std::string& error) {
    std::cerr << "Parsing error: " << error << std::endl;
    return nullptr;
}

void logWarning(const std::string& warning) {
    std::cerr << "Warning: " << warning << std::endl;
}

