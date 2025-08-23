#pragma once

#include <variant>

struct GeneratedValue;
struct GeneratedFunction;
struct GeneratedStruct;

typedef std::variant<GeneratedValue, GeneratedFunction, GeneratedStruct> Identifier;

struct GeneratedType;

typedef std::tuple<std::vector<GeneratedType*>, GeneratedType*> FunctionTypeBacker;
typedef std::variant<std::string, GeneratedType*, FunctionTypeBacker> TypeBacker;
