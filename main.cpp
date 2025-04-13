#include <fstream>
#include <iostream>

#include "lexer.h"
#include "ast.h"
#include "llvm/IR/Module.h"

using namespace llvm;

std::string readStdin() {
    std::cout << "code >" << std::endl;
    std::string text;
    std::string line;
    while (getline(std::cin, line)) {
        text += line;
    }
    return text;
}

std::string readFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return "";
    }
    std::string text;
    std::string line;
    while (getline(file, line)) {
        text += line;
    }
    file.close();
    return text;
}

int main() {
    std::string text = readFile("test.ax");
    Lexer lexer(text);
    auto mainBlock = parseBlock(lexer, true);
    std::cout << mainBlock->toString() << std::endl;
    return 0;
}
