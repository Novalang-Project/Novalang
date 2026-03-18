#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include <cmath>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace nova {

// ============================================================================
// Interpreter Implementation using Bytecode VM
// ============================================================================

Interpreter::Interpreter() : compiler(std::make_unique<Compiler>()), vm(std::make_unique<VirtualMachine>()) {

}

Value Interpreter::interpret(Program& program) {
    // Use bytecode-based interpretation
    try {
        BytecodeProgram bytecode = compiler->compile(program);
        VMValue result = vm->execute(bytecode);
        
        // Convert VMValue to Value
        lastValue = convertVMValueToValue(result);
        return lastValue;
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
        return Value(nullptr);
    }
}

Value Interpreter::interpretExpression(ExprPtr& expr) {
    return Value(nullptr);
}

// Helper to convert VMValue to Value
Value Interpreter::convertVMValueToValue(const VMValue& vmVal) {
    if (vmVal.isInteger()) return Value(vmVal.asInteger());
    if (vmVal.isFloat()) return Value(vmVal.asFloat());
    if (vmVal.isString()) return Value(vmVal.asString());
    if (vmVal.isBool()) return Value(vmVal.asBool());
    if (vmVal.isList()) {
        std::vector<Value> list;
        for (const auto& elem : vmVal.asList()) {
            list.push_back(convertVMValueToValue(elem));
        }
        return Value(std::move(list));
    }
    if (vmVal.isStruct()) {
        auto structData = std::make_shared<Value::StructData>();
        for (const auto& [key, val] : vmVal.asStruct()) {
            (*structData)[key] = convertVMValueToValue(val);
        }
        return Value(structData);
    }
    return Value(nullptr);
}

// Stub implementations for backward compatibility
Environment* Interpreter::currentEnv() { return nullptr; }
void Interpreter::pushScope() {}
void Interpreter::pushScope(Environment* parent) { (void)parent; }
void Interpreter::popScope() {}

} // namespace nova
