cmake_minimum_required(VERSION 3.31)
project(Axon)

set(CMAKE_C_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(Axon main.cpp)
target_link_libraries(Axon LLVM)