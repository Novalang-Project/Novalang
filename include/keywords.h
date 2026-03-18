#pragma once
#include <string>
#include <unordered_set>

namespace nova {
    inline const std::unordered_set<std::string> keywords = {
        "func", "const", "class", "return", "if", "else", "elif", "none",
        "for", "while", "self", "true", "false", "struct",
        "break", "continue", "in", "import", "global"
    };

    inline bool is_keyword(const std::string& s) {
        return keywords.find(s) != keywords.end();
    }
}