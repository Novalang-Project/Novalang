#pragma once
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_set>
#include "lexer.h"
#include "ast.h"

namespace nova {

// Parser error struct
struct ParseError {
    int line;
    int column;
    std::string message;

    ParseError(int line, int column, std::string message)
        : line(line), column(column), message(std::move(message)) {}
};

using ASTNodePtr = std::unique_ptr<ASTNode>;
using DeclPtr = std::unique_ptr<Declaration>;
using StmtPtr = std::unique_ptr<Statement>;
using ExprPtr = std::unique_ptr<Expression>;
using TypePtr = std::unique_ptr<TypeNode>;

class Parser {
public:
    Parser(const std::vector<Token>& tokens);

    ASTNodePtr parseProgram();

    DeclPtr parseVarDecl(bool isConst);
    DeclPtr parseFuncDecl();
    DeclPtr parseStructDecl();
    DeclPtr parseImportDecl();
    DeclPtr parseDeclaration();

    StmtPtr parseStatement();
    StmtPtr parseExpressionStatement();
    BlockStmt parseBlock();

    TypePtr parseType();

    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseBinary(int precedence = 0);
    ExprPtr parseUnary();
    ExprPtr parsePrimary();

    int getPrecedence(const Token& tk) const;

    bool isAtEnd() const;

    bool check(TokenType type) const;
    const Token& advance();

    // Error handling
    void error(const std::string& message);
    void errorAt(const Token& token, const std::string& message);
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<ParseError>& getErrors() const { return errors; }
    void clearErrors() { errors.clear(); }

private:
    const std::vector<Token>& tokens;
    size_t current = 0;
    std::vector<ParseError> errors;
    std::unordered_set<std::string> structNames;  // Track declared struct types

public:
    void addStructName(const std::string& name) { structNames.insert(name); }
    bool isStructType(const std::string& name) const { return structNames.count(name) > 0; }

    const Token& peek() const;
    const Token& previous() const;
    bool match(TokenType type);
};

} // namespace nova