#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <stack>
#include <optional>
#include <memory>
#include <functional>

namespace nova {

// Forward declarations
struct Value;
struct Program;
struct ASTNode;

// ============================================================================
// VM Opcodes
// ============================================================================

enum class OpCode : uint8_t {
    // Stack operations
    NOP,            // No operation
    POP,            // Pop top of stack
    DUP,            // Duplicate top of stack
    SWAP,           // Swap top two stack values
    
    // Constants
    PUSH_NIL,            // Push nil/null
    PUSH_TRUE,           // Push true
    PUSH_FALSE,          // Push false
    PUSH_INT,            // Push integer (operand: int64_t)
    PUSH_FLOAT,          // Push float (operand: double)
    PUSH_STRING,         // Push string (operand: string index)
    
    // Local variables
    LOAD_LOCAL,     // Load local variable (operand: index)
    STORE_LOCAL,    // Store local variable (operand: index)
    LOAD_GLOBAL,    // Load global variable (operand: string index)
    STORE_GLOBAL,   // Store global variable (operand: string index)
    
    // Upvalues (for closures)
    LOAD_UPVALUE,   // Load from upvalue
    STORE_UPVALUE,  // Store to upvalue
    CLOSURE,        // Create closure (operand: function index)
    
    // Arithmetic
    ADD,            // +
    SUB,            // -
    MUL,            // *
    DIV,            // /
    MOD,            // %
    POW,            // ^
    NEG,            // Unary negation
    
    // Comparison
    EQ,             // ==
    NEQ,            // !=
    LT,             // <
    LTE,            // <=
    GT,             // >
    GTE,            // >=
    
    // Logical
    NOT,            // !
    AND,            // &&
    OR,             // ||
    
    // Bitwise
    BIT_AND,        // &
    BIT_OR,         // |
    BIT_XOR,        // ^
    BIT_NOT,        // ~ (unary)
    SHL,            // <<
    SHR,            // >>
    
    // Increment/Decrement
    INC,            // ++ (post or pre)
    DEC,            // -- (post or pre)
    
    // Compound assignment (binary op then store)
    ADD_ASSIGN,     // +=
    SUB_ASSIGN,     // -=
    MUL_ASSIGN,     // *=
    DIV_ASSIGN,     // /=
    
    // Control flow
    JUMP,           // Unconditional jump (operand: offset)
    JUMP_IF_FALSE,  // Jump if false (operand: offset)
    JUMP_IF_TRUE,   // Jump if true (operand: offset)
    JUMP_BACK,      // Unconditional jump backwards (for loops)
    
    // Function calls
    CALL,           // Call function (operand: arg count)
    RETURN,         // Return from function
    RETURN_VALUE,   // Return with value
    
    // Built-in functions
    ARG_COUNT,        // Argument count for next builtin call
    BUILTIN,         // Call built-in (operand: builtin index)
    
    // List operations
    LIST,           // Create list (operand: element count)
    INDEX_GET,      // Get element at index
    INDEX_SET,      // Set element at index
    
    // Struct operations
    STRUCT_NEW,     // Create new struct (operand: struct name index)
    FIELD_GET,      // Get struct field (operand: field name index)
    FIELD_SET,      // Set struct field (operand: field name index)
    
    // Scope
    ENTER_SCOPE,    // Enter new scope
    EXIT_SCOPE,     // Exit current scope
    
    // Loop control
    BREAK,          // Break from loop
    CONTINUE,       // Continue to next iteration
    
    // Type checking
    TYPEOF,         // Get type of value
    
    // End of program
    HALT,           // Halt execution
};

// ============================================================================
// Bytecode Types
// ============================================================================

// A single instruction: opcode + optional operand
struct Instruction {
    OpCode opcode;
    int64_t operand = 0;     // For immediate values
    double operandFloat = 0; // For float operands
    std::string operandStr;  // For string operands
    int line = 0;            // Source line number for error reporting
    int column = 0;          // Source column number for error reporting
    
    Instruction(OpCode op) : opcode(op) {}
    Instruction(OpCode op, int64_t val) : opcode(op), operand(val) {}
    Instruction(OpCode op, double val) : opcode(op), operandFloat(val) {}
    Instruction(OpCode op, std::string val) : opcode(op), operandStr(std::move(val)) {}
};

// A function definition in bytecode
struct BytecodeFunction {
    std::string name;
    std::vector<Instruction> instructions;
    std::vector<std::string> locals;       // Local variable names
    std::vector<std::string> upvalues;    // Upvalue names
    std::optional<std::string> returnType; // Return type (none if void)
    int numParams = 0;                     // Number of parameters
    
    size_t arity() const { return locals.size(); }
};

// A complete bytecode program
struct BytecodeProgram {
    std::vector<BytecodeFunction> functions;
    std::vector<std::string> globalNames;    // Global variable names
    std::vector<std::string> stringConstants; // String pool
    std::vector<BytecodeFunction*> functionRefs; // Function references
    std::unordered_map<std::string, int> builtinIndex; // Built-in function indices
    
    // Add a string to the constant pool and return its index
    int addString(const std::string& s) {
        for (size_t i = 0; i < stringConstants.size(); i++) {
            if (stringConstants[i] == s) return static_cast<int>(i);
        }
        stringConstants.push_back(s);
        return static_cast<int>(stringConstants.size()) - 1;
    }
};

// ============================================================================
// Chunk for debugging/serialization
// ============================================================================

struct Chunk {
    std::vector<Instruction> instructions;
    std::vector<int> lineNumbers; // Source line for each instruction
    
    void write(Instruction instr, int line) {
        instructions.push_back(instr);
        lineNumbers.push_back(line);
    }
    
    size_t count() const { return instructions.size(); }
};

} // namespace nova

// ============================================================================
// VM Value - Runtime values used by the VM
// ============================================================================

namespace nova {

struct VMValue {
    using StructData = std::unordered_map<std::string, VMValue>;
    
    std::variant<
        int64_t,                      // Integer
        double,                       // Float
        std::string,                  // String
        bool,                         // Boolean
        std::vector<VMValue>,         // List
        std::shared_ptr<StructData>,  // Struct
        std::nullptr_t                // None/NULL
    > data;

    VMValue() : data(nullptr) {}
    
    explicit VMValue(int64_t i) : data(i) {}
    explicit VMValue(double f) : data(f) {}
    explicit VMValue(const std::string& s) : data(s) {}
    explicit VMValue(bool b) : data(b) {}
    explicit VMValue(std::nullptr_t) : data(nullptr) {}
    explicit VMValue(const std::vector<VMValue>& v) : data(v) {}
    explicit VMValue(std::vector<VMValue>&& v) : data(std::move(v)) {}
    explicit VMValue(std::shared_ptr<StructData> s) : data(std::move(s)) {}

    // Type checks
    bool isInteger() const { return std::holds_alternative<int64_t>(data); }
    bool isFloat() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isList() const { return std::holds_alternative<std::vector<VMValue>>(data); }
    bool isStruct() const { return std::holds_alternative<std::shared_ptr<StructData>>(data); }
    bool isNone() const { return std::holds_alternative<std::nullptr_t>(data); }

    enum class VMType {
        Int,
        Float,
        String,
        Bool,
        List,
        Struct,
        None
    };

    VMType type() const {
        if (isInteger()) return VMType::Int;
        if (isFloat()) return VMType::Float;
        if (isString()) return VMType::String;
        if (isBool()) return VMType::Bool;
        if (isList()) return VMType::List;
        if (isStruct()) return VMType::Struct;
        return VMType::None;
    }

    // Conversions
    int64_t asInteger() const { return std::get<int64_t>(data); }
    double asFloat() const { 
        if (isFloat()) return std::get<double>(data);
        if (isInteger()) return static_cast<double>(std::get<int64_t>(data));
        return 0.0;
    }
    const std::string& asString() const { return std::get<std::string>(data); }
    bool asBool() const { 
        if (isBool()) return std::get<bool>(data);
        if (isInteger()) return std::get<int64_t>(data) != 0;
        if (isFloat()) return std::get<double>(data) != 0.0;
        return false;
    }
    std::vector<VMValue>& asList() { return std::get<std::vector<VMValue>>(data); }
    const std::vector<VMValue>& asList() const { return std::get<std::vector<VMValue>>(data); }
    StructData& asStruct() { return *std::get<std::shared_ptr<StructData>>(data); }
    const StructData& asStruct() const { return *std::get<std::shared_ptr<StructData>>(data); }

    // Get field from struct
    VMValue getField(const std::string& field) const;
    void setField(const std::string& field, const VMValue& value);

    // String representation
    std::string toString() const;

    // Equality comparison
    bool equals(const VMValue& other) const;
};

// Forward declaration
struct BytecodeFunction;

// Call Frame - Stack frame for function calls
struct CallFrame {
    const BytecodeFunction* function;
    size_t ip;                      // Instruction pointer
    std::vector<VMValue> locals;    // Local variables
    std::vector<VMValue> stack;     // Stack base for this frame

    CallFrame(const BytecodeFunction* fn, size_t stackBase);
};

// Virtual Machine
class VirtualMachine {
public:
    VirtualMachine();

    // Execute a bytecode program
    VMValue execute(BytecodeProgram& program);

private:
    BytecodeProgram* program = nullptr;
    std::vector<VMValue> stack;           // Value stack
    std::vector<CallFrame> frames;        // Call frames
    std::unordered_map<std::string, VMValue> globals;
    std::vector<VMValue> constants;       // Constant pool values
    bool halted = false;
    VMValue lastValue;
    int currentLine = 0;  // Current line being executed (for error reporting)
    int pendingArgCount = 0;  // Argument count for next builtin call

    // Built-in functions
    std::unordered_map<std::string, std::function<VMValue(std::vector<VMValue>)>> builtins;

    void setupBuiltins();
    VMValue run();
    void executeInstruction(CallFrame& frame, const Instruction& instr);
    VMValue binaryOp(const VMValue& a, const VMValue& b, const std::string& op);
    bool compareOp(const VMValue& a, const VMValue& b, const std::string& op);
    const BytecodeFunction* findFunction(const VMValue& funcVal);
    void callFunction(const BytecodeFunction& fn, const std::vector<VMValue>& args);
};

} // namespace nova

// Forward declaration of VirtualMachine
namespace nova {
class VirtualMachine;
}
