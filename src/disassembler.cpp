#include "disassembler.h"
#include <sstream>
#include <iomanip>

namespace nova {

// Get opcode name as string
const char* getOpCodeName(OpCode op) {
    switch (op) {
        case OpCode::NOP: return "NOP";
        case OpCode::POP: return "POP";
        case OpCode::DUP: return "DUP";
        case OpCode::SWAP: return "SWAP";
        case OpCode::PUSH_NIL: return "PUSH_NIL";
        case OpCode::PUSH_TRUE: return "PUSH_TRUE";
        case OpCode::PUSH_FALSE: return "PUSH_FALSE";
        case OpCode::PUSH_INT: return "PUSH_INT";
        case OpCode::PUSH_FLOAT: return "PUSH_FLOAT";
        case OpCode::PUSH_STRING: return "PUSH_STRING";
        case OpCode::LOAD_LOCAL: return "LOAD_LOCAL";
        case OpCode::STORE_LOCAL: return "STORE_LOCAL";
        case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
        case OpCode::LOAD_UPVALUE: return "LOAD_UPVALUE";
        case OpCode::STORE_UPVALUE: return "STORE_UPVALUE";
        case OpCode::CLOSURE: return "CLOSURE";
        case OpCode::ADD: return "ADD";
        case OpCode::SUB: return "SUB";
        case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV";
        case OpCode::MOD: return "MOD";
        case OpCode::POW: return "POW";
        case OpCode::NEG: return "NEG";
        case OpCode::EQ: return "EQ";
        case OpCode::NEQ: return "NEQ";
        case OpCode::LT: return "LT";
        case OpCode::LTE: return "LTE";
        case OpCode::GT: return "GT";
        case OpCode::GTE: return "GTE";
        case OpCode::NOT: return "NOT";
        case OpCode::AND: return "AND";
        case OpCode::OR: return "OR";
        case OpCode::BIT_AND: return "BIT_AND";
        case OpCode::BIT_OR: return "BIT_OR";
        case OpCode::BIT_XOR: return "BIT_XOR";
        case OpCode::BIT_NOT: return "BIT_NOT";
        case OpCode::SHL: return "SHL";
        case OpCode::SHR: return "SHR";
        case OpCode::INC: return "INC";
        case OpCode::DEC: return "DEC";
        case OpCode::ADD_ASSIGN: return "ADD_ASSIGN";
        case OpCode::SUB_ASSIGN: return "SUB_ASSIGN";
        case OpCode::MUL_ASSIGN: return "MUL_ASSIGN";
        case OpCode::DIV_ASSIGN: return "DIV_ASSIGN";
        case OpCode::JUMP: return "JUMP";
        case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OpCode::JUMP_BACK: return "JUMP_BACK";
        case OpCode::CALL: return "CALL";
        case OpCode::RETURN: return "RETURN";
        case OpCode::RETURN_VALUE: return "RETURN_VALUE";
        case OpCode::ARG_COUNT: return "ARG_COUNT";
        case OpCode::BUILTIN: return "BUILTIN";
        case OpCode::LIST: return "LIST";
        case OpCode::INDEX_GET: return "INDEX_GET";
        case OpCode::INDEX_SET: return "INDEX_SET";
        case OpCode::STRUCT_NEW: return "STRUCT_NEW";
        case OpCode::FIELD_GET: return "FIELD_GET";
        case OpCode::FIELD_SET: return "FIELD_SET";
        case OpCode::ENTER_SCOPE: return "ENTER_SCOPE";
        case OpCode::EXIT_SCOPE: return "EXIT_SCOPE";
        case OpCode::BREAK: return "BREAK";
        case OpCode::CONTINUE: return "CONTINUE";
        case OpCode::TYPEOF: return "TYPEOF";
        case OpCode::HALT: return "HALT";
        default: return "UNKNOWN";
    }
}

// Format a single instruction to string
std::string formatInstruction(const Instruction& instr, const BytecodeProgram& program) {
    std::ostringstream ss;
    ss << std::setw(20) << std::left << getOpCodeName(instr.opcode);
    
    switch (instr.opcode) {
        case OpCode::PUSH_INT:
            ss << " " << instr.operand;
            break;
        case OpCode::PUSH_FLOAT:
            ss << " " << instr.operandFloat;
            break;
        case OpCode::PUSH_STRING:
            ss << " \"" << instr.operandStr << "\"";
            break;
        case OpCode::LOAD_LOCAL:
        case OpCode::STORE_LOCAL:
            ss << " " << instr.operand;
            break;
        case OpCode::LOAD_GLOBAL:
        case OpCode::STORE_GLOBAL:
            ss << " " << instr.operandStr;
            break;
        case OpCode::JUMP:
        case OpCode::JUMP_IF_FALSE:
        case OpCode::JUMP_IF_TRUE:
        case OpCode::JUMP_BACK:
            ss << " +" << instr.operand;
            break;
        case OpCode::CALL:
            ss << " " << instr.operand << " args";
            break;
        case OpCode::CLOSURE:
            ss << " fn:" << instr.operand;
            break;
        default:
            break;
    }
    
    return ss.str();
}

// Convert bytecode program to string (line by line)
std::string bytecodeToString(const BytecodeProgram& program) {
    std::ostringstream ss;
    
    ss << "# Bytecode Program\n";
    ss << "# Global variables: ";
    for (size_t i = 0; i < program.globalNames.size(); i++) {
        if (i > 0) ss << ", ";
        ss << program.globalNames[i];
    }
    ss << "\n";
    ss << "# String constants: ";
    for (size_t i = 0; i < program.stringConstants.size(); i++) {
        if (i > 0) ss << ", ";
        ss << "\"" << program.stringConstants[i] << "\"";
    }
    ss << "\n\n";
    
    for (size_t fnIdx = 0; fnIdx < program.functions.size(); fnIdx++) {
        const auto& fn = program.functions[fnIdx];
        ss << "Function: " << fn.name << " (" << fn.numParams << " params, " << fn.locals.size() << " locals)\n";
        
        for (size_t i = 0; i < fn.instructions.size(); i++) {
            const auto& instr = fn.instructions[i];
            ss << "  " << std::setw(4) << std::left << i << " " << formatInstruction(instr, program) << "\n";
        }
        ss << "\n";
    }
    
    return ss.str();
}

}
