#pragma once
#include "ast.h"
#include <iostream>
#include <string>

namespace nova {

class AstPrinter {
public:
    explicit AstPrinter(std::ostream& out) : out(out), indentLevel(0) {}

    void printProgram(const Program& prog);

private:
    std::ostream& out;
    int indentLevel;

    void indent();
    void println(const std::string& s);
    void printDecl(const Declaration& d);
    void printVarDecl(const VarDecl& v);
    void printFuncDecl(const FuncDecl& f);
    void printClassDecl(const ClassDecl& c);
    void printStructDecl(const StructDecl& s);
    void printStmt(const Statement& s);
    void printExpr(const Expression& e);
    void printType(const TypeNode& t);
    void printBlock(const BlockStmt& block);

    // helpers to downcast safely
    bool isVarDecl(const Declaration& d) const;
};

} // namespace nova