#include "parser.h"
#include "ast.h"
#include <iostream>

namespace nova {

    bool isRepl = false;

    // Constructor
    Parser::Parser(const std::vector<Token>& tokens)
        : tokens(tokens) {}

    // Token utilities
    const Token& Parser::peek() const {
        if (current >= tokens.size()) return tokens.back();
        return tokens[current];
    }

    const Token& Parser::previous() const {
        return tokens[current - 1];
    }

    bool Parser::isAtEnd() const {
        if (current >= tokens.size()) return true;
        // Also consider EOF token as end
        if (current > 0 && current == tokens.size() - 1 && peek().type == TokenType::EndOfFile) {
            return true;
        }
        return false;
    }

    const Token& Parser::advance() {
        if (!isAtEnd()) current++;
        return previous();
    }

    bool Parser::check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    bool Parser::match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    // Error handling
    void Parser::error(const std::string& message) {
        errorAt(peek(), message);
    }

    void Parser::errorAt(const Token& token, const std::string& message) {
        errors.emplace_back(token.line, token.column, message);
        std::cerr << "Parsing error at line " << token.line << ", column " << token.column << ": " << message << "\n";
    }

    // Parsing functions
    ASTNodePtr Parser::parseProgram() {
        auto program = std::make_unique<Program>();
        
        // Set location from first token if available
        if (!tokens.empty()) {
            program->loc.line = tokens[0].line;
            program->loc.column = tokens[0].column;
        }
        
        if (isAtEnd()) return nullptr; // empty input

        while (!isAtEnd()) {
            if (check(TokenType::EndOfFile)) // safety check to avoid infinite loop if EOF is not handled properly
                break;

            DeclPtr decl = parseDeclaration();
            if (decl) {
                program->decls.push_back(std::move(decl));
            } else {
                // Parse a statement at top-level
                StmtPtr stmt = parseStatement();
                if (stmt) {
                    // Wrap it properly in a DeclStmt
                    program->decls.push_back(std::make_unique<StmtDecl>(std::move(stmt)));
                } else {
                    error("Unexpected token at top-level: " + peek().value);
                    exit(0); // exit on error for now
                }
            }
        }

        return program;
    }

    // Parse a variable declaration.
    // Expects a type (built-in or user-defined), a variable name, and an optional initializer.
    // Uses backtracking to distinguish declarations from expressions.
    // Returns nullptr if parsing fails.
    DeclPtr Parser::parseVarDecl(bool isConst = false) {
        auto var = std::make_unique<VarDecl>();
        var->isConst = isConst;

        size_t savedPos = current;

        TypePtr type = nullptr;
        if (check(TokenType::Identifier)) {
            std::string typeName = peek().value;
            
            if (typeName == "int" || typeName == "string" || typeName == "float" || typeName == "bool" || typeName == "auto" || typeName == "list" || typeName == "any") {
                advance();
                type = std::make_unique<SimpleType>(typeName);
                type->loc.line = previous().line;
                type->loc.column = previous().column;
            } else {

                size_t savedPos2 = current;
                advance();  
                
                // Check if next token is an identifier (variable name) or '=' or another type marker
                // If it's '(', this is likely a function call, not a variable declaration
                if (check(TokenType::Identifier) || check(TokenType::Assign) || check(TokenType::Semicolon) || check(TokenType::Separator)) {
                    type = std::make_unique<SimpleType>(typeName);
                    type->loc.line = previous().line;
                    type->loc.column = previous().column;
                } else {
                    current = savedPos2;
                    return nullptr;
                }
            }
        }

        if (!type) {
            return nullptr;
        }

        // Variable name must come after type
        if (!match(TokenType::Identifier)) {
            current = savedPos;
            return nullptr;
        }

        var->name = previous().value;
        var->type = std::move(type);
        var->loc.line = previous().line;
        var->loc.column = previous().column;

        if (match(TokenType::Assign) || match(TokenType::PlusEqual) || match(TokenType::MinusEqual) || match(TokenType::StarEqual) || match(TokenType::SlashEqual)) {
            var->init = parseExpression();
        }

        return var;
    }

    // Parse a function declaration.
    // Handles function name, parameters (typed or untyped), optional return type,
    // and body block. Supports multiple parameter type syntaxes.
    // Returns nullptr if the structure does not match a valid function declaration.
    // isAsync indicates if this is an async function.
    DeclPtr Parser::parseFuncDecl(bool isAsync) 
    { 
        auto func = std::make_unique<FuncDecl>();
        func->isAsync = isAsync; 
        if (!match(TokenType::Identifier)) return nullptr; 
        func->loc.line = previous().line;
        func->loc.column = previous().column;
        func->name = previous().value; 
        if (!match(TokenType::ParenOpen)) return nullptr; 
        while (!check(TokenType::ParenClose) && !isAtEnd()) 
        { 
            Param param; 
            
            if (match(TokenType::Identifier)) {
                std::string firstToken = previous().value;
                
                if (match(TokenType::Colon)) {
                    param.name = firstToken;
                    param.type = parseType(); 
                } else if (firstToken == "int" || firstToken == "string" || 
                           firstToken == "float" || firstToken == "bool" || 
                           firstToken == "auto" || firstToken == "list") {

                    param.type = std::make_unique<SimpleType>(firstToken);
                    param.type->loc.line = previous().line;
                    param.type->loc.column = previous().column;
                    
                    if (!match(TokenType::Identifier)) {

                        break;
                    }
                    param.name = previous().value;
                } else {
                    param.name = firstToken;
                }
            } else {
                break;
            }
            
            func->params.push_back(std::move(param)); 
            match(TokenType::Comma);
        } 
        if (!match(TokenType::ParenClose)) return nullptr; 
        if (match(TokenType::Arrow)) 
        { 
            func->returnType = parseType(); 
        } 
        func->body = parseBlock(); return func; 
    }

    // Parse a struct declaration.
    // Expects a struct name followed by a block of typed member declarations,
    // each with an optional initializer. Currently supports only built-in member types.
    // Returns nullptr if required tokens or structure are invalid.
    DeclPtr Parser::parseStructDecl() {
        auto strct = std::make_unique<StructDecl>();
        
        if (!match(TokenType::Identifier)) {
            error("Expected struct name");
            return nullptr;
        }
        strct->loc.line = previous().line;
        strct->loc.column = previous().column;
        strct->name = previous().value;
        
        // Track this struct name for variable declaration recognition
        structNames.insert(strct->name);
        
        if (!match(TokenType::BraceOpen)) {
            error("Expected '{' after struct name");
            return nullptr;
        }

        while (!check(TokenType::BraceClose) && !isAtEnd()) {
            if (!check(TokenType::Identifier)) {
                error("Expected type for struct member");
                break;
            }
            
            std::string typeName = peek().value;
            if (typeName != "int" && typeName != "string" && typeName != "float" && 
                typeName != "bool" && typeName != "auto" && typeName != "list") {
                error("Unknown type for struct member: " + typeName);
                break;
            }
            
            advance();
            auto type = std::make_unique<SimpleType>(typeName);
            type->loc.line = previous().line;
            type->loc.column = previous().column;
            
            if (!match(TokenType::Identifier)) {
                error("Expected member name");
                break;
            }
            std::string memberName = previous().value;
            
            VarDecl member;
            member.name = memberName;
            member.type = std::move(type);
            member.loc.line = previous().line;
            member.loc.column = previous().column;
            
            if (match(TokenType::Assign)) {
                member.init = parseExpression();
            }
            
            strct->members.push_back(std::move(member));
        }
        
        if (!match(TokenType::BraceClose)) {
            error("Expected '}' after struct members");
            return nullptr;
        }
        
        return strct;
    }

    // Parse an import declaration
    // Handles:
    //   import "path"                     - namespace import
    //   import { symbol } from "path"     - selective import
    //   import { symbol as alias } from "path" - selective import with alias
    DeclPtr Parser::parseImportDecl() {
        auto import = std::make_unique<ImportDecl>();
        import->loc.line = peek().line;
        import->loc.column = peek().column;
        
        // Check if this is a selective import (starts with {)
        if (check(TokenType::BraceOpen)) {
            import->isSelective = true;
            advance(); // consume {
            
            // Parse symbol list
            while (!check(TokenType::BraceClose) && !isAtEnd()) {
                // Skip commas
                if (check(TokenType::Comma)) {
                    advance();
                    continue;
                }
                
                // Expect identifier for symbol name
                if (!check(TokenType::Identifier)) {
                    error("Expected symbol name in import");
                    return nullptr;
                }
                
                ImportSymbol symbol;
                symbol.originalName = peek().value;
                advance();
                
                // Check for 'as' alias
                if (check(TokenType::Keyword) && peek().value == "as") {
                    advance(); // consume 'as'
                    if (!check(TokenType::Identifier)) {
                        error("Expected alias name after 'as'");
                        return nullptr;
                    }
                    symbol.alias = peek().value;
                    advance();
                }
                
                import->symbols.push_back(symbol);
                
                // Skip commas
                if (check(TokenType::Comma)) {
                    advance();
                }
            }
            
            // Expect closing brace
            if (!check(TokenType::BraceClose)) {
                error("Expected '}' after import symbols");
                return nullptr;
            }
            advance(); // consume }
            
            // Expect 'from' keyword
            if (!check(TokenType::Keyword) || peek().value != "from") {
                error("Expected 'from' after import symbols");
                return nullptr;
            }
            advance(); // consume 'from'
        }
        
        // Parse the module path (string literal)
        if (!check(TokenType::String)) {
            error("Expected string literal for import path");
            return nullptr;
        }
        
        std::string path = peek().value;
        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
        
        import->path = path;
        advance();
        
        return import;
    }

    // Parse a top-level declaration.
    // Dispatches to specific parsers based on keywords such as func, const, struct, and import.
    // Falls back to other declaration parsing if no known keyword is matched.
    DeclPtr Parser::parseDeclaration() {
        if (check(TokenType::Keyword)) {
            std::string kw = peek().value;
            
            if (kw == "async") {
                advance(); 
                if (check(TokenType::Keyword) && peek().value == "func") {
                    advance(); 
                    return parseFuncDecl(true); // isAsync = true
                } else {
                    error("Expected 'func' after 'async'");
                    return nullptr;
                }
            }
            
            if (kw == "func") {
                advance();
                return parseFuncDecl(); // default isAsync = false
            }
            if (kw == "const") {
                advance(); 
                auto decl = parseVarDecl(true);
                if (decl) return decl;
            }
            if (kw == "struct") {
                advance(); 
                return parseStructDecl();
            }
            if (kw == "import") {
                advance(); 
                return parseImportDecl();
            }
        }
        
        // Also handle if/while/return as statements at top-level
        // NOTE: elif is NOT included here - it can only appear after if or else
        if (check(TokenType::Identifier)) {
            std::string val = peek().value;
            if (val == "if" || val == "while" || val == "return" || val == "else" || val == "true" || val == "false") {
                // Return nullptr so parseProgram knows to call parseStatement instead
                return nullptr;
            }
        }

        
        DeclPtr varDecl = parseVarDecl();
        if (varDecl) return varDecl;
        
        //error("Expected declaration"); we allow expressive statements at top-level for REPL, so don't error here yet
        return nullptr; 
    }

    // Parse a statement.
    // Dispatches based on keywords and structure to handle control flow,
    // loops (including for-in), jump statements, and declarations.
    // Supports both block and single-statement bodies.
    // Defaults to an expression statement if no specific form is matched.
    StmtPtr Parser::parseStatement() {
        bool isKeyword = check(TokenType::Keyword);
        std::string kwValue = isKeyword ? peek().value : "";
        
        bool isPotentialKeyword = check(TokenType::Identifier) && 
            (peek().value == "if" || peek().value == "return" || 
             peek().value == "while" || peek().value == "else" || peek().value == "for" ||
             peek().value == "break" || peek().value == "continue" || peek().value == "in" ||
             peek().value == "global");
        
        if (isKeyword || isPotentialKeyword) {
            std::string kw = peek().value;
            
            if (kw == "return") {
                advance(); 
                auto ret = std::make_unique<ReturnStmt>();
                ret->loc.line = previous().line;
                ret->loc.column = previous().column;
                if (!check(TokenType::Semicolon) && !check(TokenType::BraceClose)) {
                    ret->expr = parseExpression();
                }
                return ret;
            }

            if (kw == "global") {
                advance();
                auto globalStmt = std::make_unique<GlobalStmt>();
                globalStmt->loc.line = previous().line;
                globalStmt->loc.column = previous().column;
                
                while (check(TokenType::Identifier)) {
                    globalStmt->names.push_back(peek().value);
                    advance(); 
                    if (!match(TokenType::Comma)) {
                        break;
                    }
                }
                return globalStmt;
            }

            if (kw == "if" || kw == "elif") {
                bool isElif = (kw == "elif");
                advance(); 
                SourceLocation ifLoc;
                ifLoc.line = previous().line;
                ifLoc.column = previous().column;
                
                if (!match(TokenType::ParenOpen)) {
                    error("Expected '(' after 'if' or 'elif'");
                }
                
                ExprPtr cond = parseExpression();
                
                if (!match(TokenType::ParenClose)) {
                    error("Expected ')' after condition");
                }
                
                auto ifStmt = std::make_unique<IfStmt>();
                ifStmt->loc = ifLoc;
                ifStmt->cond = std::move(cond);
                
                if (match(TokenType::BraceOpen)) {
                    // Block statement
                    while (!check(TokenType::BraceClose) && !isAtEnd()) {
                        auto stmt = parseStatement();
                        if (stmt) ifStmt->thenBody.statements.push_back(std::move(stmt));
                    }
                    match(TokenType::BraceClose);
                } else {
                    // Single statement
                    auto stmt = parseStatement();
                    if (stmt) ifStmt->thenBody.statements.push_back(std::move(stmt));
                }
                
                // Parse else clause, must specifically check for "else" keyword
                // NOT any keyword, because "if" is also a keyword and would cause issues
                // with consecutive if statements without else
                if (check(TokenType::Keyword) && peek().value == "else") {
                    advance(); 
                    ifStmt->hasElse = true;
                    
                    if (check(TokenType::Keyword) && peek().value == "elif") {
                        auto stmt = parseStatement();
                        if (stmt) ifStmt->elseBody.statements.push_back(std::move(stmt));
                    } else if (match(TokenType::BraceOpen)) {
                        while (!check(TokenType::BraceClose) && !isAtEnd()) {
                            auto stmt = parseStatement();
                            if (stmt) ifStmt->elseBody.statements.push_back(std::move(stmt));
                        }
                        match(TokenType::BraceClose);
                    } else {
                        auto stmt = parseStatement();
                        if (stmt) ifStmt->elseBody.statements.push_back(std::move(stmt));
                    }
                }
                
                return ifStmt;
            }

            // while statement
            if (kw == "while") {
                advance(); 
                SourceLocation whileLoc;
                whileLoc.line = previous().line;
                whileLoc.column = previous().column;
                
                if (!match(TokenType::ParenOpen)) {
                    error("Expected '(' after 'while'");
                }
                
                ExprPtr cond = parseExpression();
                
                if (!match(TokenType::ParenClose)) {
                    error("Expected ')' after condition");
                }
                
                auto whileStmt = std::make_unique<WhileStmt>();
                whileStmt->loc = whileLoc;
                whileStmt->cond = std::move(cond);
                
                if (match(TokenType::BraceOpen)) {
                    while (!check(TokenType::BraceClose) && !isAtEnd()) {
                        auto stmt = parseStatement();
                        if (stmt) whileStmt->body.statements.push_back(std::move(stmt));
                    }
                    match(TokenType::BraceClose);
                } else {
                    auto stmt = parseStatement();
                    if (stmt) whileStmt->body.statements.push_back(std::move(stmt));
                }
                
                return whileStmt;
            }

            // for statement
            if (kw == "for") {
                advance(); 
                SourceLocation forLoc;
                forLoc.line = previous().line;
                forLoc.column = previous().column;

                if (!match(TokenType::ParenOpen)) {
                    error("Expected '(' after 'for'");
                }

                if (check(TokenType::Identifier)) {
                    std::string varName = peek().value;
                    size_t savedPos = current;
                    advance(); 
                    if (check(TokenType::Keyword) && peek().value == "in") {
                        advance(); 
                        ExprPtr collection = parseExpression();
                        
                        if (!match(TokenType::ParenClose)) {
                            error("Expected ')' after for-in loop");
                        }
                        
                        auto forInStmt = std::make_unique<ForInStmt>();
                        forInStmt->loc = forLoc;
                        forInStmt->varName = varName;
                        forInStmt->collection = std::move(collection);
                        
                        if (match(TokenType::BraceOpen)) {
                            while (!check(TokenType::BraceClose) && !isAtEnd()) {
                                auto stmt = parseStatement();
                                if (stmt) forInStmt->body.statements.push_back(std::move(stmt));
                            }
                            match(TokenType::BraceClose);
                        } else {
                            auto stmt = parseStatement();
                            if (stmt) forInStmt->body.statements.push_back(std::move(stmt));
                        }
                        
                        return forInStmt;
                    } else {
                        current = savedPos;
                    }
                }

                auto forStmt = std::make_unique<ForStmt>();
                forStmt->loc = forLoc;

                // Parse initializer - could be declaration, expression, or empty
                if (!check(TokenType::Semicolon)) {
                    if (check(TokenType::Identifier)) {
                        std::string nextType = peek().value;
                        if (nextType == "int" || nextType == "string" || nextType == "float" || 
                            nextType == "bool" || nextType == "auto" || nextType == "list" || isStructType(nextType)) {
                            // It's a variable declaration
                            auto decl = parseVarDecl();
                            if (decl) {
                                auto declStmt = std::make_unique<DeclStmt>(std::move(decl));
                                forStmt->init = std::move(declStmt);
                            }
                        } else {
                            // It's an expression
                            forStmt->init = parseExpressionStatement();
                        }
                    } else {
                        // It's an expression
                        forStmt->init = parseExpressionStatement();
                    }
                }
                // Consume the first semicolon
                match(TokenType::Semicolon);

                if (!check(TokenType::Semicolon)) {
                    forStmt->condition = parseExpression();
                }

                // Consume the second semicolon
                match(TokenType::Semicolon);


                if (!check(TokenType::ParenClose)) {
                    forStmt->iterator = parseExpression();
                }

                if (!match(TokenType::ParenClose)) {
                    error("Expected ')' after for loop parts");
                }

                if (match(TokenType::BraceOpen)) {
                    while (!check(TokenType::BraceClose) && !isAtEnd()) {
                        auto stmt = parseStatement();
                        if (stmt) forStmt->body.statements.push_back(std::move(stmt));
                    }
                    match(TokenType::BraceClose);
                } else {
                    auto stmt = parseStatement();
                    if (stmt) forStmt->body.statements.push_back(std::move(stmt));
                }

                return forStmt;
            }
        }

        // break statement
        if (check(TokenType::Keyword) && peek().value == "break") {
            advance(); 
            auto breakStmt = std::make_unique<BreakStmt>();
            breakStmt->loc.line = previous().line;
            breakStmt->loc.column = previous().column;
            return breakStmt;
        }

        // continue statement
        if (check(TokenType::Keyword) && peek().value == "continue") {
            advance(); 
            auto continueStmt = std::make_unique<ContinueStmt>();
            continueStmt->loc.line = previous().line;
            continueStmt->loc.column = previous().column;
            return continueStmt;
        }

        if (check(TokenType::Identifier)) {
            // peek ahead: type + identifier pattern indicates variable declaration, even without 'auto' keyword (for REPL)
            std::string nextType = peek().value;
            if (nextType == "int" || nextType == "string" || nextType == "float" || nextType == "bool" || nextType == "auto" || nextType == "list" || nextType == "any" || isStructType(nextType)) {
                auto decl = parseVarDecl();
                if (decl) {
                    auto declStmt = std::make_unique<DeclStmt>(std::move(decl));
                    declStmt->loc = declStmt->decl->loc;
                    return declStmt;
                }
            }
        }

        return parseExpressionStatement();
    }

    // Parse an expression statement.
    // Parses an expression and wraps it in a statement node.
    // Returns nullptr if expression parsing fails.
    StmtPtr Parser::parseExpressionStatement() {
        auto expr = parseExpression();
        if (!expr) return nullptr;
        auto stmt = std::make_unique<ExprStmt>(std::move(expr));
        stmt->loc = stmt->expr->loc;
        return stmt;
    }

    // Parse a block.
    // Reads statements enclosed in '{' and '}'.
    // Returns an empty block if '{' is not present.
    BlockStmt Parser::parseBlock() {
        BlockStmt block;
        if (check(TokenType::BraceOpen)) {
            block.loc.line = peek().line;
            block.loc.column = peek().column;
        }
        if (!match(TokenType::BraceOpen)) return block;
        while (!check(TokenType::BraceClose) && !isAtEnd()) {
            auto stmt = parseStatement();
            if (stmt) block.statements.push_back(std::move(stmt));
        }
        match(TokenType::BraceClose);
        return block;
    }

    TypePtr Parser::parseType() 
    { 
        if (match(TokenType::Star)) 
        { // pointer type 
            auto ptr = std::make_unique<PointerType>();
            // Set location from the star token
            ptr->loc.line = previous().line;
            ptr->loc.column = previous().column;
            ptr->pointee = parseType(); return ptr; 
        } 
        if (match(TokenType::Identifier)) 
        { 
            std::string typeName = previous().value; 
            // Set location from the type name token
            SourceLocation typeLoc;
            typeLoc.line = previous().line;
            typeLoc.column = previous().column;
            
            bool optional = false; 
            if (!typeName.empty() && typeName.back() == '?') 
            {
                typeName.pop_back(); 
                optional = true; 
            } 
            auto simple = std::make_unique<SimpleType>(typeName, optional); 
            simple->loc = typeLoc;
            
            // check for list: list[T] or list[int][4] 
            if (typeName == "list" && match(TokenType::BracketOpen)) 
            { 
                auto listType = std::make_unique<ListType>(); 
                listType->loc = typeLoc;
                if (!check(TokenType::BracketClose)) 
                { 
                    listType->elementType = parseType(); 
                } 
                match(TokenType::BracketClose); 
                if (match(TokenType::BracketOpen)) 
                { 
                    // optional fixed size 
                    if (check(TokenType::Number)) 
                    { 
                        listType->fixedSize = std::stoi(advance().value); 
                    } match(TokenType::BracketClose); 
                } 
                return listType; 
            } 
            return simple; 
        } 
        if (match(TokenType::ParenOpen)) 
        { 
            // tuple 
            auto tuple = std::make_unique<TupleType>();
            // Set location from opening paren
            tuple->loc.line = previous().line;
            tuple->loc.column = previous().column;
            
            while (!check(TokenType::ParenClose) && !isAtEnd()) 
            { 
                tuple->elements.push_back(parseType()); 
                match(TokenType::Comma); // optional comma 
            } 
            match(TokenType::ParenClose); 
            return tuple; 
        } 
        return nullptr; 
    }

    ExprPtr Parser::parseExpression() {
        return parseAssignment();
    }

    ExprPtr Parser::parseAssignment() {
        auto expr = parseBinary(0);

        if (check(TokenType::Assign) || check(TokenType::PlusEqual) || check(TokenType::MinusEqual) || check(TokenType::StarEqual) || check(TokenType::SlashEqual)) {
            Token opToken = advance(); // consume =, +=, -=
            auto right = parseAssignment(); // recursive RHS
            auto assign = std::make_unique<AssignmentExpr>();
            // Set location from the target expression
            assign->loc = expr->loc;
            assign->target = std::move(expr);
            assign->value = std::move(right);
            assign->op = opToken.value; 
            return assign;
        }

        return expr;
    }

    ExprPtr Parser::parseBinary(int precedence) {
        auto left = parseUnary();
        if (!left) return nullptr; // <-- don’t throw yet, just stop

        while (true) {
            int opPrec = getPrecedence(peek());
            if (opPrec < precedence) break;

            auto opToken = advance();
            auto right = parseBinary(opPrec + 1);
            if (!right) {
                error("Expected expression on the right-hand side of '" + opToken.value + "'");
                exit(0);
            }

            auto bin = std::make_unique<BinaryExpr>();
            // Set location from left operand
            bin->loc = left->loc;
            bin->left = std::move(left);
            bin->right = std::move(right);
            bin->op = opToken.value;
            left = std::move(bin);
        }

        return left;
    }

    ExprPtr Parser::parseUnary() {
        // Handle await expression
        if (check(TokenType::Keyword) && peek().value == "await") {
            auto awaitExpr = std::make_unique<AwaitExpr>();
            awaitExpr->loc.line = peek().line;
            awaitExpr->loc.column = peek().column;
            advance(); 
            
            awaitExpr->expr = parseUnary();
            if (!awaitExpr->expr) {
                error("Expected expression after 'await'");
                return nullptr;
            }
            return awaitExpr;
        }
        
        if (match(TokenType::PlusPlus) || match(TokenType::MinusMinus) || match(TokenType::Minus) || match(TokenType::Bang)) {
            SourceLocation opLoc;
            opLoc.line = previous().line;
            opLoc.column = previous().column;
            
            auto op = previous().value;
            auto operand = parseUnary(); // recursively parse operand

            // Wrap numeric literals too; the interpreter will evaluate the negative
            auto u = std::make_unique<UnaryExpr>();
            u->loc = opLoc;
            u->op = op;
            u->operand = std::move(operand);
            return u;
        }

        ExprPtr expr = parsePrimary();

        if (expr) {
            if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.get())) {
                if (match(TokenType::PlusPlus)) {
                    auto u = std::make_unique<UnaryExpr>();
                    u->loc = ident->loc;
                    u->op = "++";  // Postfix marker
                    u->operand = std::move(expr);
                    return u;
                }
                if (match(TokenType::MinusMinus)) {
                    auto u = std::make_unique<UnaryExpr>();
                    u->loc = ident->loc;
                    u->op = "--";  // Postfix marker
                    u->operand = std::move(expr);
                    return u;
                }
            }
        }
        
        return expr;
    }

    ExprPtr Parser::parsePrimary() {
        if (isAtEnd()) return nullptr;  // <-- important

        if (match(TokenType::Number)) {
            auto& tok = previous();
            auto lit = std::make_unique<LiteralExpr>(
                tok.numberKind == NumberKind::Float ? LiteralExpr::LitKind::Float : LiteralExpr::LitKind::Integer, 
                tok.value
            );
            lit->loc.line = tok.line;
            lit->loc.column = tok.column;
            return lit;
        }
        if (match(TokenType::String)) {
            auto& tok = previous();
            auto lit = std::make_unique<LiteralExpr>(LiteralExpr::LitKind::String, tok.value);
            lit->loc.line = tok.line;
            lit->loc.column = tok.column;
            return lit;
        }
        if (match(TokenType::FString)) {
            auto& tok = previous();
            auto lit = std::make_unique<LiteralExpr>(LiteralExpr::LitKind::FString, tok.value);
            lit->loc.line = tok.line;
            lit->loc.column = tok.column;
            return lit;
        }
        if (match(TokenType::Boolean)) {
            auto& tok = previous();
            auto lit = std::make_unique<LiteralExpr>(LiteralExpr::LitKind::Boolean, tok.value);
            lit->loc.line = tok.line;
            lit->loc.column = tok.column;
            return lit;
        }

        if (check(TokenType::Keyword) && peek().value == "none") {
            advance(); 
            auto& tok = previous();
            auto lit = std::make_unique<LiteralExpr>(LiteralExpr::LitKind::None, "none");
            lit->loc.line = tok.line;
            lit->loc.column = tok.column;
            return lit;
        }

        if (match(TokenType::BracketOpen)) {
            SourceLocation bracketLoc;
            bracketLoc.line = previous().line;
            bracketLoc.column = previous().column;
            
            auto listExpr = std::make_unique<ListExpr>();
            listExpr->loc = bracketLoc;
            
            if (check(TokenType::BracketClose)) {
                match(TokenType::BracketClose);
                
                ExprPtr result = std::move(listExpr);
                while (match(TokenType::BracketOpen)) {
                    auto indexExpr = std::make_unique<IndexExpr>();
                    indexExpr->loc.line = previous().line;
                    indexExpr->loc.column = previous().column;
                    indexExpr->collection = std::move(result);
                    indexExpr->index = parseExpression();
                    match(TokenType::BracketClose);
                    result = std::move(indexExpr);
                }
                
                return result;
            }

            do {
                auto elem = parseExpression();
                if (elem) {
                    listExpr->elements.push_back(std::move(elem));
                }
            } while (match(TokenType::Comma));
            
            match(TokenType::BracketClose);
            
            // Handle index and member access after list literal
            ExprPtr result = std::move(listExpr);
            while (true) {
                if (match(TokenType::Dot)) {
                    if (!check(TokenType::Identifier)) {
                        error("Expected field name after '.'");
                        break;
                    }
                    std::string fieldName = peek().value;
                    advance();
                    
                    // Check if it's a method call
                    if (match(TokenType::ParenOpen)) {
                        auto methodCall = std::make_unique<MethodCallExpr>();
                        methodCall->loc.line = previous().line;
                        methodCall->loc.column = previous().column;
                        methodCall->object = std::move(result);
                        methodCall->method = fieldName;
                        
                        if (!check(TokenType::ParenClose)) {
                            do {
                                auto arg = parseExpression();
                                methodCall->args.push_back(std::move(arg));
                            } while (match(TokenType::Comma));
                        }
                        match(TokenType::ParenClose);
                        result = std::move(methodCall);
                    } else {
                        // Member access
                        auto memberExpr = std::make_unique<MemberExpr>();
                        memberExpr->loc.line = previous().line;
                        memberExpr->loc.column = previous().column;
                        memberExpr->object = std::move(result);
                        memberExpr->field = fieldName;
                        result = std::move(memberExpr);
                    }
                } else if (match(TokenType::BracketOpen)) {
                    auto indexExpr = std::make_unique<IndexExpr>();
                    indexExpr->loc.line = previous().line;
                    indexExpr->loc.column = previous().column;
                    indexExpr->collection = std::move(result);
                    indexExpr->index = parseExpression();
                    match(TokenType::BracketClose);
                    result = std::move(indexExpr);
                } else {
                    break;
                }
            }
            
            return result;
        }

        if (match(TokenType::Identifier)) {

            SourceLocation idLoc;
            idLoc.line = previous().line;
            idLoc.column = previous().column;
            std::string val = previous().value;

            if (match(TokenType::ParenOpen)) {

                bool looksLikeStructInit = false;
                if (check(TokenType::Identifier)) {
                    size_t savedPos = current;
                    std::string firstArg = peek().value;
                    advance(); 
                    if (check(TokenType::Assign)) {
                        looksLikeStructInit = true;
                    }
                    current = savedPos;
                }
                
                if (looksLikeStructInit) {
                    // This is a struct instantiation example: Person(name="text", age=51)
                    auto structExpr = std::make_unique<StructExpr>();
                    structExpr->loc = idLoc;
                    structExpr->typeName = val;
                    
                    if (!check(TokenType::ParenClose)) {
                        do {
                            if (!check(TokenType::Identifier)) {
                                error("Expected field name");
                                break;
                            }
                            std::string fieldName = peek().value;
                            advance(); 
                            
                            if (!match(TokenType::Assign)) {
                                error("Expected '=' after field name");
                                break;
                            }
                            
                            ExprPtr fieldValue = parseExpression();
                            structExpr->fields.push_back({fieldName, std::move(fieldValue)});
                        } while (match(TokenType::Comma));
                    }
                    match(TokenType::ParenClose);
                    
                    ExprPtr result = std::move(structExpr);
                    while (true) {
                        if (match(TokenType::Dot)) {
                            if (!check(TokenType::Identifier)) {
                                error("Expected field name after '.'");
                                break;
                            }
                            std::string memberField = peek().value;
                            advance();
                            auto memberExpr = std::make_unique<MemberExpr>();
                            memberExpr->loc.line = previous().line;
                            memberExpr->loc.column = previous().column;
                            memberExpr->object = std::move(result);
                            memberExpr->field = memberField;
                            result = std::move(memberExpr);
                        } else if (match(TokenType::BracketOpen)) {
                            auto indexExpr = std::make_unique<IndexExpr>();
                            indexExpr->loc.line = previous().line;
                            indexExpr->loc.column = previous().column;
                            indexExpr->collection = std::move(result);
                            indexExpr->index = parseExpression();
                            match(TokenType::BracketClose);
                            result = std::move(indexExpr);
                        } else {
                            break;
                        }
                    }
                    
                    return result;
                }
                
                auto call = std::make_unique<CallExpr>();
                call->loc = idLoc;
                auto id = std::make_unique<IdentifierExpr>(val);
                id->loc = idLoc;
                call->callee = std::move(id);

                if (!check(TokenType::ParenClose)) {
                    do {
                        auto arg = parseExpression();
                        call->args.push_back(std::move(arg));
                    } while (match(TokenType::Comma));
                }

                match(TokenType::ParenClose);
                
                ExprPtr result = std::move(call);
                while (true) {
                    if (match(TokenType::Dot)) {
                        if (!check(TokenType::Identifier)) {
                            error("Expected field name after '.'");
                            break;
                        }
                        std::string fieldName = peek().value;
                        advance();
                        
                        // Check if it's a method call
                        if (match(TokenType::ParenOpen)) {
                            auto methodCall = std::make_unique<MethodCallExpr>();
                            methodCall->loc.line = previous().line;
                            methodCall->loc.column = previous().column;
                            methodCall->object = std::move(result);
                            methodCall->method = fieldName;
                            
                            if (!check(TokenType::ParenClose)) {
                                do {
                                    auto arg = parseExpression();
                                    methodCall->args.push_back(std::move(arg));
                                } while (match(TokenType::Comma));
                            }
                            match(TokenType::ParenClose);
                            result = std::move(methodCall);
                        } else {
                            // Member access
                            auto memberExpr = std::make_unique<MemberExpr>();
                            memberExpr->loc.line = previous().line;
                            memberExpr->loc.column = previous().column;
                            memberExpr->object = std::move(result);
                            memberExpr->field = fieldName;
                            result = std::move(memberExpr);
                        }
                    } else if (match(TokenType::BracketOpen)) {
                        auto indexExpr = std::make_unique<IndexExpr>();
                        indexExpr->loc.line = previous().line;
                        indexExpr->loc.column = previous().column;
                        indexExpr->collection = std::move(result);
                        indexExpr->index = parseExpression();
                        match(TokenType::BracketClose);
                        result = std::move(indexExpr);
                    } else {
                        break;
                    }
                }
                
                return result;
            }

            // Method call: identifier.identifier(args)
            if (match(TokenType::Dot)) {
                if (check(TokenType::Identifier)) {
                    std::string methodName = peek().value;
                    advance();
                    
                    // Check if it's a method call with parentheses
                    if (match(TokenType::ParenOpen)) {
                        auto methodCall = std::make_unique<MethodCallExpr>();
                        methodCall->loc = idLoc;
                        auto ident = std::make_unique<IdentifierExpr>(val);
                        ident->loc = idLoc;
                        methodCall->object = std::move(ident);
                        methodCall->method = methodName;
                        
                        // Parse arguments
                        if (!check(TokenType::ParenClose)) {
                            do {
                                auto arg = parseExpression();
                                methodCall->args.push_back(std::move(arg));
                            } while (match(TokenType::Comma));
                        }
                        match(TokenType::ParenClose);
                        
                        // Handle chained method calls: a.push(1).pop()
                        ExprPtr result = std::move(methodCall);
                        while (true) {
                            if (match(TokenType::Dot)) {
                                if (check(TokenType::Identifier)) {
                                    std::string nextMethod = peek().value;
                                    advance();
                                    
                                    if (match(TokenType::ParenOpen)) {
                                        auto nextCall = std::make_unique<MethodCallExpr>();
                                        SourceLocation loc;
                                        loc.line = previous().line;
                                        loc.column = previous().column;
                                        nextCall->loc = loc;
                                        nextCall->object = std::move(result);
                                        nextCall->method = nextMethod;
                                        
                                        if (!check(TokenType::ParenClose)) {
                                            do {
                                                auto arg = parseExpression();
                                                nextCall->args.push_back(std::move(arg));
                                            } while (match(TokenType::Comma));
                                        }
                                        match(TokenType::ParenClose);
                                        result = std::move(nextCall);
                                    } else {
                                        // It's a member access (shouldn't normally happen here but handle anyway)
                                        auto memberExpr = std::make_unique<MemberExpr>();
                                        SourceLocation loc;
                                        loc.line = previous().line;
                                        loc.column = previous().column;
                                        memberExpr->loc = loc;
                                        memberExpr->object = std::move(result);
                                        memberExpr->field = nextMethod;
                                        result = std::move(memberExpr);
                                    }
                                } else {
                                    break;
                                }
                            } else if (match(TokenType::BracketOpen)) {
                                auto indexExpr = std::make_unique<IndexExpr>();
                                SourceLocation loc;
                                loc.line = previous().line;
                                loc.column = previous().column;
                                indexExpr->loc = loc;
                                indexExpr->collection = std::move(result);
                                indexExpr->index = parseExpression();
                                match(TokenType::BracketClose);
                                result = std::move(indexExpr);
                            } else {
                                break;
                            }
                        }
                        
                        return result;
                    } else {
                        auto memberExpr = std::make_unique<MemberExpr>();
                        memberExpr->loc = idLoc;
                        auto ident = std::make_unique<IdentifierExpr>(val);
                        ident->loc = idLoc;
                        memberExpr->object = std::move(ident);
                        memberExpr->field = methodName;

                        ExprPtr result = std::move(memberExpr);
                        while (true) {
                            if (match(TokenType::Dot)) {
                                if (!check(TokenType::Identifier)) {
                                    error("Expected field name after '.'");
                                    break;
                                }
                                std::string nextField = peek().value;
                                advance();
                                auto nextMember = std::make_unique<MemberExpr>();
                                SourceLocation loc;
                                loc.line = previous().line;
                                loc.column = previous().column;
                                nextMember->loc = loc;
                                nextMember->object = std::move(result);
                                nextMember->field = nextField;
                                result = std::move(nextMember);
                            } else if (match(TokenType::BracketOpen)) {
                                auto indexExpr = std::make_unique<IndexExpr>();
                                SourceLocation loc;
                                loc.line = previous().line;
                                loc.column = previous().column;
                                indexExpr->loc = loc;
                                indexExpr->collection = std::move(result);
                                indexExpr->index = parseExpression();
                                match(TokenType::BracketClose);
                                result = std::move(indexExpr);
                            } else {
                                break;
                            }
                        }
                        
                        return result;
                    }
                } else {
                    error("Expected identifier after '.'");
                }
            }

            // Index access: identifier[expression]
            if (match(TokenType::BracketOpen)) {
                auto indexExpr = std::make_unique<IndexExpr>();
                indexExpr->loc = idLoc;
                auto ident = std::make_unique<IdentifierExpr>(val);
                ident->loc = idLoc;
                indexExpr->collection = std::move(ident);
                indexExpr->index = parseExpression();
                match(TokenType::BracketClose);
                
                // Handle chained indices: identifier[0][1]
                ExprPtr result = std::move(indexExpr);
                while (true) {
                    if (match(TokenType::Dot)) {
                        if (!check(TokenType::Identifier)) {
                            error("Expected field name after '.'");
                            break;
                        }
                        std::string fieldName = peek().value;
                        advance();
                        
                        // Check if it's a method call
                        if (match(TokenType::ParenOpen)) {
                            auto methodCall = std::make_unique<MethodCallExpr>();
                            methodCall->loc.line = previous().line;
                            methodCall->loc.column = previous().column;
                            methodCall->object = std::move(result);
                            methodCall->method = fieldName;
                            
                            if (!check(TokenType::ParenClose)) {
                                do {
                                    auto arg = parseExpression();
                                    methodCall->args.push_back(std::move(arg));
                                } while (match(TokenType::Comma));
                            }
                            match(TokenType::ParenClose);
                            result = std::move(methodCall);
                        } else {
                            // Member access
                            auto memberExpr = std::make_unique<MemberExpr>();
                            memberExpr->loc.line = previous().line;
                            memberExpr->loc.column = previous().column;
                            memberExpr->object = std::move(result);
                            memberExpr->field = fieldName;
                            result = std::move(memberExpr);
                        }
                    } else if (match(TokenType::BracketOpen)) {
                        auto chainedIndex = std::make_unique<IndexExpr>();
                        chainedIndex->loc.line = previous().line;
                        chainedIndex->loc.column = previous().column;
                        chainedIndex->collection = std::move(result);
                        chainedIndex->index = parseExpression();
                        match(TokenType::BracketClose);
                        result = std::move(chainedIndex);
                    } else {
                        break;
                    }
                }
                
                return result;
            }

            // Member access: identifier.identifier
            if (match(TokenType::Dot)) {
                // This is either a struct instantiation or member access
                if (check(TokenType::Identifier)) {
                    std::string fieldName = peek().value;
                    advance();
                    
                    // Check if this is followed by parentheses (struct instantiation with args)
                    if (match(TokenType::ParenOpen)) {
                        // Struct instantiation: Person(name="text", age=51)
                        auto structExpr = std::make_unique<StructExpr>();
                        structExpr->loc = idLoc;
                        structExpr->typeName = val;
                        
                        // Parse named arguments
                        if (!check(TokenType::ParenClose)) {
                            do {
                                // Expect field name
                                if (!check(TokenType::Identifier)) {
                                    error("Expected field name");
                                    break;
                                }
                                std::string field = peek().value;
                                advance();
                                
                                // Expect equals sign
                                if (!match(TokenType::Assign)) {
                                    error("Expected '=' after field name");
                                    break;
                                }
                                
                                // Parse field value
                                ExprPtr fieldValue = parseExpression();
                                structExpr->fields.push_back({field, std::move(fieldValue)});
                            } while (match(TokenType::Comma));
                        }
                        match(TokenType::ParenClose);
                        
                        // Handle chained member access or index
                        ExprPtr result = std::move(structExpr);
                        while (true) {
                            if (match(TokenType::Dot)) {
                                if (!check(TokenType::Identifier)) {
                                    error("Expected field name after '.'");
                                    break;
                                }
                                std::string memberField = peek().value;
                                advance();
                                auto memberExpr = std::make_unique<MemberExpr>();
                                memberExpr->loc.line = previous().line;
                                memberExpr->loc.column = previous().column;
                                memberExpr->object = std::move(result);
                                memberExpr->field = memberField;
                                result = std::move(memberExpr);
                            } else if (match(TokenType::BracketOpen)) {
                                auto indexExpr = std::make_unique<IndexExpr>();
                                indexExpr->loc.line = previous().line;
                                indexExpr->loc.column = previous().column;
                                indexExpr->collection = std::move(result);
                                indexExpr->index = parseExpression();
                                match(TokenType::BracketClose);
                                result = std::move(indexExpr);
                            } else {
                                break;
                            }
                        }
                        
                        return result;
                    } else {
                        // Member access: identifier.identifier
                        auto memberExpr = std::make_unique<MemberExpr>();
                        memberExpr->loc = idLoc;
                        auto ident = std::make_unique<IdentifierExpr>(val);
                        ident->loc = idLoc;
                        memberExpr->object = std::move(ident);
                        memberExpr->field = fieldName;
                        
                        // Handle chained member access
                        ExprPtr result = std::move(memberExpr);
                        while (match(TokenType::Dot)) {
                            if (!check(TokenType::Identifier)) {
                                error("Expected field name after '.'");
                                break;
                            }
                            std::string memberField = peek().value;
                            advance();
                            auto chainedMember = std::make_unique<MemberExpr>();
                            chainedMember->loc.line = previous().line;
                            chainedMember->loc.column = previous().column;
                            chainedMember->object = std::move(result);
                            chainedMember->field = memberField;
                            result = std::move(chainedMember);
                        }
                        
                        return result;
                    }
                }
            }

            auto id = std::make_unique<IdentifierExpr>(val);
            id->loc = idLoc;
            
            if (match(TokenType::PlusPlus) || match(TokenType::MinusMinus)) {
                // Set location from the operator token
                SourceLocation uLoc;
                uLoc.line = previous().line;
                uLoc.column = previous().column;
                
                auto u = std::make_unique<UnaryExpr>();
                u->loc = uLoc;
                u->op = previous().value;
                u->operand = std::move(id);
                return u;
            }

            return id;
        }

        if (match(TokenType::ParenOpen)) {
            // Set location from opening paren
            SourceLocation parenLoc;
            parenLoc.line = previous().line;
            parenLoc.column = previous().column;
            
            auto expr = parseExpression();
            match(TokenType::ParenClose);
            
            // Handle index and member access after parenthesized expression
            while (true) {
                if (match(TokenType::Dot)) {
                    if (!check(TokenType::Identifier)) {
                        error("Expected field name after '.'");
                        break;
                    }
                    std::string fieldName = peek().value;
                    advance();
                    auto memberExpr = std::make_unique<MemberExpr>();
                    memberExpr->loc.line = previous().line;
                    memberExpr->loc.column = previous().column;
                    memberExpr->object = std::move(expr);
                    memberExpr->field = fieldName;
                    expr = std::move(memberExpr);
                } else if (match(TokenType::BracketOpen)) {
                    auto indexExpr = std::make_unique<IndexExpr>();
                    indexExpr->loc.line = previous().line;
                    indexExpr->loc.column = previous().column;
                    indexExpr->collection = std::move(expr);
                    indexExpr->index = parseExpression();
                    match(TokenType::BracketClose);
                    expr = std::move(indexExpr);
                } else {
                    break;
                }
            }
            
            // Set location on parenthesized expression if we have one
            if (expr) {
                expr->loc = parenLoc;
            }
            
            return expr;
        }

        // Nothing matches — safely return nullptr
        return nullptr;
    }

    int Parser::getPrecedence(const Token& tk) const {
        switch (tk.type) {
            case TokenType::Caret: return 80;
            case TokenType::Star:
            case TokenType::Slash:
            case TokenType::Percent: return 70;
            case TokenType::Plus:
            case TokenType::Minus: return 60;
            case TokenType::EqualEqual:
            case TokenType::BangEqual:
            case TokenType::Less:
            case TokenType::LessEqual:
            case TokenType::Greater:
            case TokenType::GreaterEqual: return 30;
            case TokenType::AmpersandAmpersand:
            case TokenType::PipePipe: return 20;
            case TokenType::Arrow: return 5;
            default: return -1;
        }
    }

} // namespace nova