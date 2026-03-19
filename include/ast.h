#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace nova
{

    // Forward declarations
    struct ASTNode;
    struct Statement;
    struct Expression;
    struct TypeNode;
    struct Declaration;

    // Aliases
    using ASTNodePtr = std::unique_ptr<ASTNode>;
    using StmtPtr = std::unique_ptr<Statement>;
    using ExprPtr = std::unique_ptr<Expression>;
    using TypePtr = std::unique_ptr<TypeNode>;
    using DeclPtr = std::unique_ptr<Declaration>;

    // Node location info (line/column)
    struct SourceLocation
    {
        int line;
        int column;
    };

    // Base AST node
    struct ASTNode
    {
        SourceLocation loc;
        virtual ~ASTNode() = default;
    };

    // Program (top-level container)
    struct Program : ASTNode
    {
        std::vector<DeclPtr> decls;
        bool isREPL = false;
    };

    // Types
    struct TypeNode : ASTNode
    {
        virtual ~TypeNode() = default;
    };

    struct SimpleType : TypeNode
    {
        std::string name;      // e.g. "int", "str", "float32", "list", etc.
        bool optional = false; // `?` suffix
        SimpleType() = default;
        explicit SimpleType(std::string n, bool opt = false) : name(std::move(n)), optional(opt) {}
    };

    struct Statement : ASTNode
    {
        virtual ~Statement() = default;
    };

    struct Declaration : ASTNode
    {
        virtual ~Declaration() = default;
    };

    struct Expression : ASTNode
    {
        virtual ~Expression() = default;
    };


    // Block bodys {} are common in functions, if/else, loops, etc. We can represent them as a statement containing a list of statements.
    struct BlockStmt : Statement
    {
        std::vector<StmtPtr> statements;
    };

    struct ListType : TypeNode
    {
        TypePtr elementType;          // may be nullptr for untyped list
        std::optional<int> fixedSize; // if fixed size declared: list[T][4] style
    };

    struct PointerType : TypeNode
    {
        TypePtr pointee;
    };

    struct TupleType : TypeNode
    {
        std::vector<TypePtr> elements;
    };

    // Declarations

    struct VarDecl : Declaration
    {
        bool isConst = false;
        std::string name;
        TypePtr type; // optional
        ExprPtr init; // optional initializer
    };

    struct Param
    {
        std::string name;
        TypePtr type; // may be nullptr
    };

    struct FuncDecl : Declaration
    {
        std::string name;
        std::vector<Param> params;
        TypePtr returnType = nullptr; // may be nullptr (void)
        BlockStmt body;
    };

    struct StructDecl : Declaration
    {
        std::string name;
        std::vector<VarDecl> members;
    };

    // Represents a single imported symbol with optional alias
    struct ImportSymbol {
        std::string originalName;  // original symbol name in the module
        std::string alias;          // alias name (empty if no alias)
    };

    struct ImportDecl : Declaration
    {
        std::string path;  // the import path, e.g., "lists", "core/test/simple_lib"
        std::vector<ImportSymbol> symbols;  // empty means import all (namespace import)
        bool isSelective = false;  // true if using { symbol } syntax
    };

    struct ClassDecl : Declaration
    {
        std::string name;
        std::vector<VarDecl> members;
        std::vector<FuncDecl> methods;
    };

    // Statements

    struct ExprStmt : Statement {
        ExprPtr expr;

        // Default constructor
        ExprStmt() = default;

        // Constructor that takes an expression
        explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
    };

    struct ReturnStmt : Statement
    {
        ExprPtr expr; // may be nullptr for implicit None / void

        ReturnStmt() = default;

        explicit ReturnStmt(ExprPtr e) : expr(std::move(e)) {}
    };

    struct IfStmt : Statement
    {
        ExprPtr cond;
        BlockStmt thenBody;
        BlockStmt elseBody; // empty if no else
        bool hasElse = false;
    };

    struct DeclStmt : Statement
    {
        DeclPtr decl;

        DeclStmt() = default;

        explicit DeclStmt(DeclPtr d) : decl(std::move(d)) {}
    };
    struct StmtDecl : Declaration {
        StmtPtr stmt;
        explicit StmtDecl(StmtPtr s) : stmt(std::move(s)) {
            if (!stmt) {
                std::__throw_runtime_error("Cannot wrap nullptr in StmtDecl");
            }
        }
};

    struct ForStmt : Statement
    {
        // initializer: either a declaration or expression  = for ( initializer? ; condition? ; iterator? ) statement
        StmtPtr init = nullptr;
        ExprPtr condition = nullptr;
        ExprPtr iterator = nullptr;
        BlockStmt body;
    };

    struct WhileStmt : Statement
    {
        ExprPtr cond;
        BlockStmt body;
    };

    // For-in loop: for (item in collection)
    struct ForInStmt : Statement
    {
        std::string varName;    // the loop variable name (e.g., item)
        ExprPtr collection;      // the collection to iterate over
        BlockStmt body;          // the loop body
    };

    // Break statement
    struct BreakStmt : Statement
    {
    };

    // Continue statement
    struct ContinueStmt : Statement
    {
    };

    // Global statement: global x - declares that a variable refers to global scope
    struct GlobalStmt : Statement
    {
        std::vector<std::string> names;  // variable names declared as global
    };

    // Expressions

    struct IdentifierExpr : Expression
    {
        std::string name;
        explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
    };

    struct LiteralExpr : Expression
    {
        enum class LitKind
        {
            Integer,
            Float,
            String,
            Boolean,
            None
        } kind;
        std::string raw; // textual representation
        LiteralExpr(LitKind k, std::string r) : kind(k), raw(std::move(r)) {}
    };

    struct BinaryExpr : Expression
    {
        std::string op;
        ExprPtr left;
        ExprPtr right;
    };

    struct UnaryExpr : Expression
    {
        std::string op;
        ExprPtr operand;
    };

    struct CallExpr : Expression
    {
        ExprPtr callee;
        std::vector<ExprPtr> args;
    };

    struct IndexExpr : Expression
    {
        ExprPtr collection;
        ExprPtr index;
    };

    struct TupleExpr : Expression
    {
        std::vector<ExprPtr> elements;
    };

    struct ListExpr : Expression
    {
        std::vector<ExprPtr> elements;
    };

    struct AssignmentExpr : Expression
    {
        ExprPtr target; // typically IdentifierExpr or IndexExpr
        ExprPtr value;

        std::string op = "="; // for now only support simple assignment, but can extend to +=, -=, etc.
    };

    // Member expression for accessing struct fields: test.name or other object oriented access in future
    struct MemberExpr : Expression
    {
        ExprPtr object;  // the struct instance (e.g., test)
        std::string field; // the field name (e.g., name)
    };

    // Method call expression: object.method(args)
    struct MethodCallExpr : Expression
    {
        ExprPtr object;  // the object (e.g., a in a.push(5))
        std::string method; // the method name (e.g., push)
        std::vector<ExprPtr> args; // the arguments
    };

    // Struct instantiation expression: Person(name="text", age=51)
    struct StructExpr : Expression
    {
        std::string typeName; // the struct type name (e.g., Person)
        std::vector<std::pair<std::string, ExprPtr>> fields; // field name and value expression pairs
    };

    // Utility helpers
    inline ExprPtr make_id(const std::string &s)
    {
        return std::make_unique<IdentifierExpr>(s);
    }

}