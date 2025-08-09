cmake_minimum_required(VERSION 3.31)
project(Axon)
set(CMAKE_CXX_STANDARD 23)

# dependencies
include(FetchContent)
FetchContent_Declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(argparse tomlplusplus)

# llvm
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(src)
add_definitions(${LLVM_DEFINITIONS})

add_executable(Axon
        src/main.cpp
        src/utils.cpp
        src/logging.cpp

        src/ast/ast_parsing.cpp
        src/ast/ast_utils.cpp
        src/ast/ast_codegen.cpp
        src/ast/ast_register.cpp
        src/ast/llvm_utils.cpp

        src/module/generated.cpp
        src/module/module_state.cpp
        src/module/module_config.cpp

        src/lexer/lexer.cpp
)

target_link_libraries(Axon LLVM argparse tomlplusplus::tomlplusplus)