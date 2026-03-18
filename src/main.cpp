#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "interpreter.h"
#include "disassembler.h"

using namespace nova;

// NovaLang version
static const char* VERSION = "0.1.1";

// Helper to read file
static std::string slurpFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Get directory from file path (cross-platform)
static std::string getDirectory(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) {
        return ".";
    }
    return filepath.substr(0, pos);
}

// Compile source to bytecode (without running)
static bool compileToBytecode(const std::string& src, const std::string& filePath, BytecodeProgram& program) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    tokens.push_back({TokenType::EndOfFile, "", 1, 0});

    Parser parser(tokens);
    auto programAst = parser.parseProgram();

    if (parser.hasErrors()) {
        std::cerr << "Parse errors:\n";
        for (const auto& err : parser.getErrors())
            std::cerr << "  Line " << err.line << ", Col " << err.column << ": " << err.message << "\n";
        return false;
    }

    Program* progPtr = dynamic_cast<Program*>(programAst.get());
    if (!progPtr) {
        std::cerr << "Failed to create program AST\n";
        return false;
    }

    Compiler compiler;
    program = compiler.compile(*progPtr);
    return true;
}

// Run source code (from file or REPL)
bool runSource(const std::string& src, Interpreter& interpreter, const std::string& filePath = "", bool isREPL = false, int lineNo = 1) {
    // Set the current file directory for local imports
    if (!filePath.empty()) {
        interpreter.setCurrentFileDir(getDirectory(filePath));
    }
    
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    tokens.push_back({TokenType::EndOfFile, "", lineNo, 0});

    Parser parser(tokens);
    auto programAst = parser.parseProgram();

    if (parser.hasErrors()) {
        std::cerr << "Parse errors:\n";
        for (const auto& err : parser.getErrors())
            std::cerr << "  Line " << err.line << ", Col " << err.column << ": " << err.message << "\n";
        return false;
    }

    Program* progPtr = dynamic_cast<Program*>(programAst.get());
    if (progPtr) {
        progPtr->isREPL = isREPL;
        interpreter.interpret(*progPtr);
        const Value& result = interpreter.getLastValue();
        if (!result.isNone() && isREPL) {
            std::cout << result.toString() << "\n";
        }
    }
    return true;
}

// Generate output filename from input path
static std::string getOutputFilename(const std::string& inputPath) {
    size_t dotPos = inputPath.rfind('.');
    if (dotPos != std::string::npos) {
        return inputPath.substr(0, dotPos) + ".cnv";
    }
    return inputPath + ".cnv";
}

int main(int argc, char** argv) {
    // Check for -bytec flag
    if (argc >= 3 && std::string(argv[1]) == "-bytec") {
        std::string inputPath = argv[2];
        std::string src = slurpFile(inputPath);
        
        if (src.empty()) {
            std::cerr << "Failed to read file: " << inputPath << "\n";
            return 1;
        }

        BytecodeProgram program;
        if (!compileToBytecode(src, inputPath, program)) {
            return 1;
        }

        std::string outputPath = getOutputFilename(inputPath);
        std::string bytecodeStr = bytecodeToString(program);
        
        std::ofstream outFile(outputPath);
        if (!outFile) {
            std::cerr << "Failed to write output file: " << outputPath << "\n";
            return 1;
        }
        
        outFile << bytecodeStr;
        outFile.close();
        
        std::cout << "Bytecode written to: " << outputPath << "\n";
        return 0;
    }

    Interpreter interpreter;

    // File mode
    if (argc >= 2) {
        std::string src = slurpFile(argv[1]);
        if (src.empty()) {
            std::cerr << "Failed to read file\n";
            return 1;
        }

        if (!runSource(src, interpreter)) return 1;

        return 0;
    }

    // REPL mode
    std::cout << "NovaLang REPL (type 'exit' to quit, 'version' to show version, 'run <file>' to run a file)\n";
    std::string line;
    int lineNo = 1;

    while (true) {
        std::cout << ">>> ";
        if (!std::getline(std::cin, line)) break;

        if (line.empty()) continue;
        if (line == "exit" || line == "quit") break;
        if (line == "version" || line == "ver") {
            std::cout << "NovaLang version " << VERSION << "\n";
            continue;
        }
        if (line.rfind("run ", 0) == 0) {
            std::string filePath = line.substr(4);
            std::string src = slurpFile(filePath);
            if (src.empty()) {
                std::cerr << "Failed to read file: " << filePath << "\n";
            } else {
                runSource(src, interpreter, filePath, true, lineNo++);
            }
            continue;
        }

        runSource(line, interpreter, "", true, lineNo++);
    }
    
    return 0;
}
