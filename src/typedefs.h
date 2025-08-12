#pragma once

#include <variant>

struct GeneratedValue;
struct GeneratedFunction;
struct GeneratedStruct;

typedef std::variant<GeneratedValue, GeneratedFunction, GeneratedStruct> Identifier;
