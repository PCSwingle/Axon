#pragma once
#include <fstream>

inline std::string readStdin() {
    std::cout << "code >" << std::endl;
    std::string text;
    std::string line;
    while (getline(std::cin, line)) {
        text += line + "\n";
    }
    return text;
}

inline std::string readFile(const std::string& filename) {
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
