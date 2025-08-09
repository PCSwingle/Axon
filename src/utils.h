#pragma once

#include <filesystem>
#include <vector>

std::vector<std::string> split(const std::string& str, const std::string& separator);

std::string readStdin();

std::string readFile(const std::filesystem::path& filename);
