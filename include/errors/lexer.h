#pragma once
#include <string>
#include <stdexcept>

namespace nova {
    
    /**
     * Base class for all lexer errors.
     * Provides line and column information for easy debugging.
     */
    class LexerError : public std::runtime_error {
    public:
        LexerError(const std::string& message, int line, int column)
            : std::runtime_error(formatMessage(message, line, column)), 
              line(line), column(column) {}
        
        virtual ~LexerError() = default;
        
        int getLine() const { return line; }
        int getColumn() const { return column; }
        
    private:
        int line;
        int column;
        
        static std::string formatMessage(const std::string& message, int line, int column) {
            return "Lexer Error at line " + std::to_string(line) + 
                   ", column " + std::to_string(column) + ": " + message;
        }
    };
    
    /**
     * Error thrown when a string literal is not properly closed.
     * Example: "unclosed string
     */
    class UnterminatedStringError : public LexerError {
    public:
        UnterminatedStringError(int line, int column, const std::string& partialContent = "")
            : LexerError(buildMessage(partialContent), line, column),
              partialContent(partialContent) {}
        
        const std::string& getPartialContent() const { return partialContent; }
        
    private:
        std::string partialContent;
        
        static std::string buildMessage(const std::string& partialContent) {
            if (partialContent.empty()) {
                return "Unterminated string literal - missing closing quote (\" or ')";
            }
            return "Unterminated string literal - started with content: \"" + 
                   partialContent + "\" but never found closing quote";
        }
    };
    
    /**
     * Error thrown when an unrecognized character is encountered.
     * Example: @ or $ or ©
     */
    class InvalidCharacterError : public LexerError {
    public:
        InvalidCharacterError(int line, int column, char invalidChar)
            : LexerError(buildMessage(invalidChar), line, column),
              invalidChar(invalidChar) {}
        
        char getInvalidCharacter() const { return invalidChar; }
        
    private:
        char invalidChar;
        
        static std::string buildMessage(char c) {
            if (c == '\0') {
                return "Unexpected end of input";
            }
            return std::string("Invalid character: '") + c + "' (ASCII: " + 
                   std::to_string(static_cast<int>(c)) + ") - not recognized by the lexer";
        }
    };
    
    /**
     * Error thrown when a multi-line comment is not properly closed.
     * Nova uses # for single-line, so this is for future extension
     * or if we add block comment support.
     */
    class UnterminatedCommentError : public LexerError {
    public:
        UnterminatedCommentError(int line, int column)
            : LexerError("Unterminated comment - reached end of input while parsing comment", 
                        line, column) {}
    };
    

    /**
     * Error thrown when a number literal is malformed.
     * Example: 1.2.3 or 123fabc
     */
    class InvalidNumberError : public LexerError {
    public:
        InvalidNumberError(int line, int column, const std::string& malformedNumber)
            : LexerError(buildMessage(malformedNumber), line, column),
              malformedNumber(malformedNumber) {}
        
        const std::string& getMalformedNumber() const { return malformedNumber; }
        
    private:
        std::string malformedNumber;
        
        static std::string buildMessage(const std::string& num) {
            return "Invalid number format: \"" + num + "\" - check for stray characters or dots";
        }
    };
    
    /**
     * Error thrown when an escape sequence in a string is invalid.
     * Example: "\q" (q is not a valid escape)
     */
    class InvalidEscapeSequenceError : public LexerError {
    public:
        InvalidEscapeSequenceError(int line, int column, char invalidEscape)
            : LexerError(buildMessage(invalidEscape), line, column),
              invalidEscape(invalidEscape) {}
        
        char getInvalidEscape() const { return invalidEscape; }
        
    private:
        char invalidEscape;
        
        static std::string buildMessage(char c) {
            return std::string("Invalid escape sequence: '\\") + c + 
                   "' - valid escapes are: \\n, \\t, \\r, \\', \\\", \\\\";
        }
    };
    
    /**
     * Error thrown when a character literal is malformed.
     * Example: 'ab' (more than one character)
     */
    class InvalidCharacterLiteralError : public LexerError {
    public:
        InvalidCharacterLiteralError(int line, int column, const std::string& content)
            : LexerError(buildMessage(content), line, column),
              content(content) {}
        
        const std::string& getContent() const { return content; }
        
    private:
        std::string content;
        
        static std::string buildMessage(const std::string& c) {
            if (c.empty()) {
                return "Empty character literal - must contain exactly one character";
            }
            return "Invalid character literal: '" + c + "' - character literals can only contain one character";
        }
    };
    
}
