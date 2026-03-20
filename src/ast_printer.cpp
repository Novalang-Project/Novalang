#include "ast_printer.h"
#include <typeinfo>
#include <sstream> 

using namespace nova;

// currently not used


void AstPrinter::indent() {
    for (int i = 0; i < indentLevel; ++i) out << "  ";
}

void AstPrinter::println(const std::string& s) {
    indent();
    out << s << "\n";
}

void AstPrinter::printProgram(const Program& prog) {
    out << "Program {\n";
    ++indentLevel;
    for (size_t i = 0; i < prog.decls.size(); ++i) {
        if (prog.decls[i]) printDecl(*prog.decls[i]);
    }
    --indentLevel;
    out << "}\n";
}

void AstPrinter::printDecl(const Declaration& d) {
    // RTTI-based dispatch (simple)
    if (const VarDecl* v = dynamic_cast<const VarDecl*>(&d)) {
        printVarDecl(*v);
    } else if (const FuncDecl* f = dynamic_cast<const FuncDecl*>(&d)) {
        printFuncDecl(*f);
    } else if (const StructDecl* s = dynamic_cast<const StructDecl*>(&d)) {
        printStructDecl(*s);
    } else if (const ClassDecl* c = dynamic_cast<const ClassDecl*>(&d)) {
        printClassDecl(*c);
    } else if (const StmtDecl* sd = dynamic_cast<const StmtDecl*>(&d)) {
        if (sd->stmt) {
            printStmt(*sd->stmt);
        } else {
            println("StmtDecl: (null stmt)");
        }
    } else {
        println(std::string("Unknown Declaration: ") + typeid(d).name());
    }
}

void AstPrinter::printVarDecl(const VarDecl& v) {
    std::string s = (v.isConst ? "const " : "let ") + v.name;
    if (v.type) {
        s += " : ";
        // print type into temporary stream
        std::ostringstream ss;
        // crude: call printType but it prints to out; instead temporarily redirect
        // For simplicity call printType directly with current out, then return.
        println(s);
        ++indentLevel;
        printType(*v.type);
        if (v.init) {
            println("init:");
            ++indentLevel;
            printExpr(*v.init);
            --indentLevel;
        }
        --indentLevel;
    } else {
        println(s);
        if (v.init) {
            ++indentLevel;
            println("init:");
            ++indentLevel;
            printExpr(*v.init);
            --indentLevel; --indentLevel;
        }
    }
}

void AstPrinter::printFuncDecl(const FuncDecl& f)
{
    std::string s = "func " + f.name + "(";

    for (size_t i = 0; i < f.params.size(); ++i) {
        s += f.params[i].name;
        if (i + 1 < f.params.size()) s += ", ";
    }

    s += ")";
    println(s);

    ++indentLevel;
    printBlock(f.body);
    --indentLevel;
}

void AstPrinter::printClassDecl(const ClassDecl& c) {
    println("class " + c.name);
    ++indentLevel;
    println("{");
    ++indentLevel;
    for (const auto& m : c.members) {
        printVarDecl(m);
    }
    --indentLevel;
    println("}");
    --indentLevel;
}

void AstPrinter::printStructDecl(const StructDecl& s) {
    println("struct " + s.name);
    ++indentLevel;
    println("{");
    ++indentLevel;
    for (auto& member : s.members) {
        printVarDecl(member);
    }
    --indentLevel;
    println("}");
    --indentLevel;
}

void AstPrinter::printBlock(const BlockStmt& block)
{
    println("{");
    ++indentLevel;
    for (size_t i = 0; i < block.statements.size(); ++i) {
        if (block.statements[i]) printStmt(*block.statements[i]);
    }
    --indentLevel;
    println("}");
}

void AstPrinter::printStmt(const Statement& s) {
    if (const ExprStmt* es = dynamic_cast<const ExprStmt*>(&s)) {
        println("ExprStmt:");
        ++indentLevel;
        if (es->expr) printExpr(*es->expr);
        --indentLevel;
    } else if (const ReturnStmt* rs = dynamic_cast<const ReturnStmt*>(&s)) {
        println("ReturnStmt:");
        ++indentLevel;
        if (rs->expr) printExpr(*rs->expr);
        --indentLevel;
    } else if (const IfStmt* ifs = dynamic_cast<const IfStmt*>(&s)) {
        println("IfStmt:");
        ++indentLevel;

        println("Cond:");
        ++indentLevel;
        printExpr(*ifs->cond);
        --indentLevel;

        println("Then:");
        ++indentLevel;
        printBlock(ifs->thenBody);
        --indentLevel;

        if (ifs->hasElse) {
            println("Else:");
            ++indentLevel;
            printBlock(ifs->elseBody);
            --indentLevel;
        }

        --indentLevel;
    } else if (const ForStmt* fs = dynamic_cast<const ForStmt*>(&s)) {
        println("ForStmt:");
        ++indentLevel;

        if (fs->init) {
            println("Init:");
            ++indentLevel;
            printStmt(*fs->init);
            --indentLevel;
        }

        if (fs->condition) {
            println("Condition:");
            ++indentLevel;
            printExpr(*fs->condition);
            --indentLevel;
        }

        if (fs->iterator) {
            println("Iterator:");
            ++indentLevel;
            printExpr(*fs->iterator);
            --indentLevel;
        }

        println("Body:");
        ++indentLevel;
        printBlock(fs->body);
        --indentLevel;

        --indentLevel;
    } else if (const WhileStmt* ws = dynamic_cast<const WhileStmt*>(&s)) {
        println("WhileStmt:");
        ++indentLevel;

        println("Cond:");
        ++indentLevel;
        printExpr(*ws->cond);
        --indentLevel;

        println("Body:");
        ++indentLevel;
        printBlock(ws->body);
        --indentLevel;

        --indentLevel;
    } else if (const DeclStmt* ds = dynamic_cast<const DeclStmt*>(&s)) {
        println("DeclStmt:");
        ++indentLevel;
        printDecl(*ds->decl);
        --indentLevel;
    } else {
        println(std::string("Unknown Statement: ") + typeid(s).name());
    }
}

void AstPrinter::printExpr(const Expression& e) {
    if (const IdentifierExpr* id = dynamic_cast<const IdentifierExpr*>(&e)) {
        println("Identifier: " + id->name);
    } else if (const LiteralExpr* lit = dynamic_cast<const LiteralExpr*>(&e)) {
        switch (lit->kind) {
            case LiteralExpr::LitKind::Integer: println("Integer: " + lit->raw); break;
            case LiteralExpr::LitKind::Float: println("Float: " + lit->raw); break;
            case LiteralExpr::LitKind::String: println("String: \"" + lit->raw + "\""); break;
            case LiteralExpr::LitKind::None: println("None"); break;
        }
    } else if (const BinaryExpr* b = dynamic_cast<const BinaryExpr*>(&e)) {
        println("BinaryExpr: " + b->op);
        ++indentLevel;
        if (b->left) printExpr(*b->left); else println("(null left)");
        if (b->right) printExpr(*b->right); else println("(null right)");
        --indentLevel;
    } else if (const UnaryExpr* u = dynamic_cast<const UnaryExpr*>(&e)) {
        println("UnaryExpr: " + u->op);
        ++indentLevel;
        if (u->operand) printExpr(*u->operand); else println("(null operand)");
        --indentLevel;
    } else if (const CallExpr* c = dynamic_cast<const CallExpr*>(&e)) {
        println("CallExpr:");
        ++indentLevel;
        println("Callee:");
        ++indentLevel;
        printExpr(*c->callee);
        --indentLevel;
        println("Args:");
        ++indentLevel;
        for (const auto& a : c->args) if (a) printExpr(*a);
        --indentLevel;
        --indentLevel;
    } else if (const IndexExpr* ix = dynamic_cast<const IndexExpr*>(&e)) {
        println("IndexExpr:");
        ++indentLevel;
        println("Collection:");
        ++indentLevel;
        printExpr(*ix->collection);
        --indentLevel;
        println("Index:");
        ++indentLevel;
        printExpr(*ix->index);
        --indentLevel;
        --indentLevel;
    } else if (const TupleExpr* t = dynamic_cast<const TupleExpr*>(&e)) {
        println("TupleExpr:");
        ++indentLevel;
        for (const auto& el : t->elements) if (el) printExpr(*el);
        --indentLevel;
    } else if (const ListExpr* l = dynamic_cast<const ListExpr*>(&e)) {
        println("ListExpr:");
        ++indentLevel;
        for (const auto& el : l->elements) if (el) printExpr(*el);
        --indentLevel;
    } else if (const AssignmentExpr* a = dynamic_cast<const AssignmentExpr*>(&e)) {
        println("Assignment:");
        ++indentLevel;
        println("Target:");
        ++indentLevel;
        printExpr(*a->target);
        --indentLevel;
        println("Value:");
        ++indentLevel;
        printExpr(*a->value);
        --indentLevel;
        --indentLevel;
    } else {
        try
        {
            println(std::string("Unknown Expression: ") + typeid(e).name());
        }
        catch(const std::exception& e)
        {
            println("Unknown Expression: <type info unavailable>");
        }
    }
}

void AstPrinter::printType(const TypeNode& t) {
    if (const SimpleType* s = dynamic_cast<const SimpleType*>(&t)) {
        println("Type: " + s->name + (s->optional ? "?" : ""));
    } else if (const ListType* l = dynamic_cast<const ListType*>(&t)) {
        println("ListType:");
        ++indentLevel;
        if (l->elementType) {
            println("ElementType:");
            ++indentLevel;
            printType(*l->elementType);
            --indentLevel;
        } else {
            println("ElementType: <unspecified>");
        }
        if (l->fixedSize) {
            println("FixedSize: " + std::to_string(*l->fixedSize));
        }
        --indentLevel;
    } else {
        println(std::string("Unknown Type: ") + typeid(t).name());
    }
}