#include <ostream>
#include <iostream>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Passes/CodeGenPassBuilder.h>
#include <llvm/Support/FileSystem.h>

#include "debug_consts.h"
#include "module_state.h"

using namespace llvm;

bool ModuleState::writeIR(const std::string& filename, bool bitcode) {
    if (DEBUG_CODEGEN_PRINT_MODULE) {
        std::cout << "full ir:" << std::endl;
        module->print(outs(), nullptr);
    }

    std::error_code EC;
    raw_fd_ostream out(filename, EC, sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file for writing: " << EC.message() << "\n";
        return false;
    }
    if (bitcode) {
        WriteBitcodeToFile(*module, out);
    } else {
        module->print(out, nullptr);
    }
    out.close();
    return true;
}

AllocaInst* ModuleState::createAlloca(TypeAST* type, std::string& name) {
    auto oldIP = builder->saveIP();
    auto& entry = builder->GetInsertBlock()->getParent()->getEntryBlock();
    builder->SetInsertPoint(&entry, entry.begin());
    auto* newAlloca = builder->CreateAlloca(type->getType(*this), nullptr, name);
    builder->restoreIP(oldIP);
    return newAlloca;
}
