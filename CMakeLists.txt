cmake_minimum_required(VERSION 3.31)
project(Axon)
set(CMAKE_C_STANDARD 11)

# argparse
include(FetchContent)
FetchContent_Declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

# llvm
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(src)
add_definitions(${LLVM_DEFINITIONS})

add_executable(Axon
        src/main.cpp

        src/ast/ast_parsing.cpp
        src/ast/ast_utils.cpp
        src/ast/ast_codegen.cpp
        src/ast/module_state.cpp

        src/lexer/lexer.cpp
)

target_link_libraries(Axon LLVM argparse)