#pragma once

#include <filesystem>
#include <vector>

#include "typedefs.h"

struct GeneratedType;

std::vector<std::string> split(const std::string& str, const std::string& separator);

std::string readStdin();

std::string readFile(const std::filesystem::path& filename);

struct TypeBackerHash {
    size_t operator()(const TypeBacker& type) const;

    size_t operator()(const std::string& type) const;

    size_t operator()(GeneratedType* type) const;

    size_t operator()(const FunctionTypeBacker& type) const;
};
