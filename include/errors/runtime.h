#pragma once
#include <string>
#include <stdexcept>

namespace nova {
    
    /**
     * Base class for all runtime errors.
     * Provides line and column information for easy debugging.
     */
    class RuntimeError : public std::runtime_error {
    public:
        RuntimeError(const std::string& message, int line, int column)
            : std::runtime_error(formatMessage(message, line, column)), 
              line(line), column(column) {}
        
        virtual ~RuntimeError() = default;
        
        int getLine() const { return line; }
        int getColumn() const { return column; }
        
    private:
        int line;
        int column;
        
        static std::string formatMessage(const std::string& message, int line, int column) {
            return "Runtime error encountered at line " + std::to_string(line) + 
                   ", column " + std::to_string(column) + ": " + message;
        }
    };
    
    /** Unknown operator error */
    class UnknownOperatorError : public RuntimeError {
    public:
        UnknownOperatorError(int line, int column, const std::string& op)
            : RuntimeError(buildMessage(op), line, column) {}
    private:
        static std::string buildMessage(const std::string& op) {
            return "Unknown operator: " + op;
        }
    };

    /**
     * Error thrown when an operation is performed on incompatible types.
     * Example: trying to add a string and an integer.
     */
    class TypeError : public RuntimeError {
    public:
        TypeError(int line, int column, const std::string& details)
            : RuntimeError(buildMessage(details), line, column) {}
    private:
        static std::string buildMessage(const std::string& details) {
            return "Type error: " + details;
        }
    };

    /**
     * Error thrown when a variable is accessed but not defined.
     * Example: trying to read a variable that was never assigned.
     */
    class UndefinedVariableError : public RuntimeError {
    public:
        UndefinedVariableError(int line, int column, const std::string& varName)
            : RuntimeError(buildMessage(varName), line, column) {}
    private:
        static std::string buildMessage(const std::string& varName) {
            return "Undefined variable: \"" + varName + "\" - make sure it is defined before use";
        }
    };

    /**
     * Error thrown when a function is called with the wrong type of arguments.
     * Example: trying to call a function that expects an integer with a string.
     */
    class InvalidArgumentTypeError : public RuntimeError {
    public:
        InvalidArgumentTypeError(int line, int column, const std::string& details)
            : RuntimeError(buildMessage(details), line, column) {}
    private:
        static std::string buildMessage(const std::string& details) {
            return "Invalid argument type: " + details;
        }
    };

    /**
     * Error thrown when a function is called with the wrong number of arguments.
     * Example: calling a function that expects 2 arguments with only 1.
     */
    class ArgumentError : public RuntimeError {
    public:     
        ArgumentError(int line, int column, const std::string& details)
            : RuntimeError(buildMessage(details), line, column) {}
    private:
        static std::string buildMessage(const std::string& details) {
            return "Argument error: " + details;
        }
    };

    /**
     * Error thrown when an index is out of bounds.
     * Example: trying to access list[10] when the list only has 5 elements.
     */
    class IndexError : public RuntimeError {
    public:
        IndexError(int line, int column, const std::string& details)
            : RuntimeError(buildMessage(details), line, column) {}
    private:
        static std::string buildMessage(const std::string& details) {
            return "Index Error: " + details;
        }
    };

    /**
     * Error thrown when a value is not iterable but is used in a context that requires iteration.
     * Example: trying to loop over an integer or call len() on an integer.
     */
    class NotIterableError : public RuntimeError {
    public:
        NotIterableError(int line, int column, const std::string& details)
            : RuntimeError(buildMessage(details), line, column) {}
    private:
        static std::string buildMessage(const std::string& details) {
            return "Not iterable: " + details;
        }
    };

    /**
     * Error thrown when a value is not callable but is used in a function call context.
     * Example: trying to call an integer like a function.
     */
    class NotCallableError : public RuntimeError {
    public:
        NotCallableError(int line, int column, const std::string& details)
            : RuntimeError(buildMessage(details), line, column) {}
    private:
        static std::string buildMessage(const std::string& details) {
            return "Error: " + details + " is not callable";
        }
    };

    /**
     * Error thrown when a division by zero is attempted.
     * Example: trying to evaluate 10 / 0.
     */
    class DivisionByZeroError : public RuntimeError {
    public:
        DivisionByZeroError(int line, int column)
            : RuntimeError("Division by zero is not allowed", line, column) {}
    };

    /**
    * Error thrown when a value is used in an unsupported way.
    * Example: trying to add a function to an integer.
    */
    class InvalidOperationError : public RuntimeError {
    public:
        InvalidOperationError(int line, int column, const std::string& details)
            : RuntimeError(buildMessage(details), line, column) {}
    private:
        static std::string buildMessage(const std::string& details) {
            return "Invalid operation: " + details;
        }
    };

    /**
     * Undefined function error - thrown when trying to call a function that does not exist.
     * Example: trying to call foo() when no function named foo is defined.
     */
    class UndefinedFunctionError : public RuntimeError {
    public:
        UndefinedFunctionError(int line, int column, const std::string& funcName)
            : RuntimeError(buildMessage(funcName), line, column) {}
    private:
        static std::string buildMessage(const std::string& funcName) {
            return "Undefined function: \"" + funcName + "\" - make sure it is defined before use";
        }
    };  
}