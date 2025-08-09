#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "utils.h"

std::vector<std::string> split(const std::string& str, const std::string& separator) {
    std::vector<std::string> parts;
    int cur = 0;
    int loc;
    while ((loc = str.find(separator, cur)) != std::string::npos) {
        parts.push_back(str.substr(cur, loc - cur));
        cur = loc + separator.size();
    }
    parts.push_back(str.substr(cur));
    return parts;
}

std::string readStdin() {
    std::cout << "code >" << std::endl;
    std::string text;
    std::string line;
    while (getline(std::cin, line)) {
        text += line + "\n";
    }
    return text;
}

std::string readFile(const std::filesystem::path& filename) {
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

