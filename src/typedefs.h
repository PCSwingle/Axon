#pragma once

#include <variant>

struct GeneratedValue;
struct GeneratedStruct;

typedef std::variant<GeneratedValue, GeneratedStruct> Identifier;

struct GeneratedType;

typedef std::tuple<std::vector<GeneratedType*>, GeneratedType*> FunctionTypeBacker;
typedef std::variant<std::string, GeneratedType*, FunctionTypeBacker> TypeBacker;
