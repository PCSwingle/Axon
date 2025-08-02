#include <fstream>
#include <iostream>

#include "utils.h"
#include "lexer/lexer.h"
#include "ast/ast.h"
#include "ast/module_state.h"
#include "cli/cli.h"

void cleanup() {
    // This makes it easier to find leaks
    GeneratedType::free();
}

int main(int argc, char* argv[]) {
    CLIArgs args;
    if (!args.parse(argc, argv)) {
        return 1;
    }

    std::string text = readFile(args.inputFile);
    Lexer lexer(text);

    auto mainBlock = parseBlock(lexer, true);
    ModuleState module;
    bool success = mainBlock && mainBlock->codegen(module) && module.writeIR(args.outputFile, !args.outputLL);
    std::cout << (success ? "successfully compiled!" : "did not successfully compile :(") << std::endl;
    cleanup();
    return success ? 0 : 1;
}
