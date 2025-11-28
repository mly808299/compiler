#include "Lexer.h"

namespace charinfo
{
    LLVM_READNONE inline bool isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\f' || c == '\v' ||
               c == '\r' || c == '\n';
    }
    LLVM_READNONE inline bool isDigit(char c) {
        return c >= '0' && c <= '9';
    }
    LLVM_READNONE inline bool isLetter(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }
}

// تابع جادویی برای پیدا کردن شماره ستون
int Lexer::calculateColumn(const char *TokenStart) {
    const char *LineStart = TokenStart;
    // به عقب برمی‌گردیم تا به کاراکتر خط جدید (\n) یا شروع فایل برسیم
    while (LineStart > BufferStart && *(LineStart - 1) != '\n' && *(LineStart - 1) != '\r') {
        --LineStart;
    }
    // فاصله توکن تا شروع خط + 1 می‌شود شماره ستون
    return (TokenStart - LineStart) + 1;
}

void Lexer::next(Token &token)
{
    while (*BufferPtr && charinfo::isWhitespace(*BufferPtr)) {
        if (*BufferPtr == '\n') {
            CurrentLine++;
        }
        ++BufferPtr;
    }

    if (!*BufferPtr) {
        token.Kind = Token::eoi;
        return;
    }

    // ذخیره مکان شروع توکن قبل از اینکه BufferPtr جلو برود
    const char *TokenStart = BufferPtr;

    // Handle Multi-line Comments
    if (*BufferPtr == '/' && *(BufferPtr + 1) == '*') {
        BufferPtr += 2;
        while (*BufferPtr && !(*BufferPtr == '*' && *(BufferPtr + 1) == '/')) {
            if (*BufferPtr == '\n') CurrentLine++;
            ++BufferPtr;
        }
        if (*BufferPtr) BufferPtr += 2;
        next(token);
        return;
    }

    // Handle Single Line Comments
    if (*BufferPtr == '/' && *(BufferPtr + 1) == '/') {
        BufferPtr += 2;
        while (*BufferPtr && *BufferPtr != '\n' && *BufferPtr != '\r') {
            ++BufferPtr;
        }
        next(token);
        return;
    }

    if (*BufferPtr == '"') {
        const char *end = BufferPtr + 1;
        while (*end && *end != '"') ++end;
        if (*end == '"') ++end;
        // بازنشانی بافر پوینتر موقت برای formToken
        BufferPtr = TokenStart;
        formToken(token, end, Token::string_literal);
        return;
    }

    if (charinfo::isLetter(*BufferPtr)) {
        const char *end = BufferPtr + 1;
        while (charinfo::isLetter(*end) || charinfo::isDigit(*end) || *end == '_')
            ++end;

        llvm::StringRef Name(BufferPtr, end - BufferPtr);
        Token::TokenKind kind = Token::ident;

        if (Name == "var") kind = Token::KW_var;
        else if (Name == "int") kind = Token::KW_int;
        else if (Name == "float") kind = Token::KW_float;
        else if (Name == "bool") kind = Token::KW_bool;
        else if (Name == "array") kind = Token::KW_array;
        else if (Name == "string") kind = Token::KW_string;
        else if (Name == "true") kind = Token::KW_true;
        else if (Name == "false") kind = Token::KW_false;

        else if (Name == "ADD") kind = Token::KW_ADD;
        else if (Name == "SUB") kind = Token::KW_SUB;
        else if (Name == "MUL") kind = Token::KW_MUL;
        else if (Name == "DIV") kind = Token::KW_DIV;
        else if (Name == "MOD") kind = Token::KW_MOD;
        else if (Name == "INC") kind = Token::KW_INC;
        else if (Name == "DEC") kind = Token::KW_DEC;
        else if (Name == "PLE") kind = Token::KW_PLE;
        else if (Name == "MIE") kind = Token::KW_MIE;
        else if (Name == "AND") kind = Token::KW_AND;
        else if (Name == "OR") kind = Token::KW_OR;

        else if (Name == "if") kind = Token::KW_if;
        else if (Name == "elif") kind = Token::KW_elif;
        else if (Name == "else") kind = Token::KW_else;
        else if (Name == "for") kind = Token::KW_for;
        else if (Name == "foreach") kind = Token::KW_foreach;
        else if (Name == "in") kind = Token::KW_in;
        else if (Name == "match") kind = Token::KW_match;

        else if (Name == "print") kind = Token::KW_print;
        else if (Name == "length") kind = Token::KW_length;
        else if (Name == "index") kind = Token::KW_index;
        else if (Name == "max") kind = Token::KW_max;
        else if (Name == "abs") kind = Token::KW_abs;
        else if (Name == "find") kind = Token::KW_find;

        else if (Name == "to_int") kind = Token::KW_to_int;
        else if (Name == "to_float") kind = Token::KW_to_float;
        else if (Name == "to_bool") kind = Token::KW_to_bool;

        BufferPtr = TokenStart; // برگرداندن به شروع توکن برای محاسبه صحیح
        formToken(token, end, kind);
        return;
    }

    if (charinfo::isDigit(*BufferPtr)) {
        const char *end = BufferPtr + 1;
        while (charinfo::isDigit(*end)) ++end;
        if (*end == '.') {
            const char *decimalEnd = end + 1;
            if (charinfo::isDigit(*decimalEnd)) {
                while (charinfo::isDigit(*decimalEnd)) ++decimalEnd;
                end = decimalEnd;
            }
        }
        BufferPtr = TokenStart;
        formToken(token, end, Token::number);
        return;
    }

    if (*BufferPtr == '=' && *(BufferPtr+1) == '=') { BufferPtr=TokenStart; formToken(token, BufferPtr + 2, Token::eq); return; }
    if (*BufferPtr == '!' && *(BufferPtr+1) == '=') { BufferPtr=TokenStart; formToken(token, BufferPtr + 2, Token::neq); return; }
    if (*BufferPtr == '>' && *(BufferPtr+1) == '=') { BufferPtr=TokenStart; formToken(token, BufferPtr + 2, Token::gte); return; }
    if (*BufferPtr == '<' && *(BufferPtr+1) == '=') { BufferPtr=TokenStart; formToken(token, BufferPtr + 2, Token::lte); return; }
    if (*BufferPtr == '&' && *(BufferPtr+1) == '&') { BufferPtr=TokenStart; formToken(token, BufferPtr + 2, Token::land); return; }
    if (*BufferPtr == '|' && *(BufferPtr+1) == '|') { BufferPtr=TokenStart; formToken(token, BufferPtr + 2, Token::lor); return; }

    BufferPtr = TokenStart; // برای سوییچ کیس تک حرفی
    switch (*BufferPtr) {
        case '=': formToken(token, BufferPtr + 1, Token::assign); break;
        case '+': formToken(token, BufferPtr + 1, Token::plus); break;
        case '-': formToken(token, BufferPtr + 1, Token::minus); break;
        case '*': formToken(token, BufferPtr + 1, Token::star); break;
        case '/': formToken(token, BufferPtr + 1, Token::slash); break;
        case '%': formToken(token, BufferPtr + 1, Token::mod); break;
        case '(': formToken(token, BufferPtr + 1, Token::l_paren); break;
        case ')': formToken(token, BufferPtr + 1, Token::r_paren); break;
        case '{': formToken(token, BufferPtr + 1, Token::l_brace); break;
        case '}': formToken(token, BufferPtr + 1, Token::r_brace); break;
        case '[': formToken(token, BufferPtr + 1, Token::l_square); break;
        case ']': formToken(token, BufferPtr + 1, Token::r_square); break;
        case ',': formToken(token, BufferPtr + 1, Token::comma); break;
        case ';': formToken(token, BufferPtr + 1, Token::semicolon); break;
        case ':': formToken(token, BufferPtr + 1, Token::colon); break;
        case '>': formToken(token, BufferPtr + 1, Token::gt); break;
        case '<': formToken(token, BufferPtr + 1, Token::lt); break;
        case '_': formToken(token, BufferPtr + 1, Token::underscore); break;
        default:  formToken(token, BufferPtr + 1, Token::unknown);
    }
}

void Lexer::formToken(Token &Tok, const char *TokEnd, Token::TokenKind Kind)
{
    Tok.Kind = Kind;
    Tok.Text = llvm::StringRef(BufferPtr, TokEnd - BufferPtr);
    Tok.Line = CurrentLine;

    // محاسبه ستون: شروع توکن (BufferPtr) کجاست؟
    Tok.Col = calculateColumn(BufferPtr);

    BufferPtr = TokEnd;
}