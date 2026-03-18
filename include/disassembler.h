#pragma once
#include "vm.h"
#include <string>

namespace nova {

// Get opcode name as string
const char* getOpCodeName(OpCode op);

// Format a single instruction to string
std::string formatInstruction(const Instruction& instr, const BytecodeProgram& program);

// Convert bytecode program to string (line by line)
std::string bytecodeToString(const BytecodeProgram& program);

} // namespace nova
