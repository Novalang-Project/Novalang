#include "lexer.h"
#include "keywords.h"
#include "errors/lexer.h"

#include <cctype>
#include <iostream>

using namespace std;
using namespace nova; 

namespace nova {

    Lexer::Lexer(const string& src)
        : source(src), pos(0), line(1), col(1) {}

    char Lexer::peek(int ahead) const {
        if (pos + ahead < source.size()) return source[pos + ahead];
        return '\0';
    }

    char Lexer::get() {
        char c = peek();
        pos++;
        if (c == '\n') { line++; col = 1; }
        else col++;
        return c;
    }

    void Lexer::skipWhitespace() {
        while (isspace(static_cast<unsigned char>(peek()))) get();
    }

    Token Lexer::nextToken() {
        skipWhitespace();

        int startLine = line;
        int startCol = col;
        char c = peek();

        if (c == '\0') {
            return {TokenType::EndOfFile, "", line, col};
        }

        // Single-line comments: //
        if (c == '/' && peek(1) == '/') {
            string comment;
            get(); get();
            while (peek() != '\n' && peek() != '\0') comment += get();
            return {TokenType::Comment, comment, startLine, startCol};
        }

        // Multi-line comments: /* ... */
        if (c == '/' && peek(1) == '*') {
            string comment;
            get(); get();
            while (true) {
                char ch = peek();
                if (ch == '\0') {
                    throw UnterminatedCommentError(startLine, startCol);
                }
                if (ch == '*' && peek(1) == '/') {
                    get(); get();
                    break;
                }
                comment += get();
            }
            return {TokenType::Comment, comment, startLine, startCol};
        }
        
        // Hash comments: #
        if (c == '#') {
            string comment;
            get();
            while (peek() != '\n' && peek() != '\0') comment += get();
            return {TokenType::Comment, comment, startLine, startCol};
        }

        // F-strings f'...' or f"..." (formatted strings with {expressions})
        // MUST check before identifiers since 'f' is alphabetic
        if ((c == 'f' || c == 'F') && (peek(1) == '"' || peek(1) == '\'')) {
            get(); // consume 'f'
            char quote = get();
            string value;
            while (true) {
                char ch = peek();
                if (ch == '\0') {
                    throw UnterminatedStringError(startLine, startCol, value);
                }

                if (ch == quote) {
                    get();
                    break;
                }

                if (ch == '\\') {
                    get();
                    char next = get();
                    switch (next) {
                        case 'n': value += '\n'; break;
                        case 't': value += '\t'; break;
                        case 'r': value += '\r'; break;
                        case '\\': value += '\\'; break;
                        case '"': value += '"'; break;
                        case '\'': value += '\''; break;
                        case '0': value += '\0'; break;
                        default:
                            throw InvalidEscapeSequenceError(line, col, next);
                    }
                } else {
                    value += get();
                }
            }

            return {TokenType::FString, value, startLine, startCol};
        }

        // Identifiers and keywords (allow '?' suffix for option types)
        if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
            string value;
            while (isalnum(static_cast<unsigned char>(peek())) || peek() == '_' || peek() == '?')
                value += get();
        if (value == "true" || value == "false")
            return {TokenType::Boolean, value, startLine, startCol};

        if (is_keyword(value))
            return {TokenType::Keyword, value, startLine, startCol};

        return {TokenType::Identifier, value, startLine, startCol};
        }

        // Numbers (integers and floats)
        if (isdigit(static_cast<unsigned char>(c))) {
            std::string value;
            NumberKind kind = NumberKind::Integer; 

            // Integer part
            while (isdigit(static_cast<unsigned char>(peek()))) value += get();

            // Fractional part
            if (peek() == '.') {
                value += get();
                kind = NumberKind::Float;

                while (isdigit(static_cast<unsigned char>(peek()))) value += get();

                if (isalpha(peek())) {
                    std::string invalidSuffix(1, get()); 
                    throw InvalidNumberError(startLine, startCol, value + invalidSuffix);
                }
            } 
            else if (isalpha(peek())) {
                std::string invalidSuffix(1, get());
                throw InvalidNumberError(startLine, startCol, value + invalidSuffix);
            }

            return Token{TokenType::Number, value, startLine, startCol, kind};
        }

        // Strings '...' or "..."
        if (c == '"' || c == '\'') {
            char quote = get();
            string value;
            while (true) {
                char ch = peek();
                if (ch == '\0') {
                    throw UnterminatedStringError(startLine, startCol, value);
                }

                if (ch == quote) {
                    get();
                    break;
                }

                if (ch == '\\') {
                    get();
                    char next = get();
                    switch (next) {
                        case 'n': value += '\n'; break;
                        case 't': value += '\t'; break;
                        case 'r': value += '\r'; break;
                        case '\\': value += '\\'; break;
                        case '"': value += '"'; break;
                        case '\'': value += '\''; break;
                        case '0': value += '\0'; break;
                        default:
                            throw InvalidEscapeSequenceError(line, col, next);
                    }
                } else {
                    value += get();
                }
            }

            return {TokenType::String, value, startLine, startCol};
        }



// ==== MULTI-CHAR OPERATORS ====
        // ++
        if (c == '+' && peek(1) == '+') {
            get(); get();
            return {TokenType::PlusPlus, "++", startLine, startCol};
        }

        // --
        if (c == '-' && peek(1) == '-') {
            get(); get();
            return {TokenType::MinusMinus, "--", startLine, startCol};
        }

        // +=
        if (c == '+' && peek(1) == '=') {
            get(); get();
            return {TokenType::PlusEqual, "+=", startLine, startCol};
        }

        // -=
        if (c == '-' && peek(1) == '=') {
            get(); get();
            return {TokenType::MinusEqual, "-=", startLine, startCol};
        }

        // ==
        if (c == '=' && peek(1) == '=') {
            get(); get();
            return {TokenType::EqualEqual, "==", startLine, startCol};
        }

        // !=
        if (c == '!' && peek(1) == '=') {
            get(); get();
            return {TokenType::BangEqual, "!=", startLine, startCol};
        }

        // <=
        if (c == '<' && peek(1) == '=') {
            get(); get();
            return {TokenType::LessEqual, "<=", startLine, startCol};
        }

        // >=
        if (c == '>' && peek(1) == '=') {
            get(); get();
            return {TokenType::GreaterEqual, ">=", startLine, startCol};
        }

        // &&
        if (c == '&' && peek(1) == '&') {
            get(); get();
            return {TokenType::AmpersandAmpersand, "&&", startLine, startCol};
        }

        // ||
        if (c == '|' && peek(1) == '|') {
            get(); get();
            return {TokenType::PipePipe, "||", startLine, startCol};
        }

        // ->
        if (c == '-' && peek(1) == '>') {
            get(); get();
            return {TokenType::Arrow, "->", startLine, startCol};
        }
        // *=
        if (c == '*' && peek(1) == '=') {
            get(); get();
            return {TokenType::StarEqual, "*=", startLine, startCol};
        }

        // /=
        if (c == '/' && peek(1) == '=') {
            get(); get();
            return {TokenType::SlashEqual, "/=", startLine, startCol};
        }


// ==== SINGLE-CHAR OPERATORS ====
        switch (c) {
            case '!': get(); return {TokenType::Bang, "!", startLine, startCol};
            case '+': get(); return {TokenType::Plus, "+", startLine, startCol};
            case '-': get(); return {TokenType::Minus, "-", startLine, startCol};
            case '*': get(); return {TokenType::Star, "*", startLine, startCol};
            case '/': get(); return {TokenType::Slash, "/", startLine, startCol};
            case '%': get(); return {TokenType::Percent, "%" , startLine, startCol};
            case '<': get(); return {TokenType::Less, "<", startLine, startCol};
            case '>': get(); return {TokenType::Greater, ">", startLine, startCol};
            case '=': get(); return {TokenType::Assign, "=", startLine, startCol};
            case '^': get(); return {TokenType::Caret, "^", startLine, startCol};
        }

// ==== PUNCTUATION ====
        switch (c) {
            
            case '(': get(); return {TokenType::ParenOpen, "(", startLine, startCol};
            case ')': get(); return {TokenType::ParenClose, ")", startLine, startCol};
            case '[': get(); return {TokenType::BracketOpen, "[", startLine, startCol};
            case ']': get(); return {TokenType::BracketClose, "]", startLine, startCol};
            case '{': get(); return {TokenType::BraceOpen, "{", startLine, startCol};
            case '}': get(); return {TokenType::BraceClose, "}", startLine, startCol};
            case ':': get(); return {TokenType::Colon, ":", startLine, startCol};
            case ';': get(); return {TokenType::Semicolon, ";", startLine, startCol};
            case ',': get(); return {TokenType::Comma, ",", startLine, startCol};
            case '.': get(); return {TokenType::Dot, ".", startLine, startCol};
            default:
                // Unknown single char token - throw an error
                char invalidChar = get();
                throw InvalidCharacterError(startLine, startCol, invalidChar);
        }
    }

    std::vector<Token> Lexer::tokenize() {
        std::vector<Token> tokens;
        while (true) {
            Token tk = nextToken();
            if (tk.type == TokenType::EndOfFile) break;
            if (tk.type != TokenType::Comment) // filter comments by default
                tokens.push_back(tk);
        }
        return tokens;
    }

}