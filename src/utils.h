#pragma once

inline std::string typeToString(llvm::Type* type) {
    std::string S;
    llvm::raw_string_ostream OS(S);
    type->print(OS);
    return OS.str();
}
