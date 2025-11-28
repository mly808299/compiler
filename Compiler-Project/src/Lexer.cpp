#include "Lexer.h"

namespace charinfo
{
    LLVM_READNONE inline bool isWhitespace(char c)
    {
        return c == ' ' || c == '\t' || c == '\f' || c == '\v' ||
               c == '\r' || c == '\n';
    }

    LLVM_READNONE inline bool isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    LLVM_READNONE inline bool isLetter(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    // Updated to include & and | for logical operators
    LLVM_READNONE inline bool isSpecialSign(char c)
    {
        return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
               c == '=' || c == '<' || c == '>' || c == '!' || c == '&' || c == '|';
    }
}

void Lexer::next(Token &token)
{
    // 1. Skip Whitespace
    while (*BufferPtr && charinfo::isWhitespace(*BufferPtr))
    {
        ++BufferPtr;
    }

    // 2. Handle Multi-line Comments /* ... */
    if (*BufferPtr == '/' && *(BufferPtr + 1) == '*')
    {
        BufferPtr += 2; // Skip /*
        while (*BufferPtr && !(*BufferPtr == '*' && *(BufferPtr + 1) == '/'))
        {
            ++BufferPtr;
        }
        if (*BufferPtr)
            BufferPtr += 2; // Skip */

        // Recursively call next to get the real token after comment
        next(token);
        return;
    }

    // 3. Check End of Input
    if (!*BufferPtr)
    {
        token.Kind = Token::eoi;
        return;
    }

    // 4. Identifiers & Keywords
    if (charinfo::isLetter(*BufferPtr))
    {
        const char *end = BufferPtr + 1;
        while (charinfo::isLetter(*end) || charinfo::isDigit(*end))
            ++end;

        llvm::StringRef Name(BufferPtr, end - BufferPtr);
        Token::TokenKind kind = Token::ident;

        // Keyword Mapping
        if (Name == "var") kind = Token::KW_var;
        else if (Name == "int") kind = Token::KW_int;
        else if (Name == "float") kind = Token::KW_float;
        else if (Name == "bool") kind = Token::KW_bool;
        else if (Name == "array") kind = Token::KW_array;
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
        else if (Name == "to_int") kind = Token::KW_to_int;
        else if (Name == "to_float") kind = Token::KW_to_float;
        else if (Name == "to_bool") kind = Token::KW_to_bool;
        else if (Name == "abs") kind = Token::KW_abs;
        else if (Name == "length") kind = Token::KW_length;
        else if (Name == "max") kind = Token::KW_max;
        else if (Name == "index") kind = Token::KW_index;
        else if (Name == "find") kind = Token::KW_find;

        formToken(token, end, kind);
        return;
    }

    // 5. Numbers (Integers)
    else if (charinfo::isDigit(*BufferPtr))
    {
        const char *end = BufferPtr + 1;
        while (charinfo::isDigit(*end))
            ++end;
        // Note: Floating point logic can be added here if needed (checking for '.')
        formToken(token, end, Token::number);
        return;
    }

    // 6. Operators & Symbols
    else
    {
        // Check for multi-character tokens first
        if (*BufferPtr == '=' && *(BufferPtr+1) == '=') {
             formToken(token, BufferPtr + 2, Token::eq); return;
        }
        if (*BufferPtr == '!' && *(BufferPtr+1) == '=') {
             formToken(token, BufferPtr + 2, Token::neq); return;
        }
        if (*BufferPtr == '>' && *(BufferPtr+1) == '=') {
             formToken(token, BufferPtr + 2, Token::gte); return;
        }
        if (*BufferPtr == '<' && *(BufferPtr+1) == '=') {
             formToken(token, BufferPtr + 2, Token::lte); return;
        }
        if (*BufferPtr == '&' && *(BufferPtr+1) == '&') {
             formToken(token, BufferPtr + 2, Token::land); return;
        }
        if (*BufferPtr == '|' && *(BufferPtr+1) == '|') {
             formToken(token, BufferPtr + 2, Token::lor); return;
        }

        // Single-character tokens
        switch (*BufferPtr)
        {
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

            default:
                formToken(token, BufferPtr + 1, Token::unknown);
        }
        return;
    }
}

void Lexer::formToken(Token &Tok, const char *TokEnd, Token::TokenKind Kind)
{
    Tok.Kind = Kind;
    Tok.Text = llvm::StringRef(BufferPtr, TokEnd - BufferPtr);
    BufferPtr = TokEnd;
}