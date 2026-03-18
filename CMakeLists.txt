cmake_minimum_required(VERSION 3.10)
project(nova_lang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(nova
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast_printer.cpp
    src/interpreter.cpp
    src/vm.cpp
    src/compiler.cpp
    src/disassembler.cpp
)