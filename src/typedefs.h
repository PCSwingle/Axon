#pragma once

#include <variant>

struct GeneratedValue;
struct GeneratedStruct;

// TODO: rename to GeneratedIdentifier
typedef std::variant<GeneratedValue, GeneratedStruct> Identifier;

struct GeneratedType;

typedef std::tuple<std::vector<GeneratedType*>, GeneratedType*> FunctionTypeBacker;
typedef std::variant<std::string, GeneratedType*, FunctionTypeBacker> TypeBacker;
