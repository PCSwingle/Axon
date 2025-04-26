#pragma once

#include <argparse/argparse.hpp>

class CLIArgs {
public:
    std::string inputFile;
    std::string outputFile;

    bool outputLL;

    bool parse(int argc, char* argv[]) {
        argparse::ArgumentParser program("Axon");
        program.add_argument("input-file").help("the file to compile");

        program.add_argument("--output-file", "-o").help("the output file");
        program.add_argument("--output-ll", "-l").help("output human readable ir instead of bitcode").flag();

        try {
            program.parse_args(argc, argv);
        } catch (const std::exception& err) {
            std::cerr << err.what() << std::endl;
            std::cerr << program;
            return false;
        }

        inputFile = program.get("input-file");
        outputLL = program.get<bool>("--output-ll");
        if (auto file = program.present("output-file")) {
            outputFile = *file;
        } else {
            auto ext = outputLL ? ".ll" : ".bc";
            std::filesystem::path p(inputFile);
            p.replace_extension(ext);
            outputFile = p.string();
        }
        return true;
    }
};
