#include <fstream>
#include <iostream>

#include "lexer/lexer.h"
#include "ast/ast.h"

std::string readStdin() {
    std::cout << "code >" << std::endl;
    std::string text;
    std::string line;
    while (getline(std::cin, line)) {
        text += line + "\n";
    }
    return text;
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return "";
    }
    std::string text;
    std::string line;
    while (getline(file, line)) {
        text += line + "\n";
    }
    file.close();
    return text;
}

int main() {
    std::string text = readFile("test.ax");
    Lexer lexer(text);

    auto mainBlock = parseBlock(lexer, true);
    ModuleState module;
    if (mainBlock && mainBlock->codegen(module)) {
        std::cout << "successfully compiled!" << std::endl;
        module.writeIR("out.bc");
    } else {
        std::cout << "did not successfully compile :(";
    }
    return 0;
}
