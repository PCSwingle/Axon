#include <fstream>
#include <iostream>

#include "utils.h"
#include "lexer/lexer.h"
#include "ast/ast.h"
#include "module/module_state.h"
#include "module/module_config.h"

void cleanup() {
    // This makes it easier to find leaks
    GeneratedType::free();
}

int main(const int argc, char* argv[]) {
    ModuleConfig config;
    if (!config.parseArgs(argc, argv) || !config.parseConfig()) {
        return 1;
    }

    ModuleState module(config);
    bool success = module.compileModule() && module.writeIR(config.outputFile, !config.outputLL);
    std::cout << (success ? "successfully compiled!" : "did not successfully compile :(") << std::endl;
    cleanup();
    return success ? 0 : 1;
}
