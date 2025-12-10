#ifndef PARSER_H
#define PARSER_H

#include "AST.h"
#include "Lexer.h"
#include "llvm/Support/raw_ostream.h"

class Parser
{
    Lexer &Lex;
    Token Tok;
    bool HasError;

    // اضافه شده برای دسترسی به متن اصلی کد جهت چاپ خطا
    llvm::StringRef SourceCode;

    // توابع کمکی (فقط تعریف، پیاده‌سازی در Parser.cpp است)
    void error();
    void printError(int Line, int Col, const llvm::Twine &Msg); // اضافه شده

    void advance();
    bool expect(Token::TokenKind Kind);
    bool consume(Token::TokenKind Kind);

public:
    // سازنده آپدیت شده: InputCode را می‌گیرد
    Parser(Lexer &L, llvm::StringRef InputCode)
            : Lex(L), HasError(false), SourceCode(InputCode) {
        advance();
    }

    bool hasError() const { return HasError; }
    Block *parse();

private:
    AST *parseStatement();
    Block *parseBlock();

    Declaration *parseDeclaration();
    Declaration *parseArrayDeclaration();

    AST *parseMathCommand();

    IfStmt *parseIf();
    AST *parseLoop();
    MatchStmt *parseMatch();
    PrintStmt *parsePrint();

    // --- سلسله مراتب اولویت عملگرها (Precedence Hierarchy) ---
    Expr *parseExpr();        // Logical OR (||)
    Expr *parseLogicAnd();    // Logical AND (&&)
    Expr *parseEquality();    // Equality (==, !=)
    Expr *parseRelational();  // Relational (<, >, <=, >=)
    Expr *parseAdditive();    // Additive (+, -)
    Expr *parseTerm();        // Multiplicative (*, /, %)
    Expr *parseFactor();      // پرانتز، اعداد، متغیرها
    Expr *parseFinal();
};

#endif