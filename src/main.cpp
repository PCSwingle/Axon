#include <fstream>
#include <iostream>

#include "utils.h"
#include "lexer/lexer.h"
#include "ast/ast.h"
#include "cli/cli.h"


int main(int argc, char* argv[]) {
    CLIArgs args;
    if (!args.parse(argc, argv)) {
        return 1;
    }

    std::string text = readFile(args.inputFile);
    Lexer lexer(text);

    auto mainBlock = parseBlock(lexer, true);
    ModuleState module;
    if (mainBlock && mainBlock->codegen(module)) {
        std::cout << "successfully compiled!" << std::endl;
        module.writeIR(args.outputFile, !args.outputLL);
    } else {
        std::cout << "did not successfully compile :(";
    }
    return 0;
}
