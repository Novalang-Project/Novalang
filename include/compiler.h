#pragma once
#include "ast.h"
#include "vm.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <optional>

namespace nova {

// Forward declaration
class Compiler;

// Compiler class - converts AST to bytecode
class Compiler {
public:
    Compiler();

    enum class DeclaredType {
        Int,
        Float,
        String,
        Bool,
        List,
        Struct,
        Any,
        Auto,
        Unknown
    };

    struct VariableInfo {
        int slot;
        DeclaredType type;
        bool initialized = false;  // Track if variable has been assigned a value
    };

    // Compile a program AST to bytecode
    BytecodeProgram compile(Program& program);

private:
    std::unique_ptr<BytecodeProgram> program;
    int currentLine = 1;
    int currentCol = 0;

    // Current function being compiled
    BytecodeFunction* currentFunction = nullptr;
    
    // Function nesting depth (0 = global scope, >0 = inside user-defined function)
    int functionDepth = 0;
    
    // Previous function index (for restoration after exiting a function)
    int prevFunctionIndex = -1;

    // Local variable tracking
    std::unordered_map<std::string, VariableInfo> localVariables;
    int localCount = 0;
    
    // Global variables declared with 'global' keyword in current function
    std::unordered_set<std::string> globalVariables;
    
    // All declared global variables (for semantic analysis)
    std::unordered_set<std::string> declaredGlobals;
    
    // Global variable types (for type checking)
    std::unordered_map<std::string, VariableInfo> globalVariablesInfo;
    
    // Scope stack for proper block-level scoping
    std::vector<std::unordered_map<std::string, VariableInfo>> scopeStack;
    std::vector<int> scopeLocalCount;
    
    // Saved local variables for nested function scopes
    std::vector<std::unordered_map<std::string, VariableInfo>> savedLocalVariables;
    std::vector<int> savedLocalCount;

    // Saved global variables for nested function scopes
    std::vector<std::unordered_set<std::string>> savedGlobalVariables;

    // Track imported files to avoid circular imports
    std::unordered_set<std::string> importedFiles;
    
    // Standard library directory
    std::string standardLibDir;
    
    // Current file directory for resolving imports
    std::string currentFileDir;

    // Loop control flow tracking
    struct LoopContext {
        std::vector<size_t> breakJumps;
        std::vector<size_t> continueJumps;
    };
    std::vector<LoopContext> loopStack;
    
    // Track if current function has explicit return
    bool currentFunctionHasReturn = false;

    // Strict mode for stricter type checking (disabled by default, restricts any usage and function argument type checks)
    bool strictMode = false;

    // Helper methods for compiling expressions
    void compileExpression(Expression& expr);
    void compileLiteral(LiteralExpr& expr);
    void compileBinary(BinaryExpr& expr);
    void compileUnary(UnaryExpr& expr);
    void compileIdentifier(IdentifierExpr& expr);
    void compileAssignment(AssignmentExpr& expr);
    void compileCall(CallExpr& expr);
    void compileMethodCall(MethodCallExpr& expr);
    void compileIndex(IndexExpr& expr);
    void compileList(ListExpr& expr);
    void compileMember(MemberExpr& expr);
    void compileStruct(StructExpr& expr);

    // Helper methods for compiling statements
    void compileStatement(Statement& stmt);
    void compileBlock(BlockStmt& block, bool createScope = false);
    DeclaredType resolveDeclaredType(const TypePtr &type);
    void compileVarDecl(VarDecl &decl);
    void compileFuncDecl(FuncDecl& decl);
    void compileStructDecl(StructDecl& decl);
    void compileIf(IfStmt& stmt);
    void compileWhile(WhileStmt& stmt);
    void compileFor(ForStmt& stmt);
    void compileForIn(ForInStmt& stmt);
    void compileBreak(BreakStmt& stmt);
    void compileContinue(ContinueStmt& stmt);
    void compileReturn(ReturnStmt& stmt);
    void compileGlobal(GlobalStmt& stmt);
    void compileImport(ImportDecl& decl);
    void compileExprStmt(ExprStmt& stmt);
    void compileDeclStmt(DeclStmt& stmt);

    // Emit instructions
    void emit(OpCode op);
    void emitInt(OpCode op, int64_t operand);
    void emitDouble(OpCode op, double operand);
    void emitString(OpCode op, const std::string& operand);
    size_t emitJump(OpCode op);
    size_t emitJumpWithOffset(OpCode op, int64_t offset);
    void patchJump(size_t jumpPos);
    void patchJump(size_t jumpPos, size_t targetPos);

    // Helper to get/set current line
    void setLine(int line) { currentLine = line; }
    void setCol(int col) { currentCol = col; }

    // Get or add string to constant pool
    int addString(const std::string& s);

    // Create a new function
    BytecodeFunction* enterFunction(const std::string& name);
    void exitFunction();

    // Scope management
    void enterScope();
    void exitScope();
    
    // Find variable slot in any scope (returns pair of found and slot index)
    std::pair<bool, int> findVariableInAnyScope(const std::string& name);
    
    // Find variable info in any scope (returns pair of found and VariableInfo)
    std::pair<bool, VariableInfo> findVariableInfoInAnyScope(const std::string& name);
    
    // Infer type from an expression (for 'auto' type)
    DeclaredType inferTypeFromExpression(Expression& expr);
    
    // Check type compatibility
    bool checkTypeCompatibility(DeclaredType declared, DeclaredType actual);
    
    // Import handling
    std::optional<std::string> resolveImportPath(const std::string& path, const std::string& currentFileDir);
    void compileImportFile(const std::string& filepath);
};

} // namespace nova
