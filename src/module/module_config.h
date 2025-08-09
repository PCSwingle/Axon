#pragma once

#include <filesystem>

class ModuleConfig {
public:
    std::string name;
    std::string main;

    std::filesystem::path buildFile;
    std::filesystem::path outputFile;

    bool outputLL;

    bool parseArgs(int argc, char* argv[]);

    bool parseConfig();

    std::filesystem::path moduleRoot() const;
};
