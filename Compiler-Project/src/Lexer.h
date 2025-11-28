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
        eoi, unknown,
        ident, number, string_literal,
        comma, semicolon, colon, underscore,
        l_paren, r_paren, l_brace, r_brace, l_square, r_square,
        assign, plus, minus, star, slash, mod,
        eq, neq, gt, lt, gte, lte,
        land, lor, not_op,
        KW_var, KW_int, KW_float, KW_bool, KW_array, KW_string, KW_true, KW_false,
        KW_ADD, KW_SUB, KW_MUL, KW_DIV, KW_MOD,
        KW_INC, KW_DEC, KW_PLE, KW_MIE,
        KW_AND, KW_OR,
        KW_if, KW_elif, KW_else, KW_for, KW_foreach, KW_in, KW_match,
        KW_print, KW_length, KW_index, KW_max, KW_abs, KW_find,
        KW_to_int, KW_to_float, KW_to_bool
    };

private:
    TokenKind Kind;
    llvm::StringRef Text;
    int Line;
    int Col; // شماره ستون

public:
    TokenKind getKind() const { return Kind; }
    llvm::StringRef getText() const { return Text; }
    int getLine() const { return Line; }
    int getCol() const { return Col; }

    bool is(TokenKind K) const { return Kind == K; }
    bool isOneOf(TokenKind K1, TokenKind K2) const { return is(K1) || is(K2); }
    template <typename... Ts>
    bool isOneOf(TokenKind K1, TokenKind K2, Ts... Ks) const { return is(K1) || isOneOf(K2, Ks...); }
};

class Lexer
{
    const char *BufferStart; // شروع کل فایل
    const char *BufferPtr;   // مکان فعلی
    int CurrentLine = 1;

public:
    Lexer(const llvm::StringRef &Buffer)
    {
        BufferStart = Buffer.begin();
        BufferPtr = BufferStart;
        CurrentLine = 1;
    }

    void next(Token &token);

private:
    void formToken(Token &Result, const char *TokEnd, Token::TokenKind Kind);
    // تابع جدید برای محاسبه دقیق ستون
    int calculateColumn(const char *TokenStart);
};

#endif