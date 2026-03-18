#pragma once
#include "token.h"
#include <string>
#include <vector>
#include <unordered_set>

namespace nova {
    class Lexer {
    public:
        explicit Lexer(const std::string& src);

        // Tokenize the whole input. Comments are returned as Comment tokens;
        // caller can choose to filter them out.
        std::vector<Token> tokenize();

    private:
        std::string source;
        size_t pos;
        int line;
        int col;

        char peek(int ahead = 0) const;
        char get();
        void skipWhitespace();
        Token nextToken();

        static const std::unordered_set<char> singleCharOps;
        static const std::unordered_set<std::string> doubleCharOps;
    };
}