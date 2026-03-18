#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <variant>
#include <iostream>
#include <memory>
#include <filesystem>
#include "ast.h"
#include "errors/runtime.h"
#include "compiler.h"
#include "vm.h"

namespace nova {

// Forward declaration
struct StructDecl;

// Runtime value type - holds integers, floats, strings, booleans, lists, structs, or None
struct Value {
    // Use a shared_ptr to a map for struct values
    using StructData = std::unordered_map<std::string, Value>;
    
    std::variant<
        int64_t,      // Integer
        double,       // Float
        std::string,  // String
        bool,         // Boolean
        std::vector<Value>, // List
        std::shared_ptr<StructData>, // Struct
        std::nullptr_t // None
    > data;

    Value() : data(nullptr) {}
    
    explicit Value(int64_t i) : data(i) {}
    explicit Value(double f) : data(f) {}
    explicit Value(const std::string& s) : data(s) {}
    explicit Value(bool b) : data(b) {}
    explicit Value(std::nullptr_t) : data(nullptr) {}
    explicit Value(const std::vector<Value>& v) : data(v) {}
    explicit Value(std::vector<Value>&& v) : data(std::move(v)) {}
    explicit Value(std::shared_ptr<StructData> s) : data(std::move(s)) {}

    bool isInteger() const { return std::holds_alternative<int64_t>(data); }
    bool isFloat() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isList() const { return std::holds_alternative<std::vector<Value>>(data); }
    bool isStruct() const { return std::holds_alternative<std::shared_ptr<StructData>>(data); }
    bool isNone() const { return std::holds_alternative<std::nullptr_t>(data); }

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
    std::vector<Value>& asList() { return std::get<std::vector<Value>>(data); }
    const std::vector<Value>& asList() const { return std::get<std::vector<Value>>(data); }
    StructData& asStruct() { return *std::get<std::shared_ptr<StructData>>(data); }
    const StructData& asStruct() const { return *std::get<std::shared_ptr<StructData>>(data); }

    // Get a field from a struct
    Value getField(const std::string& field) const {
        if (isStruct()) {
            const auto& s = asStruct();
            auto it = s.find(field);
            if (it != s.end()) {
                return it->second;
            }
        }
        throw RuntimeError("Value is not a struct or field does not exist: " + field, 0, 0);
    }

    // Set a field in a struct
    void setField(const std::string& field, const Value& value) {
        if (isStruct()) {
            asStruct()[field] = value;
        } else {
            throw RuntimeError("Value is not a struct", 0, 0);
        }
    }

    std::string toString() const {
        if (isInteger()) return std::to_string(asInteger());
        if (isFloat()) return std::to_string(asFloat());
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
};

// Environment/scope for variables
class Environment {
public:
    Environment() = default;
    
    // Constructor with parent environment (for lexical scoping)
    Environment(Environment* parent) : parent(parent) {}
    
    void define(const std::string& name, Value value) {
        variables[name] = std::move(value);
    }

    Value get(const std::string& name, int line, int col) const {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        // Look up in parent scope
        if (parent) {
            return parent->get(name, line, col);
        }
        throw UndefinedVariableError(line, col, name);
    }
    
    // Get a reference to a value in the environment (non-const for modification)
    Value& getRef(const std::string& name, int line, int col) {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        // Look up in parent scope
        if (parent) {
            return parent->getRef(name, line, col);
        }
        throw UndefinedVariableError(line, col, name);
    }

    void assign(const std::string& name, Value value, int line, int col) {
        auto it = variables.find(name);
        if (it != variables.end()) {
            it->second = std::move(value);
        } else if (parent) {
            // Try to assign in parent scope
            parent->assign(name, std::move(value), line, col);
        } else {
            throw UndefinedVariableError(line, col, name);
        }
    }
    
    // Assign only in current scope (for shadowing)
    void assignLocal(const std::string& name, Value value, int line, int col) {
        auto it = variables.find(name);
        if (it != variables.end()) {
            it->second = std::move(value);
        } else {
            throw UndefinedVariableError(line, col, name);
        }
    }

    bool has(const std::string& name) const {
        if (variables.find(name) != variables.end()) {
            return true;
        }
        if (parent) {
            return parent->has(name);
        }
        return false;
    }
    
    // Check if variable exists in current scope only
    bool hasLocal(const std::string& name) const {
        return variables.find(name) != variables.end();
    }

    // Get all variables (for copying to new environment)
    const std::unordered_map<std::string, Value>& getAll() const {
        return variables;
    }
    
    Environment* getParent() const { return parent; }
    
    void setParent(Environment* p) { parent = p; }

private:
    std::unordered_map<std::string, Value> variables;
    Environment* parent = nullptr;
};

// Interpreter class
class Interpreter {
public:
    Interpreter();

    // Interpret a complete program using bytecode VM
    Value interpret(Program& program);

    // Interpret a single expression (for REPL)
    Value interpretExpression(ExprPtr& expr);

    // Get the result of the last interpretation
    const Value& getLastValue() const { return lastValue; }
    
    // Current file directory for local imports
    std::string currentFileDir;
    
    // Set the current file directory for local imports
    void setCurrentFileDir(const std::string& dir) { currentFileDir = dir; }

private:
    std::unique_ptr<Compiler> compiler;
    std::unique_ptr<VirtualMachine> vm;
    Value lastValue;
    std::unordered_map<std::string, FuncDecl> functions; // Store function definitions
    std::unordered_map<std::string, StructDecl> structs; // Store struct definitions
    
    // Loop control: enum to indicate whether to break, continue, or continue to next iteration
    enum class LoopControl { None, Break, Continue };
    LoopControl loopControl = LoopControl::None;

    // Convert VMValue to Value (for returning results from VM)
    Value convertVMValueToValue(const VMValue& vmVal);

    // Helper to get current (innermost) environment
    Environment* currentEnv();
    
    // Push a new scope
    void pushScope();
    void pushScope(Environment* parent);
    
    // Pop the current scope
    void popScope();

    // Expression evaluation
    Value evaluate(Expression& expr);
    Value evaluateBinary(BinaryExpr& expr);
    Value evaluateUnary(UnaryExpr& expr);
    Value evaluateLiteral(LiteralExpr& expr);
    Value evaluateIdentifier(IdentifierExpr& expr);
    Value evaluateAssignment(AssignmentExpr& expr);
    Value evaluateCall(CallExpr& expr);
    Value evaluateIndex(IndexExpr& expr);
    Value evaluateList(ListExpr& expr);
    Value evaluateMember(MemberExpr& expr);
    Value evaluateStruct(StructExpr& expr);

    // Statement execution
    void execute(Statement& stmt);
    void executeBlock(BlockStmt& block, bool createScope = false);
    void executeVarDecl(VarDecl& decl);
    void executeReturn(ReturnStmt& stmt);
    void executeIf(IfStmt& stmt);
    void executeWhile(WhileStmt& stmt);
    void executeFor(ForStmt& stmt);
    void executeForIn(ForInStmt& stmt);
    void executeBreak(BreakStmt& stmt);
    void executeContinue(ContinueStmt& stmt);
    void executeExpressionStmt(ExprStmt& stmt);
    void executeDeclStmt(DeclStmt& stmt);
    void executeFuncDecl(FuncDecl& decl);
    void executeStructDecl(StructDecl& decl);
    void executeImport(ImportDecl& decl);
    
    // Helper to resolve import path (standard lib first, then local)
    std::optional<std::string> resolveImportPath(const std::string& path, const std::string& currentFileDir);

};

} // namespace nova
