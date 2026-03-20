#include "vm.h"
#include "errors/runtime.h"
#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace nova {

// ============================================================================
// VMValue Implementation
// ============================================================================

// Helper function to format float with up to 16 decimals, removing trailing zeros
static std::string formatFloat(double value) {
    // Handle exact integers (like 1.0, 2.0) - show as integer
    if (value == std::floor(value) && value == std::ceil(value)) {
        return std::to_string(static_cast<long long>(value));
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(16) << value;
    std::string result = oss.str();
    
    if (result.find('.') != std::string::npos) {
        while (!result.empty() && result.back() == '0') {
            result.pop_back();
        }
        if (!result.empty() && result.back() == '.') {
            result.pop_back();
        }
    }
    
    return result;
}

std::string VMValue::toString() const {
    if (isInteger()) return std::to_string(asInteger());
    if (isFloat()) return formatFloat(asFloat());
    if (isString()) return asString();
    if (isBool()) return asBool() ? "true" : "false";
    if (isList()) {
        const auto& list = asList();
        std::string result = "[";
        for (size_t i = 0; i < list.size(); i++) {
            result += list[i].toString();
            if (i < list.size() - 1) result += ", ";
        }
        result += "]";
        return result;
    }
    if (isStruct()) {
        const auto& s = asStruct();
        std::string result = "{";
        bool first = true;
        for (const auto& [key, val] : s) {
            if (!first) result += ", ";
            result += key + ": " + val.toString();
            first = false;
        }
        result += "}";
        return result;
    }
    return "none";
}

VMValue VMValue::getField(const std::string& field) const {
    if (isStruct()) {
        const auto& s = asStruct();
        auto it = s.find(field);
        if (it != s.end()) {
            return it->second;
        }
    }
    throw RuntimeError("Value is not a struct or field does not exist: " + field, 0, 0);
}

void VMValue::setField(const std::string& field, const VMValue& value) {
    if (isStruct()) {
        asStruct()[field] = value;
    } else {
        throw RuntimeError("Value is not a struct", 0, 0);
    }
}

bool VMValue::equals(const VMValue& other) const {
    if (isInteger() && other.isInteger()) {
        return asInteger() == other.asInteger();
    }
    if (isFloat() && other.isFloat()) {
        return asFloat() == other.asFloat();
    }
    if (isString() && other.isString()) {
        return asString() == other.asString();
    }
    if (isBool() && other.isBool()) {
        return asBool() == other.asBool();
    }
    if (isNone() && other.isNone()) {
        return true;
    }
    return false;
}

// ============================================================================
// CallFrame Implementation
// ============================================================================

CallFrame::CallFrame(const BytecodeFunction* fn, size_t stackBase)
    : function(fn), ip(0), stack(stackBase) {
    // Initialize locals - resize to match function's local count
    // This is done dynamically in case additional locals were added during compilation
    size_t localsSize = fn->locals.size();
    // Make sure we have at least some space for loop variables etc.
    // If the function has fewer locals than expected, use a reasonable default
    if (localsSize < 10) {
        locals.resize(10);
    } else {
        locals.resize(localsSize);
    }
}

// ============================================================================
// Virtual Machine Implementation
// ============================================================================

VirtualMachine::VirtualMachine() {
    setupBuiltins();
}

void VirtualMachine::setupBuiltins() {
    // len - get length of list or string
    builtins["len"] = [this](const std::vector<VMValue>& args) -> VMValue {
        if (args.empty()) return VMValue(nullptr);
        const VMValue& arg = args[0];
        if (arg.isList()) {
            return VMValue(static_cast<int64_t>(arg.asList().size()));
        }
        if (arg.isString()) {
            return VMValue(static_cast<int64_t>(arg.asString().size()));
        }
        throw RuntimeError("len() argument must be a list or string", currentLine, 0);
    };

    // push - add element to list
    builtins["push"] = [this](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 2) {
            throw RuntimeError("push() expects 2 arguments", currentLine, 0);
        }
        if (!args[0].isList()) {
            throw RuntimeError("push() first argument must be a list", currentLine, 0);
        }
        std::string globalName;
        for (const auto& pair : globals) {
            if (pair.second.isList() && pair.second.toString() == args[0].toString()) {
                globalName = pair.first;
                break;
            }
        }
        if (!globalName.empty()) {

            globals[globalName].asList().push_back(args[1]);
            return globals[globalName]; 
        }
        VMValue listVal = args[0];
        listVal.asList().push_back(args[1]);
        return listVal;
    };

    // pop - remove and return last element
    builtins["pop"] = [this](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 1) {
            throw RuntimeError("pop() expects 1 argument", currentLine, 0);
        }
        if (!args[0].isList()) {
            throw RuntimeError("pop() argument must be a list", currentLine, 0);
        }
        // Check if this list is in globals - if so, modify it in place
        std::string globalName;
        for (const auto& pair : globals) {
            if (pair.second.isList() && pair.second.toString() == args[0].toString()) {
                globalName = pair.first;
                break;
            }
        }
        if (!globalName.empty()) {
            // Modify the global directly
            auto& list = globals[globalName].asList();
            if (list.empty()) {
                throw RuntimeError("pop() from empty list", currentLine, 0);
            }
            VMValue result = list.back();
            list.pop_back();
            return result;
        }
        // Not a global - work with copy
        VMValue listVal = args[0];
        auto& list = listVal.asList();
        if (list.empty()) {
            throw RuntimeError("pop() from empty list", currentLine, 0);
        }
        VMValue result = list.back();
        list.pop_back();
        return result;
    };

    // println - print to console
    builtins["println"] = [](const std::vector<VMValue>& args) -> VMValue {
        for (size_t i = 0; i < args.size(); i++) {
            std::cout << args[i].toString();
            if (i < args.size() - 1) std::cout << " ";
        }
        std::cout << "\n";
        return VMValue(nullptr);
    };

    // removeAt - remove element at index
    builtins["removeAt"] = [this](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 2) {
            throw RuntimeError("removeAt() expects 2 arguments", currentLine, 0);
        }
        if (!args[0].isList()) {
            throw RuntimeError("removeAt() first argument must be a list", currentLine, 0);
        }
        if (!args[1].isInteger()) {
            throw RuntimeError("removeAt() second argument must be an integer", currentLine, 0);
        }
        // Check if this list is in globals - if so, modify it in place
        std::string globalName;
        for (const auto& pair : globals) {
            if (pair.second.isList() && pair.second.toString() == args[0].toString()) {
                globalName = pair.first;
                break;
            }
        }
        
        int64_t idx = args[1].asInteger();
        
        if (!globalName.empty()) {
            // Modify global in place
            auto& list = globals[globalName].asList();
            if (idx < 0) {
                idx = static_cast<int64_t>(list.size()) + idx;
            }
            if (idx < 0 || idx >= static_cast<int64_t>(list.size())) {
                throw RuntimeError("index out of bounds", currentLine, 0);
            }
            VMValue result = list[idx];
            list.erase(list.begin() + idx);
            return globals[globalName];  // Return modified list for chaining
        }
        
        // Not a global
        VMValue listVal = args[0];
        auto& list = listVal.asList();
        if (idx < 0) {
            idx = static_cast<int64_t>(list.size()) + idx;
        }
        if (idx < 0 || idx >= static_cast<int64_t>(list.size())) {
            throw RuntimeError("index out of bounds", currentLine, 0);
        }
        VMValue result = list[idx];
        list.erase(list.begin() + idx);
        return listVal;  // Return modified list for chaining
    };

    // input - read user input (optional prompt string)
    builtins["input"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() > 1) {
            throw RuntimeError("input() expects 0 or 1 arguments", 0, 0);
        }
        // Print prompt if provided
        if (!args.empty() && args[0].isString()) {
            std::cout << args[0].asString();
            std::cout.flush();
        }
        std::string input;
        std::getline(std::cin, input);
        return VMValue(input);
    };

    // toInt - convert value to integer
    builtins["toInt"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 1) {
            throw RuntimeError("toInt() expects 1 argument", 0, 0);
        }
        const VMValue& arg = args[0];
        
        if (arg.isInteger()) {
            return arg;
        }
        if (arg.isFloat()) {
            return VMValue(static_cast<int64_t>(arg.asFloat()));
        }
        if (arg.isString()) {
            const std::string& str = arg.asString();
            if (str.empty()) {
                throw RuntimeError("toInt() cannot convert empty string", 0, 0);
            }
            try {
                size_t pos;
                int64_t result = std::stoll(str, &pos);
                if (pos != str.size()) {
                    throw RuntimeError("toInt() invalid integer string: " + str, 0, 0);
                }
                return VMValue(result);
            } catch (const std::exception&) {
                throw RuntimeError("toInt() cannot convert string to integer: " + str, 0, 0);
            }
        }
        if (arg.isBool()) {
            return VMValue(arg.asBool() ? int64_t(1) : int64_t(0));
        }
        throw RuntimeError("toInt() cannot convert this type to integer", 0, 0);
    };

    // toFloat - convert value to float
    builtins["toFloat"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 1) {
            throw RuntimeError("toFloat() expects 1 argument", 0, 0);
        }
        const VMValue& arg = args[0];
        
        if (arg.isFloat()) {
            return arg;
        }
        if (arg.isInteger()) {
            return VMValue(static_cast<double>(arg.asInteger()));
        }
        if (arg.isString()) {
            const std::string& str = arg.asString();
            if (str.empty()) {
                throw RuntimeError("toFloat() cannot convert empty string", 0, 0);
            }
            try {
                size_t pos;
                double result = std::stod(str, &pos);
                if (pos != str.size()) {
                    throw RuntimeError("toFloat() invalid float string: " + str, 0, 0);
                }
                return VMValue(result);
            } catch (const std::exception&) {
                throw RuntimeError("toFloat() cannot convert string to float: " + str, 0, 0);
            }
        }
        if (arg.isBool()) {
            return VMValue(arg.asBool() ? 1.0 : 0.0);
        }
        throw RuntimeError("toFloat() cannot convert this type to float", 0, 0);
    };

    // toString - convert value to string
    builtins["toString"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 1) {
            throw RuntimeError("toString() expects 1 argument", 0, 0);
        }
        return VMValue(args[0].toString());
    };

    // toBool - convert value to boolean
    builtins["toBool"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 1) {
            throw RuntimeError("toBool() expects 1 argument", 0, 0);
        }
        const VMValue& arg = args[0];
        
        if (arg.isBool()) {
            return arg;
        }
        if (arg.isInteger()) {
            return VMValue(arg.asInteger() != 0);
        }
        if (arg.isFloat()) {
            return VMValue(arg.asFloat() != 0.0);
        }
        if (arg.isString()) {
            return VMValue(!arg.asString().empty());
        }
        if (arg.isList()) {
            return VMValue(!arg.asList().empty());
        }
        if (arg.isNone()) {
            return VMValue(false);
        }
        return VMValue(true);
    };

    // format - format float to specified decimal places
    builtins["format"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 2) {
            throw RuntimeError("format() expects 2 arguments", 0, 0);
        }
        const VMValue& value = args[0];
        const VMValue& decimals = args[1];
        
        if (!value.isFloat()) {
            throw RuntimeError("format() first argument must be a float", 0, 0);
        }
        if (!decimals.isInteger()) {
            throw RuntimeError("format() second argument must be an integer", 0, 0);
        }
        
        double val = value.asFloat();
        int dec = static_cast<int>(decimals.asInteger());
        
        if (dec < 0) dec = 0;
        if (dec > 16) dec = 16;
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(dec) << val;
        std::string result = oss.str();
        
        // Remove trailing zeros after decimal point if not using fixed precision display
        if (dec > 0) {
            while (!result.empty() && result.back() == '0') {
                result.pop_back();
            }
            if (!result.empty() && result.back() == '.') {
                result.pop_back();
            }
        }
        
        return VMValue(result);
    };

    // round - round float to specified decimal places (returns string)
    builtins["round"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 2) {
            throw RuntimeError("round() expects 2 arguments", 0, 0);
        }
        const VMValue& value = args[0];
        const VMValue& decimals = args[1];
        
        if (!value.isFloat()) {
            throw RuntimeError("round() first argument must be a float", 0, 0);
        }
        if (!decimals.isInteger()) {
            throw RuntimeError("round() second argument must be an integer", 0, 0);
        }
        
        double val = value.asFloat();
        int dec = static_cast<int>(decimals.asInteger());
        
        if (dec < 0) dec = 0;
        if (dec > 16) dec = 16;
        
        // Use round-half-up rounding
        double multiplier = std::pow(10.0, dec);
        double rounded = std::round(val * multiplier) / multiplier;
        
        // Format the rounded value as string
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(dec) << rounded;
        std::string result = oss.str();
        
        // Remove trailing zeros after decimal point
        if (dec > 0) {
            while (!result.empty() && result.back() == '0') {
                result.pop_back();
            }
            if (!result.empty() && result.back() == '.') {
                result.pop_back();
            }
        }
        
        return VMValue(result);
    };

    // typeof - get type name of value
    builtins["typeof"] = [](const std::vector<VMValue>& args) -> VMValue {
        if (args.size() != 1) {
            throw RuntimeError("typeof() expects 1 argument", 0, 0);
        }
        const VMValue& arg = args[0];
        
        if (arg.isNone()) return VMValue("none");
        if (arg.isBool()) return VMValue("bool");
        if (arg.isInteger()) return VMValue("int");
        if (arg.isFloat()) return VMValue("float");
        if (arg.isString()) return VMValue("string");
        if (arg.isList()) return VMValue("list");
        
        return VMValue("unknown");
    };
}

// Execute a bytecode program.
// Initializes VM state, resolves the entry function (main, _start, or fallback),
// sets up the initial call frame, and starts execution.
// Returns the final value produced by the program.
VMValue VirtualMachine::execute(BytecodeProgram& prog) {
    program = &prog;
    stack.clear();
    frames.clear();
    globals.clear();
    constants.clear();
    halted = false;

    // Find the _start function (entry point)
    if (prog.functions.empty()) {
        return VMValue(nullptr);
    }

    const BytecodeFunction* mainFn = nullptr;
    for (const auto& fn : prog.functions) {
        if (fn.name == "_start") {
            mainFn = &fn;
            break;
        }
    }
    
    // If no _start function, fallback to first function
    if (!mainFn) {
        mainFn = &prog.functions[0];
    }

    callFunction(*mainFn, {});
    
    // Run the VM
    return run();
}

// Run the virtual machine.
// Executes instructions frame-by-frame until halted or no frames remain.
// Handles implicit returns and propagates return values on the stack.
// Returns the last computed value.
VMValue VirtualMachine::run() {
    while (!halted && !frames.empty()) {
        CallFrame& frame = frames.back();
        
        if (frame.ip >= frame.function->instructions.size()) {
            frames.pop_back();
            if (!frames.empty()) {
                frames.back().ip++;
                stack.push_back(VMValue(nullptr));
            }
            continue;
        }
        
        const Instruction& instr = frame.function->instructions[frame.ip];
        executeInstruction(frame, instr);
    }
    
    return lastValue;
}

void VirtualMachine::executeInstruction(CallFrame& frame, const Instruction& instr) {
    currentLine = instr.line;
    
    switch (instr.opcode) {
        // Stack operations
        case OpCode::NOP:
            frame.ip++;
            break;
        case OpCode::ARG_COUNT:
            pendingArgCount = static_cast<int>(instr.operand);
            frame.ip++;
            break;
        case OpCode::POP:
            if (!stack.empty()) {
                stack.pop_back();
            }
            frame.ip++;
            break;
        case OpCode::DUP:
            if (!stack.empty()) {
                stack.push_back(stack.back());
            }
            frame.ip++;
            break;
        case OpCode::SWAP: {
            if (stack.size() >= 2) {
                VMValue a = stack.back();
                stack.pop_back();
                VMValue b = stack.back();
                stack.back() = a;
                stack.push_back(b);
            }
            frame.ip++;
            break;
        }
        
        // Constants
        case OpCode::PUSH_NIL:
            stack.push_back(VMValue(nullptr));
            frame.ip++;
            break;
        case OpCode::PUSH_TRUE:
            stack.push_back(VMValue(true));
            frame.ip++;
            break;
        case OpCode::PUSH_FALSE:
            stack.push_back(VMValue(false));
            frame.ip++;
            break;
        case OpCode::PUSH_INT:
            stack.push_back(VMValue(instr.operand));
            frame.ip++;
            break;
        case OpCode::PUSH_FLOAT:
            stack.push_back(VMValue(instr.operandFloat));
            frame.ip++;
            break;
        case OpCode::PUSH_STRING: {
            int idx = static_cast<int>(instr.operand);
            if (idx > 0 && idx < static_cast<int>(program->stringConstants.size())) {
                stack.push_back(VMValue(program->stringConstants[idx]));
            } else {
                stack.push_back(VMValue(instr.operandStr));
            }
            frame.ip++;
            break;
        }
        
        // Local variables
        case OpCode::LOAD_LOCAL: {
            int idx = static_cast<int>(instr.operand);
            if (idx >= 0 && idx < static_cast<int>(frame.locals.size())) {
                stack.push_back(frame.locals[idx]);
            } else {
                stack.push_back(VMValue(nullptr));
            }
            frame.ip++;
            break;
        }
        case OpCode::STORE_LOCAL: {
            int idx = static_cast<int>(instr.operand);
            if (idx >= 0 && idx < static_cast<int>(frame.locals.size())) {
                if (!stack.empty()) {
                    // Get new value first
                    VMValue newValue = std::move(stack.back());
                    stack.pop_back();
                    
                    // Explicitly free old value's resources BEFORE assigning new value
                    VMValue& oldRef = frame.locals[idx];
                    if (oldRef.isString()) {
                        // Access the string directly from the variant and clear it
                        std::get<std::string>(oldRef.data).clear();
                        std::get<std::string>(oldRef.data).shrink_to_fit();
                    }

                    // Now assign the new value
                    frame.locals[idx] = std::move(newValue);
                }
            }
            frame.ip++;
            break;
        }
        
        // Global variables
        case OpCode::LOAD_GLOBAL: {
            std::string name = instr.operandStr;
            if (name.empty() && instr.operand >= 0 && instr.operand < static_cast<int>(program->stringConstants.size())) {
                name = program->stringConstants[instr.operand];
            }
            auto it = globals.find(name);
            if (it != globals.end()) {
                stack.push_back(it->second);
            } else {
                stack.push_back(VMValue(nullptr));
            }
            frame.ip++;
            break;
        }
        case OpCode::STORE_GLOBAL: {
            std::string name = instr.operandStr;
            if (name.empty() && instr.operand >= 0 && instr.operand < static_cast<int>(program->stringConstants.size())) {
                name = program->stringConstants[instr.operand];
            }
            if (!stack.empty()) {
                VMValue newValue = std::move(stack.back());
                stack.pop_back();

                auto it = globals.find(name);
                if (it != globals.end()) {

                    if (it->second.isString()) {
                        std::get<std::string>(it->second.data).clear();
                        std::get<std::string>(it->second.data).shrink_to_fit();
                    }
                }
                
                globals[name] = std::move(newValue);
            }
            frame.ip++;
            break;
        }
        
        // Arithmetic
        case OpCode::ADD: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(binaryOp(a, b, "+"));
            }
            frame.ip++;
            break;
        }
        case OpCode::SUB: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(binaryOp(a, b, "-"));
            }
            frame.ip++;
            break;
        }
        case OpCode::MUL: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(binaryOp(a, b, "*"));
            }
            frame.ip++;
            break;
        }
        case OpCode::DIV: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                if (b.asFloat() == 0.0) {
                    throw RuntimeError("division by zero", 0, 0);
                }
                stack.push_back(binaryOp(a, b, "/"));
            }
            frame.ip++;
            break;
        }
        case OpCode::MOD: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(binaryOp(a, b, "%"));
            }
            frame.ip++;
            break;
        }
        case OpCode::POW: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(binaryOp(a, b, "^"));
            }
            frame.ip++;
            break;
        }
        case OpCode::NEG: {
            if (!stack.empty()) {
                VMValue a = stack.back(); stack.pop_back();
                if (a.isInteger()) {
                    stack.push_back(VMValue(-a.asInteger()));
                } else {
                    stack.push_back(VMValue(-a.asFloat()));
                }
            }
            frame.ip++;
            break;
        }
        
        // Comparison
        case OpCode::EQ: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(compareOp(a, b, "==")));
            }
            frame.ip++;
            break;
        }
        case OpCode::NEQ: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(!compareOp(a, b, "==")));
            }
            frame.ip++;
            break;
        }
        case OpCode::LT: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(compareOp(a, b, "<")));
            }
            frame.ip++;
            break;
        }
        case OpCode::LTE: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(compareOp(a, b, "<=")));
            }
            frame.ip++;
            break;
        }
        case OpCode::GT: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(compareOp(a, b, ">")));
            }
            frame.ip++;
            break;
        }
        case OpCode::GTE: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(compareOp(a, b, ">=")));
            }
            frame.ip++;
            break;
        }
        
        // Logical
        case OpCode::NOT: {
            if (!stack.empty()) {
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(!a.asBool()));
            }
            frame.ip++;
            break;
        }
        case OpCode::AND: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(a.asBool() && b.asBool()));
            }
            frame.ip++;
            break;
        }
        case OpCode::OR: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(a.asBool() || b.asBool()));
            }
            frame.ip++;
            break;
        }
        
        // Bitwise
        case OpCode::BIT_AND: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(a.asInteger() & b.asInteger()));
            }
            frame.ip++;
            break;
        }
        case OpCode::BIT_OR: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(a.asInteger() | b.asInteger()));
            }
            frame.ip++;
            break;
        }
        case OpCode::BIT_NOT: {
            if (!stack.empty()) {
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(~a.asInteger()));
            }
            frame.ip++;
            break;
        }
        case OpCode::SHL: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(a.asInteger() << b.asInteger()));
            }
            frame.ip++;
            break;
        }
        case OpCode::SHR: {
            if (stack.size() >= 2) {
                VMValue b = stack.back(); stack.pop_back();
                VMValue a = stack.back(); stack.pop_back();
                stack.push_back(VMValue(a.asInteger() >> b.asInteger()));
            }
            frame.ip++;
            break;
        }
        
        // Increment/Decrement
        case OpCode::INC: {
            if (!stack.empty()) {
                VMValue a = stack.back();
                if (a.isInteger()) {
                    stack.back() = VMValue(a.asInteger() + 1);
                } else {
                    stack.back() = VMValue(a.asFloat() + 1.0);
                }
            }
            frame.ip++;
            break;
        }
        case OpCode::DEC: {
            if (!stack.empty()) {
                VMValue a = stack.back();
                if (a.isInteger()) {
                    stack.back() = VMValue(a.asInteger() - 1);
                } else {
                    stack.back() = VMValue(a.asFloat() - 1.0);
                }
            }
            frame.ip++;
            break;
        }
        
        // Control flow
        case OpCode::JUMP: {
            int64_t offset = instr.operand;
            frame.ip += offset;
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            int64_t offset = instr.operand;
            if (!stack.empty()) {
                VMValue cond = stack.back();
                stack.pop_back();
                if (!cond.asBool()) {
                    frame.ip += offset;
                    break;
                }
            }
            frame.ip++;
            break;
        }
        case OpCode::JUMP_IF_TRUE: {
            int64_t offset = instr.operand;
            if (!stack.empty()) {
                VMValue cond = stack.back();
                stack.pop_back();
                if (cond.asBool()) {
                    frame.ip += offset;
                    break;
                }
            }
            frame.ip++;
            break;
        }
        case OpCode::JUMP_BACK: {
            int64_t offset = instr.operand;
            frame.ip += offset;
            break;
        }
        
        // Function calls
        case OpCode::CALL: {
            int64_t argCount = instr.operand;
            
            if (stack.size() >= static_cast<size_t>(argCount + 1)) {
                VMValue funcVal = stack[stack.size() - 1 - argCount];
                
                if (funcVal.isString()) {
                    std::string funcName = funcVal.asString();
                    
                    // Handle namespace function calls: "namespace.function" -> "function"
                    size_t dotPos = funcName.rfind('.');
                    std::string lookupName = funcName;
                    if (dotPos != std::string::npos) {
                        lookupName = funcName.substr(dotPos + 1);
                    }
                    
                    std::vector<VMValue> args;
                    for (int i = 0; i < argCount; i++) {
                        args.insert(args.begin(), stack.back());
                        stack.pop_back();
                    }
                    stack.pop_back();

                    auto it = builtins.find(lookupName);
                    if (it != builtins.end()) {
                        VMValue result = it->second(args);
                        stack.push_back(result);
                    } else {
                        bool found = false;
                        for (size_t i = 0; i < program->functions.size(); i++) {
                            if (program->functions[i].name == lookupName) {
                                callFunction(program->functions[i], args);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            // If main function doesn't exist, just skip the call (like Python)
                            // Arguments were already popped above, just push nil as return value
                            stack.push_back(VMValue(nullptr));
                        }
                    }
                    if (frames.size() >= 2) {
                        frames[frames.size() - 2].ip++;
                    }
                    // Note: we don't increment the new frame's ip here, it starts at 0
                } else {
                    for (int i = 0; i < argCount; i++) {
                        stack.pop_back();
                    }
                    stack.pop_back(); 
                    stack.push_back(VMValue(nullptr));
                    frame.ip++;
                }
            } else {
                frame.ip++;
            }
            break;
        }
        case OpCode::RETURN:
            frames.pop_back();
            break;
        case OpCode::RETURN_VALUE:
            if (!stack.empty()) {
                lastValue = stack.back();
                stack.pop_back();
            }
            frames.pop_back();
            if (frames.empty()) {
                halted = true;
            } else {
                stack.push_back(lastValue);
            }
            break;
        
        // Built-in functions
        case OpCode::BUILTIN: {
            int builtinIdx = static_cast<int>(instr.operand);
            
            int expectedArgs = 0;
            if (builtinIdx == 0) expectedArgs = 1;       // len
            else if (builtinIdx == 1) expectedArgs = 2;  // push
            else if (builtinIdx == 2) expectedArgs = 1;  // pop
            else if (builtinIdx == 3) expectedArgs = pendingArgCount; // println (use arg count)
            else if (builtinIdx == 4) expectedArgs = 2;  // removeAt
            else if (builtinIdx == 5) expectedArgs = pendingArgCount; // input (use arg count)
            else if (builtinIdx == 6) expectedArgs = 2;  // format
            else if (builtinIdx == 7) expectedArgs = 2;  // round
            else if (builtinIdx == 8) expectedArgs = 1;  // toInt
            else if (builtinIdx == 9) expectedArgs = 1;  // toFloat
            else if (builtinIdx == 10) expectedArgs = 1; // toString
            else if (builtinIdx == 11) expectedArgs = 1; // toBool
            else if (builtinIdx == 12) expectedArgs = 1; // typeof
            
            // Collect arguments from the stack
            std::vector<VMValue> args;
            
            // Use pendingArgCount for variadic functions (println, input)
            if (builtinIdx == 3 || builtinIdx == 5) {
                // Collect exactly pendingArgCount arguments
                for (int i = 0; i < pendingArgCount && !stack.empty(); i++) {
                    args.insert(args.begin(), stack.back());
                    stack.pop_back();
                }
                pendingArgCount = 0;  
            } else if (expectedArgs == -2) {
                while (!stack.empty()) {
                    args.insert(args.begin(), stack.back());
                    stack.pop_back();
                }
            } else {
                for (int i = 0; i < expectedArgs && !stack.empty(); i++) {
                    args.insert(args.begin(), stack.back());
                    stack.pop_back();
                }
            }
            
            // Find and call the built-in
            std::string funcName;
            if (builtinIdx == 0) funcName = "len";
            else if (builtinIdx == 1) funcName = "push";
            else if (builtinIdx == 2) funcName = "pop";
            else if (builtinIdx == 3) funcName = "println";
            else if (builtinIdx == 4) funcName = "removeAt";
            else if (builtinIdx == 5) funcName = "input";
            else if (builtinIdx == 6) funcName = "format";
            else if (builtinIdx == 7) funcName = "round";
            else if (builtinIdx == 8) funcName = "toInt";
            else if (builtinIdx == 9) funcName = "toFloat";
            else if (builtinIdx == 10) funcName = "toString";
            else if (builtinIdx == 11) funcName = "toBool";
            else if (builtinIdx == 12) funcName = "typeof";
            
            auto it = builtins.find(funcName);
            if (it != builtins.end()) {
                VMValue result = it->second(args);
                if (!result.isNone()) {
                    stack.push_back(result);
                }
            }
            frame.ip++;
            break;
        }
        
        // List operations
        case OpCode::LIST: {
            int64_t count = instr.operand;
            std::vector<VMValue> list;
            for (int64_t i = 0; i < count; i++) {
                if (!stack.empty()) {
                    list.insert(list.begin(), stack.back());
                    stack.pop_back();
                }
            }
            stack.push_back(VMValue(std::move(list)));
            frame.ip++;
            break;
        }
        case OpCode::INDEX_GET: {
            if (stack.size() >= 2) {
                VMValue index = stack.back(); stack.pop_back();
                VMValue collection = stack.back(); stack.pop_back();
                
                if (collection.isList()) {
                    if (!index.isInteger()) {
                        throw RuntimeError("list index must be an integer", 0, 0);
                    }
                    int64_t idx = index.asInteger();
                    auto& list = collection.asList();
                    if (idx < 0) {
                        idx = static_cast<int64_t>(list.size()) + idx;
                    }
                    if (idx < 0 || idx >= static_cast<int64_t>(list.size())) {
                        throw RuntimeError("index out of bounds", currentLine, 0);
                    }
                    stack.push_back(list[idx]);
                } else if (collection.isString()) {
                    if (!index.isInteger()) {
                        throw RuntimeError("string index must be an integer", currentLine, 0);
                    }
                    int64_t idx = index.asInteger();
                    const std::string& str = collection.asString();
                    if (idx < 0) {
                        idx = static_cast<int64_t>(str.size()) + idx;
                    }
                    if (idx < 0 || idx >= static_cast<int64_t>(str.size())) {
                        throw RuntimeError("index out of bounds", currentLine, 0);
                    }
                    std::string charStr(1, str[idx]);
                    stack.push_back(VMValue(charStr));
                } else {
                    throw RuntimeError("can only index into lists and strings", currentLine, 0);
                }
            }
            frame.ip++;
            break;
        }
        case OpCode::INDEX_SET: {
            if (stack.size() >= 3) {
                VMValue value = stack.back(); stack.pop_back();
                VMValue index = stack.back(); stack.pop_back();
                VMValue collection = stack.back(); stack.pop_back();
                
                if (collection.isList()) {
                    if (!index.isInteger()) {
                        throw RuntimeError("list index must be an integer", currentLine, 0);
                    }
                    int64_t idx = index.asInteger();
                    auto& list = collection.asList();
                    if (idx < 0) {
                        idx = static_cast<int64_t>(list.size()) + idx;
                    }
                    if (idx < 0 || idx >= static_cast<int64_t>(list.size())) {
                        throw RuntimeError("index out of bounds", currentLine, 0);
                    }
                    list[idx] = value;
                    // Push the modified collection back onto the stack
                    stack.push_back(collection);
                } else {
                    throw RuntimeError("can only assign to list elements", currentLine, 0);
                }
            }
            frame.ip++;
            break;
        }
        
        // Struct operations
        case OpCode::STRUCT_NEW: {
            std::string typeName = instr.operandStr;
            if (instr.operand > 0 && instr.operand < static_cast<int>(program->stringConstants.size())) {
                typeName = program->stringConstants[instr.operand];
            }
            auto structData = std::make_shared<VMValue::StructData>();
            stack.push_back(VMValue(structData));
            frame.ip++;
            break;
        }
        case OpCode::FIELD_GET: {
            if (!stack.empty()) {
                VMValue object = stack.back();
                std::string field = instr.operandStr;
                if (instr.operand > 0 && instr.operand < static_cast<int>(program->stringConstants.size())) {
                    field = program->stringConstants[instr.operand];
                }
                stack.back() = object.getField(field);
            }
            frame.ip++;
            break;
        }
        case OpCode::FIELD_SET: {
            if (stack.size() >= 2) {
                // Check if this is struct creation (with DUP) or simple assignment
                // Struct creation has 3+ items on stack: [original, duplicate, value]
                // Simple assignment has 2 items: [object, value]
                bool isStructCreation = (stack.size() >= 3);
                
                VMValue value = stack.back(); stack.pop_back();
                VMValue object = stack.back();
                std::string field = instr.operandStr;
                // Only use stringConstants if operand was explicitly set (> 0)
                if (instr.operand > 0 && instr.operand < static_cast<int>(program->stringConstants.size())) {
                    field = program->stringConstants[instr.operand];
                }
                // Set the field on the object
                // Note: Since VMValue uses shared_ptr for structs, modifications to object
                // are visible to the original (they share the same StructData)
                object.setField(field, value);
                
                // For struct creation (with DUP): pop the duplicate
                // For simple assignment: leave the object on stack
                if (isStructCreation) {
                    // Pop the duplicate struct (the one we got with DUP)
                    // The original struct is still on the stack
                    stack.pop_back();
                }
            }
            frame.ip++;
            break;
        }
        
        // these are dead ops, compiler handles scopes in its own scope stack
        // Scope
        case OpCode::ENTER_SCOPE:
            frame.ip++;
            break;
        case OpCode::EXIT_SCOPE:
            frame.ip++;
            break;
        
        // not emmited actually by the compiler we use JUMP instead right now.
        // Loop control
        case OpCode::BREAK:
            frame.ip++;
            break;
        case OpCode::CONTINUE:
            frame.ip++;
            break;
        
        // Type checking
        case OpCode::TYPEOF: {
            if (!stack.empty()) {
                VMValue& val = stack.back();
                std::string typeName;
                if (val.isInteger()) typeName = "int";
                else if (val.isFloat()) typeName = "float";
                else if (val.isString()) typeName = "string";
                else if (val.isBool()) typeName = "bool";
                else if (val.isList()) typeName = "list";
                else if (val.isStruct()) typeName = "struct";
                else typeName = "none";
                stack.back() = VMValue(typeName);
            }
            frame.ip++;
            break;
        }
        
        // Halt
        case OpCode::HALT:
            halted = true;
            break;
            
        default:
            frame.ip++;
            break;
    }
}

VMValue VirtualMachine::binaryOp(const VMValue& a, const VMValue& b, const std::string& op) {
    if (op == "+") {
        if (a.isString() || b.isString()) {
            std::string result = a.isString() ? a.asString() : a.toString();
            result += b.isString() ? b.asString() : b.toString();
            return VMValue(result);
        }
        if (a.isInteger() && b.isInteger()) {
            return VMValue(a.asInteger() + b.asInteger());
        }
        return VMValue(a.asFloat() + b.asFloat());
    }
    
    if (op == "-") {
        if (a.isInteger() && b.isInteger()) {
            return VMValue(a.asInteger() - b.asInteger());
        }
        return VMValue(a.asFloat() - b.asFloat());
    }
    
    if (op == "*") {
        if (a.isInteger() && b.isInteger()) {
            return VMValue(a.asInteger() * b.asInteger());
        }
        return VMValue(a.asFloat() * b.asFloat());
    }
    
    if (op == "/") {
        if (a.isInteger() && b.isInteger()) {
            return VMValue(a.asInteger() / b.asInteger());
        }
        return VMValue(a.asFloat() / b.asFloat());
    }
    
    if (op == "%") {
        if (a.isInteger() && b.isInteger()) {
            return VMValue(a.asInteger() % b.asInteger());
        }
        return VMValue(std::fmod(a.asFloat(), b.asFloat()));
    }
    
    if (op == "^") {
        if (a.isInteger() && b.isInteger()) {
            return VMValue(static_cast<int64_t>(std::pow(static_cast<double>(a.asInteger()), static_cast<double>(b.asInteger()))));
        }
        return VMValue(std::pow(a.asFloat(), b.asFloat()));
    }
    
    return VMValue(nullptr);
}

bool VirtualMachine::compareOp(const VMValue& a, const VMValue& b, const std::string& op) {
    if (op == "==") {
        return a.equals(b);
    }
    
    if (op == "!=") {
        return !a.equals(b);
    }
    
    if (op == "<") {
        if (a.isInteger() && b.isInteger()) {
            return a.asInteger() < b.asInteger();
        }
        return a.asFloat() < b.asFloat();
    }
    
    if (op == "<=") {
        if (a.isInteger() && b.isInteger()) {
            return a.asInteger() <= b.asInteger();
        }
        return a.asFloat() <= b.asFloat();
    }
    
    if (op == ">") {
        if (a.isInteger() && b.isInteger()) {
            return a.asInteger() > b.asInteger();
        }
        return a.asFloat() > b.asFloat();
    }
    
    if (op == ">=") {
        if (a.isInteger() && b.isInteger()) {
            return a.asInteger() >= b.asInteger();
        }
        return a.asFloat() >= b.asFloat();
    }
    
    return false;
}

const BytecodeFunction* VirtualMachine::findFunction(const VMValue& funcVal) {
    return nullptr;
}

void VirtualMachine::callFunction(const BytecodeFunction& fn, const std::vector<VMValue>& args) {
    // Stack base is current size (arguments were already popped by CALL handler)
    size_t stackBase = stack.size();
    frames.emplace_back(&fn, stackBase);
    
    CallFrame& frame = frames.back();
    for (size_t i = 0; i < args.size() && i < frame.locals.size(); i++) {
        frame.locals[i] = args[i];
    }
}

} 
