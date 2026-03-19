#pragma once
#include <string>

namespace nova {
    enum class TokenType {
        Identifier,
        Keyword,
        Number,
        String,
        Boolean,
        Plus,
        Minus,
        Star,
        Slash,
        Percent,
        PlusPlus,
        MinusMinus,
        PlusEqual,
        MinusEqual,
        EqualEqual,
        Bang,
        BangEqual,
        Less,
        LessEqual,
        Greater,
        StarEqual,
        SlashEqual, 
        Caret,
        GreaterEqual,
        AmpersandAmpersand,
        PipePipe,
        Arrow,
        Separator,
        ParenOpen,
        ParenClose,
        BracketOpen,
        BracketClose,
        BraceOpen,
        BraceClose,
        Colon,
        Semicolon,
        Comma,
        Dot,
        Assign,
        Pointer,
        Comment,
        EndOfFile,
        Unknown
    };

    enum class NumberKind { None, Integer, Float };
    struct Token {
        TokenType type;
        std::string value = ""; // for identifiers, literals
        
        int line;
        int column;

        NumberKind numberKind = NumberKind::None; // valid if type == Number/Float
    };
}