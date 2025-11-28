#ifndef LEXER_H
#define LEXER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"

class Lexer;

class Token
{
    friend class Lexer;

public:
    enum TokenKind : unsigned short
    {
        eoi,            // End of input
        unknown,        // Error

        // Identifiers & Literals
        ident,          // variable names
        number,         // integer literals (e.g. 123)
        float_literal,  // float literals (e.g. 3.14) - Optional if needed later

        // Delimiters
        comma,          // ,
        semicolon,      // ;
        colon,          // :
        l_paren,        // (
        r_paren,        // )
        l_brace,        // {  (Replaces begin)
        r_brace,        // }  (Replaces end)
        l_square,       // [  (For arrays)
        r_square,       // ]  (For arrays)

        // Standard Operators (Used in expressions like print(2*5))
        assign,         // =
        plus,           // +
        minus,          // -
        star,           // *
        slash,          // /
        mod,            // %

        // Comparison Operators
        eq,             // ==
        neq,            // !=
        gt,             // >
        lt,             // <
        gte,            // >=
        lte,            // <=

        // Logical Operators (For conditions: &&, ||)
        land,           // &&
        lor,            // ||

        // --- NEW KEYWORDS (Phase 1 Requirements) ---

        // Data Types & Declarations
        KW_var,         // var
        KW_int,         // int
        KW_float,       // float
        KW_bool,        // bool
        KW_array,       // array
        KW_true,        // true
        KW_false,       // false

        // Arithmetic Statements (Prefix style)
        KW_ADD,         // ADD
        KW_SUB,         // SUB
        KW_MUL,         // MUL
        KW_DIV,         // DIV
        KW_MOD,         // MOD
        KW_INC,         // INC
        KW_DEC,         // DEC
        KW_PLE,         // PLE
        KW_MIE,         // MIE

        // Bitwise/Logical Statements (Prefix style)
        KW_AND,         // AND
        KW_OR,          // OR

        // Control Flow
        KW_if,          // if
        KW_elif,        // elif
        KW_else,        // else
        KW_for,         // for
        KW_foreach,     // foreach
        KW_in,          // in
        KW_match,       // match

        // Built-in Functions
        KW_print,       // print
        KW_to_int,      // to_int
        KW_to_float,    // to_float
        KW_to_bool,     // to_bool
        KW_abs,         // abs
        KW_length,      // length
        KW_max,         // max
        KW_index,       // index
        KW_find         // find
    };

private:
    TokenKind Kind;
    llvm::StringRef Text;

public:
    TokenKind getKind() const { return Kind; }
    llvm::StringRef getText() const { return Text; }

    bool is(TokenKind K) const { return Kind == K; }
    bool isOneOf(TokenKind K1, TokenKind K2) const { return is(K1) || is(K2); }
    template <typename... Ts>
    bool isOneOf(TokenKind K1, TokenKind K2, Ts... Ks) const { return is(K1) || isOneOf(K2, Ks...); }
};

class Lexer
{
    const char *BufferStart;
    const char *BufferPtr;

public:
    Lexer(const llvm::StringRef &Buffer)
    {
        BufferStart = Buffer.begin();
        BufferPtr = BufferStart;
    }

    void next(Token &token);

private:
    void formToken(Token &Result, const char *TokEnd, Token::TokenKind Kind);
};

#endif