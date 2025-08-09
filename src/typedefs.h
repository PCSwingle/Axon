#pragma once

#include <variant>

struct GeneratedVariable;
struct GeneratedFunction;
struct GeneratedStruct;

typedef std::variant<GeneratedVariable, GeneratedFunction, GeneratedStruct> Identifier;
