#include <fstream>
#include <iostream>

#include "utils.h"
#include "lexer/lexer.h"
#include "ast/ast.h"


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
