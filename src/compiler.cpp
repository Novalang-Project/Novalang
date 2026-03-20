#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "errors/runtime.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace nova {

// ============================================================================
// Compiler Implementation
// ============================================================================

// Initializes the compiler state and determines the standard library location.
// Priority order:
// 1. NOVALANG_STDLIB environment variable
// 2. Local ./standardlib directory (development)
// 3. XDG_DATA_HOME/novalang/stdlib
// 4. ~/.local/share/novalang/stdlib (fallback)
// Also creates the bytecode program and sets up the default "main" function.
Compiler::Compiler() {
    program = std::make_unique<BytecodeProgram>();

    enterFunction("main");

    const char* envStdlib = std::getenv("NOVALANG_STDLIB");
    if (envStdlib) {
        standardLibDir = envStdlib;
    } else {
        // Try both .nv and .nova extensions for stdlib detection
        std::vector<std::string> extensions = {".nv", ".nova"};
        bool foundStdlib = false;
        
        // Check current directory
        for (const auto& ext : extensions) {
            std::ifstream test("./standardlib/strings" + ext);
            if (test.good()) {
                standardLibDir = "./standardlib";
                test.close();
                foundStdlib = true;
                break;
            }
        }
        
        // Check XDG_DATA_HOME
        if (!foundStdlib) {
            const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
            if (xdgDataHome) {
                std::string basePath = std::string(xdgDataHome) + "/novalang/stdlib";
                for (const auto& ext : extensions) {
                    std::ifstream test(basePath + "/strings" + ext);
                    if (test.good()) {
                        standardLibDir = basePath;
                        test.close();
                        foundStdlib = true;
                        break;
                    }
                }
            }
        }
        
        // Check HOME/.local/share/novalang/stdlib
        if (!foundStdlib) {
            const char* home = std::getenv("HOME");
            if (home) {
                std::string basePath = std::string(home) + "/.local/share/novalang/stdlib";
                for (const auto& ext : extensions) {
                    std::ifstream test(basePath + "/strings" + ext);
                    if (test.good()) {
                        standardLibDir = basePath;
                        test.close();
                        foundStdlib = true;
                        break;
                    }
                }
            }
        }
        
        if (standardLibDir.empty()) {
            standardLibDir = "./standardlib";
        }
    }
}

// Compiles the AST into a BytecodeProgram.
// Resets compiler state (for REPL reuse), emits all top-level declarations,
// and generates a _start entry point that optionally calls user-defined "main".
BytecodeProgram Compiler::compile(Program& programAst) {
    program = std::make_unique<BytecodeProgram>();
    
    functionDepth = 0;
    savedLocalVariables.clear();
    savedLocalCount.clear();
    
    enterFunction("_start");
    
    for (auto& decl : programAst.decls) {
        if (auto* funcDecl = dynamic_cast<FuncDecl*>(decl.get())) {
            compileFuncDecl(*funcDecl);
        } else if (auto* structDecl = dynamic_cast<StructDecl*>(decl.get())) {
            compileStructDecl(*structDecl);
        } else if (auto* varDecl = dynamic_cast<VarDecl*>(decl.get())) {
            compileVarDecl(*varDecl);
        } else if (auto* importDecl = dynamic_cast<ImportDecl*>(decl.get())) {
            compileImport(*importDecl);
        } else if (auto* stmtDecl = dynamic_cast<StmtDecl*>(decl.get())) {
            if (stmtDecl->stmt) {
                compileStatement(*stmtDecl->stmt);
            }
        } else if (auto* declStmt = dynamic_cast<DeclStmt*>(decl.get())) {
            if (declStmt->decl) {
                if (auto* varDecl = dynamic_cast<VarDecl*>(declStmt->decl.get())) {
                    compileVarDecl(*varDecl);
                } else if (auto* funcDecl = dynamic_cast<FuncDecl*>(declStmt->decl.get())) {
                    compileFuncDecl(*funcDecl);
                } else if (auto* structDecl = dynamic_cast<StructDecl*>(declStmt->decl.get())) {
                    compileStructDecl(*structDecl);
                }
            }
        } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(decl.get())) {
            compileExprStmt(*exprStmt);
        } else {
            std::cerr << "DEBUG: Unknown decl type: " << typeid(*decl).name() << "\n";
        }
    }

    emitString(OpCode::PUSH_STRING, "main");
    emitInt(OpCode::CALL, 0); 
    emit(OpCode::POP);
    
    emit(OpCode::HALT);

    return std::move(*program);
}

// ============================================================================
// Expression Compilation
// ============================================================================

// Compile a generic expression by dispatching to the appropriate
// compile* function based on the expression type.
void Compiler::compileExpression(Expression& expr) {
    if (auto* lit = dynamic_cast<LiteralExpr*>(&expr)) {
        compileLiteral(*lit);
    } else if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        compileBinary(*bin);
    } else if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        compileUnary(*un);
    } else if (auto* id = dynamic_cast<IdentifierExpr*>(&expr)) {
        compileIdentifier(*id);
    } else if (auto* assign = dynamic_cast<AssignmentExpr*>(&expr)) {
        compileAssignment(*assign);
    } else if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        compileCall(*call);
    } else if (auto* idx = dynamic_cast<IndexExpr*>(&expr)) {
        compileIndex(*idx);
    } else if (auto* list = dynamic_cast<ListExpr*>(&expr)) {
        compileList(*list);
    } else if (auto* member = dynamic_cast<MemberExpr*>(&expr)) {
        compileMember(*member);
    } else if (auto* s = dynamic_cast<StructExpr*>(&expr)) {
        compileStruct(*s);
    } else if (auto* methodCall = dynamic_cast<MethodCallExpr*>(&expr)) {
        compileMethodCall(*methodCall);
    } else {
        // Tuple and other expressions which are not implemented yet - emit nil
        emit(OpCode::PUSH_NIL);
    }
}

// Compile a literal expression and emit the corresponding bytecode.
// Supports integers, floats, booleans, strings, and 'none'.
void Compiler::compileLiteral(LiteralExpr& expr) {
    setLine(expr.loc.line);
    setCol(expr.loc.column);

    switch (expr.kind) {
        case LiteralExpr::LitKind::Boolean:
            if (expr.raw == "true") {
                emit(OpCode::PUSH_TRUE);
            } else {
                emit(OpCode::PUSH_FALSE);
            }
            break;
        case LiteralExpr::LitKind::Integer:
            emitInt(OpCode::PUSH_INT, static_cast<int64_t>(std::stoll(expr.raw)));
            break;
        case LiteralExpr::LitKind::Float:
            emitDouble(OpCode::PUSH_FLOAT, std::stod(expr.raw));
            break;
        case LiteralExpr::LitKind::String:
            emitString(OpCode::PUSH_STRING, expr.raw);
            break;
        case LiteralExpr::LitKind::FString: {
            // F-string: parse and compile at compile time
            // Format: f"Hello {name}!" -> compile as fstring("Hello {}", name)
            const std::string& fstr = expr.raw;
            std::string formatStr;
            std::vector<std::string> varNames;
            
            // Parse the f-string to extract format parts
            for (size_t i = 0; i < fstr.length(); i++) {
                if (fstr[i] == '{') {
                    // Find closing brace
                    size_t j = i + 1;
                    while (j < fstr.length() && fstr[j] != '}') j++;
                    
                    if (j < fstr.length() && j > i + 1) {
                        // Extract variable name
                        std::string varName = fstr.substr(i + 1, j - i - 1);
                        varNames.push_back(varName);
                        formatStr += "{}";  // Replace {var} with placeholder
                        i = j;  // Skip to after }
                    } else {
                        formatStr += fstr[i];
                    }
                } else {
                    formatStr += fstr[i];
                }
            }
            
            // Emit ARG_COUNT for variadic call
            emitInt(OpCode::ARG_COUNT, static_cast<int64_t>(varNames.size() + 1));
            
            // Push format string
            emitString(OpCode::PUSH_STRING, formatStr);
            
            // For each variable, we need to look it up and push its value
            // Use findVariableInAnyScope to find the variable
            for (const auto& varName : varNames) {
                auto [found, slot] = findVariableInAnyScope(varName);
                
                if (found) {
                    // Local variable - use slot index
                    emitInt(OpCode::LOAD_LOCAL, slot);
                } else if (declaredGlobals.find(varName) != declaredGlobals.end()) {
                    // Global variable - emit LOAD_GLOBAL with string name
                    emitString(OpCode::LOAD_GLOBAL, varName);
                } else {
                    // Variable not found - push empty string as placeholder
                    emitString(OpCode::PUSH_STRING, "");
                }
            }
            
            // Call fstring builtin (index 21)
            emitInt(OpCode::BUILTIN, 21);
            break;
        }
        case LiteralExpr::LitKind::None:
            emit(OpCode::PUSH_NIL);
            break;
    }
}

// Compile a binary expression by first compiling its operands
// and then emitting the appropriate bytecode for the operator.
void Compiler::compileBinary(BinaryExpr& expr) {
    compileExpression(*expr.left);
    compileExpression(*expr.right);
    
    setLine(expr.loc.line);
    setCol(expr.loc.column);
    
    if (expr.op == "+") {
        emit(OpCode::ADD);
    } else if (expr.op == "-") {
        emit(OpCode::SUB);
    } else if (expr.op == "*") {
        emit(OpCode::MUL);
    } else if (expr.op == "/") {
        emit(OpCode::DIV);
    } else if (expr.op == "%") {
        emit(OpCode::MOD);
    } else if (expr.op == "^") {
        emit(OpCode::POW);
    } else if (expr.op == "==") {
        emit(OpCode::EQ);
    } else if (expr.op == "!=") {
        emit(OpCode::NEQ);
    } else if (expr.op == "<") {
        emit(OpCode::LT);
    } else if (expr.op == "<=") {
        emit(OpCode::LTE);
    } else if (expr.op == ">") {
        emit(OpCode::GT);
    } else if (expr.op == ">=") {
        emit(OpCode::GTE);
    } else if (expr.op == "&&") {
        emit(OpCode::AND);
    } else if (expr.op == "||") {
        emit(OpCode::OR);
    } else if (expr.op == "&") {
        emit(OpCode::BIT_AND);
    } else if (expr.op == "|") {
        emit(OpCode::BIT_OR);
    } else if (expr.op == "<<") {
        emit(OpCode::SHL);
    } else if (expr.op == ">>") {
        emit(OpCode::SHR);
    }
}

// Compile a unary expression by first compiling its operand and then
// emitting the appropriate bytecode for the operator.
void Compiler::compileUnary(UnaryExpr& expr) {
    compileExpression(*expr.operand);
    
    setLine(expr.loc.line);
    setCol(expr.loc.column);

    if (expr.op == "-") {
        emit(OpCode::NEG);
    } else if (expr.op == "!") {
        emit(OpCode::NOT);
    } else if (expr.op == "++") {
        emit(OpCode::INC);
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.operand.get())) {
            auto [found, slot] = findVariableInAnyScope(ident->name);
            if (found) {
                emitInt(OpCode::STORE_LOCAL, slot);
            } else if (globalVariables.find(ident->name) != globalVariables.end()) {
                emitString(OpCode::STORE_GLOBAL, ident->name);
            } else if (functionDepth == 0 && declaredGlobals.find(ident->name) != declaredGlobals.end()) {
                // At top level, allow incrementing declared globals
                emitString(OpCode::STORE_GLOBAL, ident->name);
            } else if (functionDepth > 0) {
                throw AssigmentToGlobalVariableError(currentLine,currentCol,ident->name);
            }
        }
    } else if (expr.op == "--") {
        emit(OpCode::DEC);
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.operand.get())) {
            auto [found, slot] = findVariableInAnyScope(ident->name);
            if (found) {
                emitInt(OpCode::STORE_LOCAL, slot);
            } else if (globalVariables.find(ident->name) != globalVariables.end()) {
                emitString(OpCode::STORE_GLOBAL, ident->name);
            } else if (functionDepth == 0 && declaredGlobals.find(ident->name) != declaredGlobals.end()) {
                // At top level, allow decrementing declared globals
                emitString(OpCode::STORE_GLOBAL, ident->name);
            } else if (functionDepth > 0) {
                throw AssigmentToGlobalVariableError(currentLine,currentCol,ident->name);
            }
        }
    } else if (expr.op == "~") {
        emit(OpCode::BIT_NOT);
    }
}

// Compile an identifier expression by emitting bytecode to load its value.
// It first checks if the identifier is a declared global, then a local variable,
// and defaults to loading a global variable if not found locally.
// Also enforces namespace usage for imported modules.
void Compiler::compileIdentifier(IdentifierExpr& expr) {
    setLine(expr.loc.line);
    setCol(expr.loc.column);

    auto [found, slot] = findVariableInAnyScope(expr.name);
    if (found) {
        emitInt(OpCode::LOAD_LOCAL, slot);
        return;
    }

    if (globalVariables.find(expr.name) != globalVariables.end() ||
        declaredGlobals.find(expr.name) != declaredGlobals.end()) {
        emitString(OpCode::LOAD_GLOBAL, expr.name);
        return;
    }
    
    // Check if this identifier is from an imported namespace (but not selectively imported)
    // If so, enforce namespace usage
    for (const auto& [nsName, symbols] : importedNamespaces) {
        if (symbols.find(expr.name) != symbols.end()) {
            // This symbol is from an imported namespace
            // Check if it was also selectively imported (those are allowed without prefix)
            if (selectiveImports.find(expr.name) == selectiveImports.end()) {
                throw ImportError(currentLine, currentCol,
                    "Symbol '" + expr.name + "' is from namespace '" + nsName + "'. "
                    "Use '" + nsName + "." + expr.name + "' to access it, or "
                    "use 'import { " + expr.name + " } from \"" + nsName + "\"' for direct access.");
            }
        }
    }
    
    // Variable not found - error
    throw UndefinedVariableError(currentLine, currentCol, expr.name);
}

// Compile an assignment expression, handling different target types:
// - Index assignment (e.g., arr[0] = value)
// - Member assignment (e.g., obj.field = value)
// - Simple variable assignment (local or global)
// Supports compound operators like +=, -=, *=, /=.
void Compiler::compileAssignment(AssignmentExpr& expr) {
    if (auto* indexExpr = dynamic_cast<IndexExpr*>(expr.target.get())) {
        if (auto* ident = dynamic_cast<IdentifierExpr*>(indexExpr->collection.get())) {
            compileExpression(*indexExpr->collection);  
            compileExpression(*indexExpr->index);        
            compileExpression(*expr.value);             
            emit(OpCode::INDEX_SET);
            
            // Store the modified collection back to the variable
            if (globalVariables.find(ident->name) != globalVariables.end()) {
                emitString(OpCode::STORE_GLOBAL, ident->name);
            } else {
                auto [found, slot] = findVariableInAnyScope(ident->name);
                if (found) {
                    emitInt(OpCode::STORE_LOCAL, slot);
                } else if (functionDepth == 0) {
                    if (declaredGlobals.find(ident->name) != declaredGlobals.end()) {
                        emitString(OpCode::STORE_GLOBAL, ident->name);
                    }
                }
            }
            return;
        }
        
        compileExpression(*expr.target);
        compileExpression(*expr.value);
        emit(OpCode::INDEX_SET);
        return;
    }
    
    if (auto* member = dynamic_cast<MemberExpr*>(expr.target.get())) {
        compileExpression(*member->object);
        compileExpression(*expr.value);

        emitString(OpCode::FIELD_SET, member->field);
        return;
    }

    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.target.get())) {
        if (expr.op == "=") {
            compileExpression(*expr.value);
        } else {
            compileIdentifier(*ident);
            compileExpression(*expr.value);
            
            if (expr.op == "+=") {
                emit(OpCode::ADD);
            } else if (expr.op == "-=") {
                emit(OpCode::SUB);
            } else if (expr.op == "*=") {
                emit(OpCode::MUL);
            } else if (expr.op == "/=") {
                emit(OpCode::DIV);
            }
        }
        
        // Type checking for assigned variables (not just in strict mode)
        auto [found, varInfo] = findVariableInfoInAnyScope(ident->name);
        if (found) {
            DeclaredType actualType = inferTypeFromExpression(*expr.value);
                        bool enforceTypeCheck = false;
            
            if (varInfo.type == DeclaredType::Any) {
                enforceTypeCheck = strictMode;
            } else if (varInfo.type == DeclaredType::Auto || varInfo.type == DeclaredType::Unknown) {
                enforceTypeCheck = varInfo.initialized;
            } else {
                enforceTypeCheck = true;
            }
            
            if (enforceTypeCheck && !checkTypeCompatibility(varInfo.type, actualType)) {
                throw TypeError(currentLine, currentCol, "Cannot assign '" + 
                    std::string(actualType == DeclaredType::Int ? "int" :
                               actualType == DeclaredType::Float ? "float" :
                               actualType == DeclaredType::String ? "string" :
                               actualType == DeclaredType::Bool ? "bool" :
                               actualType == DeclaredType::List ? "list" :
                               actualType == DeclaredType::Struct ? "struct" : "unknown") +
                    "' to variable '" + ident->name + "' of type '" +
                    std::string(varInfo.type == DeclaredType::Int ? "int" :
                              varInfo.type == DeclaredType::Float ? "float" :
                              varInfo.type == DeclaredType::String ? "string" :
                              varInfo.type == DeclaredType::Bool ? "bool" :
                              varInfo.type == DeclaredType::List ? "list" :
                              varInfo.type == DeclaredType::Struct ? "struct" :
                              varInfo.type == DeclaredType::Auto ? "auto" :
                              varInfo.type == DeclaredType::Any ? "any" : "unknown") + "'");
            }
            
            // If 'auto' was used and not yet initialized, lock the type
            if ((varInfo.type == DeclaredType::Auto || varInfo.type == DeclaredType::Unknown) && !varInfo.initialized) {
                if (localVariables.find(ident->name) != localVariables.end()) {
                    localVariables[ident->name].type = actualType;
                    localVariables[ident->name].initialized = true;
                }
            }
        }
        
        if (globalVariables.find(ident->name) != globalVariables.end()) {
            emitString(OpCode::STORE_GLOBAL, ident->name);
        } else {
            auto [found, slot] = findVariableInAnyScope(ident->name);
            if (found) {
                emitInt(OpCode::STORE_LOCAL, slot);
            } else if (functionDepth == 0) {
                if (declaredGlobals.find(ident->name) != declaredGlobals.end()) {
                    emitString(OpCode::STORE_GLOBAL, ident->name);
                } else {
                    throw UndefinedVariableError(currentLine, currentCol, ident->name);
                }
            } else {
                throw AssigmentToGlobalVariableError(currentLine,currentCol, ident->name);
            }
        }
        return;
    }
    
    emit(OpCode::POP);
}

// Compile a function call expression.
// Handles both built-in functions (len, push, pop, println, removeAt, input)
// and user-defined functions.
// For built-ins that modify a local variable (push/pop/removeAt), the first
// argument is checked to emit STORE_LOCAL for the result.
void Compiler::compileCall(CallExpr& expr) {
    // Handle namespace function calls: strings.concat(a, b)
    if (auto* member = dynamic_cast<MemberExpr*>(expr.callee.get())) {
        if (auto* ident = dynamic_cast<IdentifierExpr*>(member->object.get())) {
            std::string nsName = ident->name;
            std::string fieldName = member->field;
            
            auto nsIt = importedNamespaces.find(nsName);
            if (nsIt != importedNamespaces.end()) {
                const auto& symbols = nsIt->second;
                if (symbols.find(fieldName) != symbols.end()) {
                    // This is a namespace function call - emit as string call
                    std::string fullName = nsName + "." + fieldName;
                    emitString(OpCode::PUSH_STRING, fullName);
                    
                    for (auto& arg : expr.args) {
                        compileExpression(*arg);
                    }
                    
                    emitInt(OpCode::CALL, static_cast<int64_t>(expr.args.size()));
                    return;
                }
            }
        }
    }
    
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.callee.get())) {
        std::string funcName = ident->name;
        
        for (const auto& [nsName, symbols] : importedNamespaces) {
            if (symbols.find(funcName) != symbols.end()) {
                if (selectiveImports.find(funcName) == selectiveImports.end()) {
                    throw ImportError(currentLine, currentCol,
                        "Function '" + funcName + "' is from namespace '" + nsName + "'. "
                        "Use '" + nsName + "." + funcName + "()' to call it, or "
                        "use 'import { " + funcName + " } from \"" + nsName + "\"' for direct access.");
                }
            }
        }
        
        // Check if this is an aliased import, resolve to original function name
        auto aliasIt = selectiveImports.find(funcName);
        if (aliasIt != selectiveImports.end()) {
            std::string fullName = aliasIt->second;
            size_t dotPos = fullName.rfind('.');
            if (dotPos != std::string::npos) {
                funcName = fullName.substr(dotPos + 1);
            }
        }
        
        if (funcName == "len" || funcName == "push" || funcName == "pop" || 
            funcName == "println" || funcName == "removeAt" || funcName == "input" ||
            funcName == "toInt" || funcName == "toFloat" || funcName == "toString" || funcName == "toBool" ||
            funcName == "typeof") {
            int localIdx = -1;
            bool isPushPopRemoveAt = (funcName == "push" || funcName == "pop" || funcName == "removeAt");
            
            for (size_t i = 0; i < expr.args.size(); i++) {
                compileExpression(*expr.args[i]);
                
                if (isPushPopRemoveAt && i == 0) {
                    if (auto* argIdent = dynamic_cast<IdentifierExpr*>(expr.args[i].get())) {
                        auto [found, slot] = findVariableInAnyScope(argIdent->name);
                        if (found) {
                            localIdx = slot;
                        }
                    }
                }
            }
            
            emitInt(OpCode::ARG_COUNT, static_cast<int64_t>(expr.args.size()));
            
            // when adding a new builtin don't be stupid like me and dont forget to map it here D:
            static std::unordered_map<std::string, int> builtinMap = {
                {"len", 0},
                {"push", 1},
                {"pop", 2},
                {"println", 3},
                {"removeAt", 4},
                {"input", 5},
                {"format", 6},
                {"round", 7},
                {"toInt", 8},
                {"toFloat", 9},
                {"toString", 10},
                {"toBool", 11},
                {"typeof", 12},
                // File I/O builtins
                {"open", 13},
                {"close", 14},
                {"read", 15},
                {"read_line", 16},
                {"write", 17},
                {"read_file", 18},
                {"write_file", 19},
                // Debug and f-string
                {"debug", 20},
                {"fstring", 21}
            };
            
            emitInt(OpCode::BUILTIN, builtinMap[funcName]);
            
            if (localIdx >= 0) {
                emitInt(OpCode::STORE_LOCAL, localIdx);
            }
            return;
        }
        
        emitString(OpCode::PUSH_STRING, funcName);
        
        for (auto& arg : expr.args) {
            compileExpression(*arg);
        }
        
        emitInt(OpCode::CALL, static_cast<int64_t>(expr.args.size()));
        return;
    }
    
    emit(OpCode::PUSH_NIL);
}

// Compile a method call expression: object.method(args).
// Currently only supports known list methods: push, pop, removeAt.
// The object is passed as the first argument, followed by the method arguments.
// If the object is a local variable, the result is stored back to it.
void Compiler::compileMethodCall(MethodCallExpr& expr) {
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
        std::string nsName = ident->name;
        
        auto nsIt = importedNamespaces.find(nsName);
        if (nsIt != importedNamespaces.end()) {
            const auto& symbols = nsIt->second;
            if (symbols.find(expr.method) != symbols.end()) {
                std::string fullName = nsName + "." + expr.method;
                emitString(OpCode::PUSH_STRING, fullName);
                
                for (auto& arg : expr.args) {
                    compileExpression(*arg);
                }
                
                emitInt(OpCode::CALL, static_cast<int64_t>(expr.args.size()));
                return;
            }
        }
    }
    
    std::string methodName = expr.method;
    
    if (methodName == "push" || methodName == "pop" || methodName == "removeAt") {
        compileExpression(*expr.object);
        
        for (auto& arg : expr.args) {
            compileExpression(*arg);
        }
        
        emitInt(OpCode::ARG_COUNT, static_cast<int64_t>(expr.args.size() + 1));
        
        static std::unordered_map<std::string, int> methodBuiltinMap = {
            {"push", 1},
            {"pop", 2},
            {"removeAt", 4}
        };
        
        emitInt(OpCode::BUILTIN, methodBuiltinMap[methodName]);
        
        int localIdx = -1;
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
            auto [found, slot] = findVariableInAnyScope(ident->name);
            if (found) {
                localIdx = slot;
            }
        }
        
        if (localIdx >= 0) {
            emitInt(OpCode::STORE_LOCAL, localIdx);
        }
        
        return;
    }
    
    emit(OpCode::PUSH_NIL);
}

// Compile an index access expression: collection[index].
// Pushes the value at the specified index onto the stack.
void Compiler::compileIndex(IndexExpr& expr) {
    compileExpression(*expr.collection);
    compileExpression(*expr.index);
    
    setLine(expr.loc.line);
    setCol(expr.loc.column);
    emit(OpCode::INDEX_GET);
}

// Compile a list expression: [val1, val2, ...].
// Each element is compiled and pushed onto the stack, then a LIST opcode
// creates the list with the correct number of elements.
void Compiler::compileList(ListExpr& expr) {
    for (auto& elem : expr.elements) {
        compileExpression(*elem);
    }
    
    emitInt(OpCode::LIST, static_cast<int64_t>(expr.elements.size()));
}

// Compile a member access expression: object.field.
// Compiles the object and emits a FIELD_GET opcode to access the specified field.
// Also validates namespace access for imported modules.
void Compiler::compileMember(MemberExpr& expr) {
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
        std::string nsName = ident->name;
        
        auto nsIt = importedNamespaces.find(nsName);
        if (nsIt != importedNamespaces.end()) {
            const auto& symbols = nsIt->second;
            if (symbols.find(expr.field) == symbols.end()) {
                throw ImportError(currentLine, currentCol,
                    "Symbol '" + expr.field + "' not found in namespace '" + nsName + "'");
            }
        }
    }
    
    compileExpression(*expr.object);
    
    emitString(OpCode::FIELD_GET, expr.field);
}

// Compile a struct literal expression: StructType { field1: val1, ... }.
// Emits STRUCT_NEW to create the struct, then sets each field by compiling
// its value and using FIELD_SET.
void Compiler::compileStruct(StructExpr& expr) {
    emitString(OpCode::STRUCT_NEW, expr.typeName);
    
    for (auto& fieldPair : expr.fields) {
        emit(OpCode::DUP);

        compileExpression(*fieldPair.second);

        emitString(OpCode::FIELD_SET, fieldPair.first);
    }
}

// ============================================================================
// Statement Compilation
// ============================================================================

// Dispatch compilation of a statement based on its actual type.
// Each statement type is handled by its corresponding compile* function.
void Compiler::compileStatement(Statement& stmt) {
    if (auto* block = dynamic_cast<BlockStmt*>(&stmt)) {
        compileBlock(*block);
    } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(&stmt)) {
        compileExprStmt(*exprStmt);
    } else if (auto* declStmt = dynamic_cast<DeclStmt*>(&stmt)) {
        compileDeclStmt(*declStmt);
    } else if (auto* ret = dynamic_cast<ReturnStmt*>(&stmt)) {
        compileReturn(*ret);
    } else if (auto* ifStmt = dynamic_cast<IfStmt*>(&stmt)) {
        compileIf(*ifStmt);
    } else if (auto* whileStmt = dynamic_cast<WhileStmt*>(&stmt)) {
        compileWhile(*whileStmt);
    } else if (auto* forStmt = dynamic_cast<ForStmt*>(&stmt)) {
        compileFor(*forStmt);
    } else if (auto* forInStmt = dynamic_cast<ForInStmt*>(&stmt)) {
        compileForIn(*forInStmt);
    } else if (auto* breakStmt = dynamic_cast<BreakStmt*>(&stmt)) {
        compileBreak(*breakStmt);
    } else if (auto* continueStmt = dynamic_cast<ContinueStmt*>(&stmt)) {
        compileContinue(*continueStmt);
    } else if (auto* globalStmt = dynamic_cast<GlobalStmt*>(&stmt)) {
        compileGlobal(*globalStmt);
    }
}

// Compile a block of statements, optionally creating a new scope.
// If createScope is true, emit ENTER_SCOPE/EXIT_SCOPE opcodes
// and manage the compiler’s internal scope stack.
void Compiler::compileBlock(BlockStmt& block, bool createScope) {
    if (createScope) {
        emit(OpCode::ENTER_SCOPE);
        enterScope();
    }
    
    for (auto& stmt : block.statements) {
        compileStatement(*stmt);
    }
    
    if (createScope) {
        emit(OpCode::EXIT_SCOPE);
        exitScope();
    }
}

// Compile a variable declaration.
// Handles optional initialization and determines whether the variable
// is global (top-level) or local (inside a function).
void Compiler::compileVarDecl(VarDecl& decl) {
    // Resolve the declared type from the AST
    DeclaredType declaredType = resolveDeclaredType(decl.type);
    
    bool hasInitializer = (decl.init != nullptr);
    
    if (decl.init) {
        compileExpression(*decl.init);
        
        // If declared type is auto, infer from initializer
        if (declaredType == DeclaredType::Auto) {
            declaredType = inferTypeFromExpression(*decl.init);
        }
    } else {
        emit(OpCode::PUSH_NIL);
    }
    
    bool isGlobal = (functionDepth == 0);
    
    if (isGlobal) {
        declaredGlobals.insert(decl.name);
        globalVariablesInfo[decl.name] = {-1, declaredType, hasInitializer};
        emitString(OpCode::STORE_GLOBAL, decl.name);
    } else {
        if (localVariables.find(decl.name) == localVariables.end()) {
            localVariables[decl.name] = {localCount++, declaredType, hasInitializer};
            currentFunction->locals.push_back(decl.name);
        }
        emitInt(OpCode::STORE_LOCAL, localVariables[decl.name].slot);
    }
    emit(OpCode::POP);
}

// Compile a function declaration.
// Sets up a new function context, adds parameters as locals,
// compiles the function body, and emits an implicit return if needed.
void Compiler::compileFuncDecl(FuncDecl& decl) {
    // Track function name for collision detection (current file only)
    if (functionDepth == 0) {
        currentFileFunctions.insert(decl.name);
    }
    
    BytecodeFunction* newFunc = enterFunction(decl.name);
    newFunc->numParams = decl.params.size();
    
    for (auto& param : decl.params) {
        DeclaredType paramType = resolveDeclaredType(param.type);
        localVariables[param.name] = {localCount++, paramType};
        newFunc->locals.push_back(param.name);
    }
    
    currentFunctionHasReturn = false;
    
    compileBlock(decl.body);
    
    if (!currentFunctionHasReturn) {
        emit(OpCode::PUSH_NIL);
        emit(OpCode::RETURN_VALUE);
    }
    
    exitFunction();
    
    // Note: We do NOT clear localVariables here because they belong to the
    // function's scope. The outer scope's variables are preserved in
    // localVariables and will be used when compiling subsequent code.
    // The function's localVariables were for the function's parameters.
    // localCount is reset in exitFunction if needed.
}

// Struct declarations are handled at compile-time for type checking
// The actual struct creation is done in runtime via STRUCT_NEW opcode
// We just need to make sure the type name is in the constant pool
void Compiler::compileStructDecl(StructDecl& decl) {
    addString(decl.name);
}

// Compile an if statement, including optional else branch.
// 1. Compile the condition expression and emit a conditional jump.
// 2. Compile the "then" block.
// 3. If an else block exists, emit a jump to skip it after "then",
//    patch the conditional jump to the start of the "else" block,
//    compile the "else" block, then patch the end jump.
// 4. If no else block, patch the conditional jump to continue after "then".
void Compiler::compileIf(IfStmt& stmt) {
    compileExpression(*stmt.cond);

    size_t elseJump = emitJump(OpCode::JUMP_IF_FALSE);

    compileBlock(stmt.thenBody, true);

    if (stmt.hasElse) {
        size_t endJump = emitJump(OpCode::JUMP);

        patchJump(elseJump);

        compileBlock(stmt.elseBody, true);

        patchJump(endJump);
    } else {
        patchJump(elseJump);
    }
}

// Compile a while loop statement.
// 1. Push a new loop context to track break/continue jumps.
// 2. Record the start of the loop for jumping back after each iteration.
// 3. Compile the loop condition and emit a jump to exit if false.
// 4. Compile the loop body.
// 5. Emit an unconditional jump back to the loop start.
// 6. Patch the exit jump to after the loop body.
// 7. Patch any continue jumps to the loop start (or next iteration).
// 8. Patch any break jumps to after the loop.
// 9. Pop the loop context.
void Compiler::compileWhile(WhileStmt& stmt) {
    loopStack.push_back(LoopContext());
    size_t loopIdx = loopStack.size() - 1;
    
    size_t loopStart = currentFunction->instructions.size();
    
    compileExpression(*stmt.cond);
    
    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    
    // Note: Don't create a new scope here, the while loop body should share
    // the function's local scope. Block scoping is handled by if/for/etc at statement level.
    compileBlock(stmt.body);
    
    emitJump(OpCode::JUMP);
    patchJump(currentFunction->instructions.size() - 1, loopStart);
    
    patchJump(exitJump);
    
    for (size_t continueJump : loopStack[loopIdx].continueJumps) {
        patchJump(continueJump);
    }
    
    for (size_t breakJump : loopStack[loopIdx].breakJumps) {
        patchJump(breakJump);
    }
    
    loopStack.pop_back();
}

// Compile a traditional for loop.
// Handles optional initialization, condition, and iterator expressions.
// Sets up a new loop context, manages break/continue jumps, and scopes properly.
void Compiler::compileFor(ForStmt& stmt) {
    loopStack.push_back(LoopContext());
    size_t loopIdx = loopStack.size() - 1;
    
    if (stmt.init) {
        compileStatement(*stmt.init);
    }
    
    size_t loopStart = currentFunction->instructions.size();
    
    if (stmt.condition) {
        compileExpression(*stmt.condition);
    } else {
        emit(OpCode::PUSH_TRUE);
    }
    
    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    
    // For loops create their own scope for loop variables
    compileBlock(stmt.body, true);
    
    if (stmt.iterator) {
        compileExpression(*stmt.iterator);
        emit(OpCode::POP); 
    }
    
    emitJump(OpCode::JUMP);
    patchJump(currentFunction->instructions.size() - 1, loopStart);
    
    patchJump(exitJump);
    
    for (size_t continueJump : loopStack[loopIdx].continueJumps) {
        patchJump(continueJump);
    }
    
    for (size_t breakJump : loopStack[loopIdx].breakJumps) {
        patchJump(breakJump);
    }
    
    loopStack.pop_back();
}

// For-in loop: for (item in collection)
// 1. Getting the collection and storing it
// 2. Getting the collection length
// 3. Using index-based iteration
void Compiler::compileForIn(ForInStmt& stmt) {
    loopStack.push_back(LoopContext());
    size_t loopIdx = loopStack.size() - 1;
    
    // For-in loop variables are inferred as Unknown (dynamic)
    if (localVariables.find(stmt.varName) == localVariables.end()) {
        localVariables[stmt.varName] = {localCount++, DeclaredType::Unknown};
        currentFunction->locals.push_back(stmt.varName);
    }
    
    compileExpression(*stmt.collection);
    int collIdx = localCount++;
    emitInt(OpCode::STORE_LOCAL, collIdx);
    
    emitInt(OpCode::BUILTIN, 0);
    int lenIdx = localCount++;
    emitInt(OpCode::STORE_LOCAL, lenIdx);
    
    int idxIdx = localCount++;
    emitInt(OpCode::PUSH_INT, 0);
    emitInt(OpCode::STORE_LOCAL, idxIdx);
    
    size_t loopStart = currentFunction->instructions.size();
    
    emitInt(OpCode::LOAD_LOCAL, idxIdx);
    emitInt(OpCode::LOAD_LOCAL, lenIdx);
    emit(OpCode::LT);
    
    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    
    emitInt(OpCode::LOAD_LOCAL, collIdx);
    
    emitInt(OpCode::LOAD_LOCAL, idxIdx);
    emit(OpCode::INDEX_GET);
    emitInt(OpCode::STORE_LOCAL, localVariables[stmt.varName].slot);
    
    compileBlock(stmt.body, true);
    
    emitInt(OpCode::LOAD_LOCAL, idxIdx);
    emitInt(OpCode::PUSH_INT, 1);
    emit(OpCode::ADD);
    emitInt(OpCode::STORE_LOCAL, idxIdx);

    emitJump(OpCode::JUMP);
    patchJump(currentFunction->instructions.size() - 1, loopStart);
    
    patchJump(exitJump);
    
    for (size_t continueJump : loopStack[loopIdx].continueJumps) {
        patchJump(continueJump);
    }
    for (size_t breakJump : loopStack[loopIdx].breakJumps) {
        patchJump(breakJump);
    }
    
    loopStack.pop_back();
}

// Compile a 'break' statement.
// Emits a jump to exit the current loop and records it in the loop context.
void Compiler::compileBreak(BreakStmt& stmt) {
    if (!loopStack.empty()) {
        size_t jumpPos = emitJump(OpCode::JUMP);
        loopStack.back().breakJumps.push_back(jumpPos);
    }
}

// Compile a 'continue' statement.
// Emits a jump to the start of the current loop's next iteration and records it.
void Compiler::compileContinue(ContinueStmt& stmt) {
    if (!loopStack.empty()) {
        size_t jumpPos = emitJump(OpCode::JUMP);
        loopStack.back().continueJumps.push_back(jumpPos);
    }
}

// Compile a 'return' statement.
// Evaluates the optional return expression and emits a RETURN_VALUE instruction.
// Marks the current function as having an explicit return.
void Compiler::compileReturn(ReturnStmt& stmt) {
    if (stmt.expr) {
        compileExpression(*stmt.expr);
    } else {
        emit(OpCode::PUSH_NIL);
    }
    emit(OpCode::RETURN_VALUE);
    

    currentFunctionHasReturn = true;
}

// Compile a 'global' declaration.
// Marks the listed variable names as global, so future loads/stores use global ops
void Compiler::compileGlobal(GlobalStmt& stmt) {
    for (const auto& name : stmt.names) {
        globalVariables.insert(name);
        declaredGlobals.insert(name);
    }
}

// Compile an import declaration.
// Resolves the import path, checks for circular imports, and compiles the imported file.
// Handles:
//   - Regular imports: import "path" - creates a namespace with all exported symbols
//   - Selective imports: import { symbol } from "path" - imports specific symbols
//   - Selective imports with alias: import { symbol as alias } from "path"
void Compiler::compileImport(ImportDecl& decl) {
    std::string importPath = decl.path;
    
    std::optional<std::string> resolvedPath = resolveImportPath(importPath, currentFileDir);
    
    if (!resolvedPath) {
        throw ImportError(currentLine,currentCol,importPath);
    }
    
    std::string fullPath = *resolvedPath;
    
    std::filesystem::path normalizedPath = std::filesystem::absolute(fullPath);
    std::string normalizedPathStr = normalizedPath.string();
    
    if (importedFiles.find(normalizedPathStr) != importedFiles.end()) {
        return;
    }
    
    importedFiles.insert(normalizedPathStr);
    
    std::filesystem::path pathObj(importPath);
    std::string moduleName = pathObj.filename().string();
    
    std::unordered_set<std::string> exportedSymbols;
    
    exportedSymbols = compileImportFile(fullPath, moduleName);
    
    if (decl.isSelective) {
        // Selective import: import { symbol } from "module"
        // Check for collision with existing global symbols
        for (const auto& symbol : decl.symbols) {
            std::string targetName = symbol.alias.empty() ? symbol.originalName : symbol.alias;
            
            if (exportedSymbols.find(symbol.originalName) == exportedSymbols.end()) {
                throw ImportError(currentLine, currentCol, 
                    "Symbol '" + symbol.originalName + "' not found in module '" + moduleName + "'");
            }

            if (globalSymbols.find(targetName) != globalSymbols.end() ||
                currentFileFunctions.find(targetName) != currentFileFunctions.end()) {
                throw ImportError(currentLine, currentCol, 
                    "Symbol '" + targetName + "' is already defined. "
                    "Use 'import { " + symbol.originalName + " as <alias> } from \"" + moduleName + "\"' to import with a different name.");
            }
            
            // Add to global symbols and track the selective import
            globalSymbols.insert(targetName);
            declaredGlobals.insert(targetName);
            selectiveImports[targetName] = moduleName + "." + symbol.originalName;
        }
    } else {
        importedNamespaces[moduleName] = exportedSymbols;
    }
}

// Resolve the full file path for an import.
// Checks standard library directory first, then local/current file directory.
// Supports extensions .nova and .nv
// Uses std::filesystem::path::preferred_separator for path handling
std::optional<std::string> Compiler::resolveImportPath(const std::string& path, const std::string& currentFileDir) {
    std::vector<std::string> extensions = {".nv", ".nova"};
    
    std::string sep(1, std::filesystem::path::preferred_separator);
    
    for (const auto& ext : extensions) {
        std::string fullPath = standardLibDir + sep + path + ext;
        std::ifstream test(fullPath.c_str());
        if (test.good()) {
            test.close();
            return fullPath;
        }
    }
    
    for (const auto& ext : extensions) {
        std::string localPath;
        if (!currentFileDir.empty()) {
            localPath = currentFileDir + sep + path + ext;
        } else {
            localPath = path + ext;
        }
        std::ifstream test(localPath.c_str());
        if (test.good()) {
            test.close();
            return localPath;
        }
    }
    
    return std::nullopt;
}

// Compile an imported file.
// Reads the file, lexes and parses its source, checks for parse errors,
// and compiles all top-level declarations (functions, structs, variables, nested imports).
// Saves and restores the current file directory to handle relative imports correctly.
// Returns a set of exported symbol names (functions, structs, globals).
std::unordered_set<std::string> Compiler::compileImportFile(const std::string& filepath, const std::string& moduleName) {
    std::unordered_set<std::string> exportedSymbols;
    
    std::ifstream file(filepath.c_str());
    if (!file.good()) {
        throw std::runtime_error("Cannot open import file: " + filepath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();
    
    // Use filesystem for path handling
    std::filesystem::path fsPath(filepath);
    std::string importDir = fsPath.parent_path().string();
    if (importDir.empty()) {
        importDir = ".";
    }
    
    std::string savedFileDir = currentFileDir;
    std::unordered_set<std::string> savedFileFunctions = currentFileFunctions;
    currentFileFunctions.clear(); 
    currentFileDir = importDir;
    
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    tokens.push_back({TokenType::EndOfFile, "", 1, 0});
    
    Parser parser(tokens);
    auto programAst = parser.parseProgram();
    
    if (parser.hasErrors()) {
        std::cerr << "Parse errors in imported file '" << filepath << "':\n";
        for (const auto& err : parser.getErrors()) {
            std::cerr << "  Line " << err.line << ", Col " << err.column << ": " << err.message << "\n";
        }
        throw std::runtime_error("Failed to parse imported file: " + filepath);
    }
    
    Program* importedProgram = dynamic_cast<Program*>(programAst.get());
    if (!importedProgram) {
        throw std::runtime_error("Invalid imported program: " + filepath);
    }
    
    // First pass: collect all exported symbols (functions, structs, globals)
    for (auto& decl : importedProgram->decls) {
        if (auto* funcDecl = dynamic_cast<FuncDecl*>(decl.get())) {
            exportedSymbols.insert(funcDecl->name);
        } else if (auto* structDecl = dynamic_cast<StructDecl*>(decl.get())) {
            exportedSymbols.insert(structDecl->name);
        } else if (auto* varDecl = dynamic_cast<VarDecl*>(decl.get())) {
            exportedSymbols.insert(varDecl->name);
        }
    }
    
    // Check if this is a namespace import (not selective)
    bool isNamespaceImport = importedNamespaces.find(moduleName) != importedNamespaces.end();
    
    for (auto& decl : importedProgram->decls) {
        if (auto* funcDecl = dynamic_cast<FuncDecl*>(decl.get())) {
            compileFuncDecl(*funcDecl);
        } else if (auto* structDecl = dynamic_cast<StructDecl*>(decl.get())) {
            compileStructDecl(*structDecl);
        } else if (auto* varDecl = dynamic_cast<VarDecl*>(decl.get())) {
            compileVarDecl(*varDecl);
        } else if (auto* importSubDecl = dynamic_cast<ImportDecl*>(decl.get())) {
            compileImport(*importSubDecl);
        }
    }
    
    currentFileDir = savedFileDir;
    currentFileFunctions = savedFileFunctions;  // Restore
    
    return exportedSymbols;
}

// Compile an expression statement.
// Evaluates the expression and pops its result from the stack since the value is not used.
void Compiler::compileExprStmt(ExprStmt& stmt) {
    compileExpression(*stmt.expr);
    emit(OpCode::POP);
}

// Compile a declaration statement.
// Dispatches to the appropriate compiler function based on the declaration type (variable, funtion or struct)
void Compiler::compileDeclStmt(DeclStmt& stmt) {
    if (auto* varDecl = dynamic_cast<VarDecl*>(stmt.decl.get())) {
        compileVarDecl(*varDecl);
    } else if (auto* funcDecl = dynamic_cast<FuncDecl*>(stmt.decl.get())) {
        compileFuncDecl(*funcDecl);
    } else if (auto* structDecl = dynamic_cast<StructDecl*>(stmt.decl.get())) {
        compileStructDecl(*structDecl);
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

void Compiler::emit(OpCode op) {
    Instruction instr(op);
    instr.line = currentLine;
    instr.column = currentCol;
    currentFunction->instructions.push_back(instr);
}

void Compiler::emitInt(OpCode op, int64_t operand) {
    Instruction instr(op, operand);
    instr.line = currentLine;
    instr.column = currentCol;
    currentFunction->instructions.push_back(instr);
}

void Compiler::emitDouble(OpCode op, double operand) {
    Instruction instr(op, operand);
    instr.line = currentLine;
    instr.column = currentCol;
    currentFunction->instructions.push_back(instr);
}

void Compiler::emitString(OpCode op, const std::string& operand) {
    Instruction instr(op, operand);
    instr.line = currentLine;
    instr.column = currentCol;
    currentFunction->instructions.push_back(instr);
}

size_t Compiler::emitJump(OpCode op) {
    Instruction instr(op, static_cast<int64_t>(0));
    instr.line = currentLine;
    instr.column = currentCol;
    currentFunction->instructions.push_back(instr);
    return currentFunction->instructions.size() - 1;
}

size_t Compiler::emitJumpWithOffset(OpCode op, int64_t offset) {
    Instruction instr(op, offset);
    instr.line = currentLine;
    instr.column = currentCol;
    currentFunction->instructions.push_back(instr);
    return currentFunction->instructions.size() - 1;
}

// Jump to current position (after all instructions are emitted)
// No -1 because the ip hasn't been incremented yet when we jump
void Compiler::patchJump(size_t jumpPos) {
    int64_t offset = static_cast<int64_t>(currentFunction->instructions.size() - jumpPos);
    currentFunction->instructions[jumpPos].operand = offset;
}

// Jump offset: target - current_position
// When JUMP executes, ip is at jumpPos, so we need offset = targetPos - jumpPos
void Compiler::patchJump(size_t jumpPos, size_t targetPos) {
    int64_t offset = static_cast<int64_t>(targetPos) - static_cast<int64_t>(jumpPos);
    currentFunction->instructions[jumpPos].operand = offset;
}

int Compiler::addString(const std::string& s) {
    return program->addString(s);
}

BytecodeFunction* Compiler::enterFunction(const std::string& name) {
    // Save current function index before adding new one
    int prevIndex = currentFunction ? (int)(currentFunction - &program->functions[0]) : -1;
    
    // Save current local variables to restore when exiting
    savedLocalVariables.push_back(localVariables);
    savedLocalCount.push_back(localCount);
    
    savedGlobalVariables.push_back(globalVariables);
    
    program->functions.push_back(BytecodeFunction());
    currentFunction = &program->functions.back();
    currentFunction->name = name;
    localVariables.clear();
    localCount = 0;
    globalVariables.clear();
    
    prevFunctionIndex = prevIndex;
    
    // Increment function depth only for user-defined functions (not the initial main or _start)
    if (name != "main" && name != "_start") {
        functionDepth++;
    }
    
    return currentFunction;
}

void Compiler::exitFunction() {
    // Restore previous function
    if (prevFunctionIndex >= 0 && prevFunctionIndex < (int)program->functions.size()) {
        currentFunction = &program->functions[prevFunctionIndex];
    } else {
        currentFunction = nullptr;
    }
    
    // Decrement function depth
    if (functionDepth > 0) {
        functionDepth--;
    }
    
    // Restore local variables from the outer scope
    if (!savedLocalVariables.empty()) {
        localVariables = savedLocalVariables.back();
        localCount = savedLocalCount.back();
        savedLocalVariables.pop_back();
        savedLocalCount.pop_back();
    }
    
    // Restore global variables from the outer scope
    if (!savedGlobalVariables.empty()) {
        globalVariables = savedGlobalVariables.back();
        savedGlobalVariables.pop_back();
    }
}

// Save current scope to the stack
// Note: We don't reset localCount - each variable gets a unique slot
// to avoid slot collision between scopes
void Compiler::enterScope() {
    scopeStack.push_back(localVariables);
    scopeLocalCount.push_back(localCount);
    localVariables.clear();
}

// Restore previous scope from the stack
void Compiler::exitScope() {
    if (!scopeStack.empty()) {
        localVariables = scopeStack.back();
        localCount = scopeLocalCount.back();
        scopeStack.pop_back();
        scopeLocalCount.pop_back();
    }
}

// Find variable in any scope level (current or outer scopes)
// Returns {found, slotIndex}
std::pair<bool, int> Compiler::findVariableInAnyScope(const std::string& name) {
    auto it = localVariables.find(name);
    if (it != localVariables.end()) {
        return {true, it->second.slot};
    }
    
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            return {true, varIt->second.slot};
        }
    }
    
    return {false, -1};
}

// Find variable info in any scope level (current or outer scopes)
// Returns {found, VariableInfo}
std::pair<bool, Compiler::VariableInfo> Compiler::findVariableInfoInAnyScope(const std::string& name) {
    auto it = localVariables.find(name);
    if (it != localVariables.end()) {
        return {true, it->second};
    }
    
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            return {true, varIt->second};
        }
    }
    
    // Also check global variables
    auto globalIt = globalVariablesInfo.find(name);
    if (globalIt != globalVariablesInfo.end()) {
        return {true, globalIt->second};
    }
    
    return {false, {-1, DeclaredType::Unknown, false}};
}

// Resolve a declared type from AST type node
Compiler::DeclaredType Compiler::resolveDeclaredType(const TypePtr& type) {
    if (!type) {
        return DeclaredType::Auto; 
    }
    
    if (auto* simple = dynamic_cast<SimpleType*>(type.get())) {
        std::string name = simple->name;
        
        if (name == "int") return DeclaredType::Int;
        if (name == "float") return DeclaredType::Float;
        if (name == "string" || name == "str") return DeclaredType::String;
        if (name == "bool") return DeclaredType::Bool;
        if (name == "list") return DeclaredType::List;
        if (name == "auto") return DeclaredType::Auto;
        if (name == "any") return DeclaredType::Any;
        
        // Unknown identifiers are treated as Struct
        return DeclaredType::Struct;
    }
    
    if (auto* listType = dynamic_cast<ListType*>(type.get())) {
        return DeclaredType::List;
    }
    
    return DeclaredType::Unknown;
}

// Infer type from an expression (for 'auto' type inference)
Compiler::DeclaredType Compiler::inferTypeFromExpression(Expression& expr) {
    if (auto* lit = dynamic_cast<LiteralExpr*>(&expr)) {
        switch (lit->kind) {
            case LiteralExpr::LitKind::Integer:
                return DeclaredType::Int;
            case LiteralExpr::LitKind::Float:
                return DeclaredType::Float;
            case LiteralExpr::LitKind::String:
                return DeclaredType::String;
            case LiteralExpr::LitKind::FString:
                return DeclaredType::String;  // F-strings evaluate to strings
            case LiteralExpr::LitKind::Boolean:
                return DeclaredType::Bool;
            case LiteralExpr::LitKind::None:
                return DeclaredType::Unknown;  // None can be anything
        }
    }
    
    if (auto* list = dynamic_cast<ListExpr*>(&expr)) {
        return DeclaredType::List;
    }
    
    if (auto* structExpr = dynamic_cast<StructExpr*>(&expr)) {
        return DeclaredType::Struct;
    }
    
    if (auto* ident = dynamic_cast<IdentifierExpr*>(&expr)) {
        auto [found, varInfo] = findVariableInfoInAnyScope(ident->name);
        if (found) {
            return varInfo.type;
        }
    }
    
    // For function calls, we can't easily determine return type at compile time
    // Default to Unknown (dynamic) later we will implement return type checking
    return DeclaredType::Unknown;
}

// Check type compatibility
// Returns true if actual type can be assigned to declared type
bool Compiler::checkTypeCompatibility(DeclaredType declared, DeclaredType actual) {
    if (declared == DeclaredType::Any) {
        return true;
    }
    
    if (declared == DeclaredType::Auto) {
        return true;
    }
    
    if (declared == DeclaredType::Unknown) {
        return true;
    }
    
    // If actual type is unknown (e.g., complex expression), allow it
    // since we can't determine the type at compile time
    if (actual == DeclaredType::Unknown) {
        return true;
    }
    
    if (declared == actual) {
        return true;
    }
    
    if (declared == DeclaredType::Float && actual == DeclaredType::Int) {
        return true;
    }
    
    return false;
}

}
