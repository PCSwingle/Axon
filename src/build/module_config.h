#pragma once

#include <toml++/toml.hpp>
#include <argparse/argparse.hpp>

class ModuleConfig {
public:
    std::string name;
    std::string main;

    std::filesystem::path buildFile;
    std::filesystem::path outputFile;

    bool outputLL;

    bool parseArgs(int argc, char* argv[]) {
        argparse::ArgumentParser program("Axon");
        program.add_argument("build-file").default_value(".").help("the build file");

        program.add_argument("--output-file", "-o").help("the output file");
        program.add_argument("--output-ll", "-l").help("output human readable ir instead of bitcode").flag();

        try {
            program.parse_args(argc, argv);
        } catch (const std::exception& err) {
            std::cout << "Error parsing arguments: " << err.what() << std::endl;
            std::cout << program;
            return false;
        }

        buildFile = program.get("build-file");
        outputLL = program.get<bool>("--output-ll");
        if (auto file = program.present("output-file")) {
            outputFile = *file;
        } else {
            auto ext = outputLL ? ".ll" : ".bc";
            std::filesystem::path p(buildFile);
            p.replace_extension(ext);
            outputFile = p.string();
        }
        return true;
    }

    bool parseConfig() {
        if (!exists(buildFile) || !is_regular_file(buildFile)) {
            std::cout << "File " << buildFile.string() << " does not exist" << std::endl;
            return false;
        }

        toml::parse_result buildConfig;
        try {
            buildConfig = toml::parse_file(buildFile.c_str());
        } catch (toml::parse_error err) {
            std::cout << "Error parsing " << buildFile.string() << ": " << err << std::endl;
            return false;
        }

        auto maybeName = buildConfig["name"].value<std::string>();
        if (!maybeName.has_value()) {
            std::cout << "Error parsing " << buildFile.string() << ": module name required" << std::endl;
            return false;
        }
        name = maybeName.value();

        auto maybeMain = buildConfig["main"].value<std::string>();
        if (!maybeMain.has_value()) {
            std::cout << "Error parsing " << buildFile.string() << ": main required" << std::endl;
            return false;
        }
        main = maybeMain.value();

        return true;
    }

    std::filesystem::path moduleRoot() const {
        return buildFile.parent_path();
    }
};
