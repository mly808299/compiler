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

    void error() {
        llvm::errs() << "Parser Error: Unexpected token '" << Tok.getText() << "' at line " << Tok.getLine() << "\n";
        HasError = true;
    }

    void advance() { Lex.next(Tok); }

    bool expect(Token::TokenKind Kind) {
        if (Tok.getKind() != Kind) {
            error();
            return true;
        }
        return false;
    }

    bool consume(Token::TokenKind Kind) {
        if (Tok.getKind() == Kind) {
            advance();
            return true;
        }
        return false;
    }

public:
    Parser(Lexer &Lex) : Lex(Lex), HasError(false) {
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
    Expr *parseExpr();        // Logical OR (||) - پایین‌ترین اولویت
    Expr *parseLogicAnd();    // Logical AND (&&)
    Expr *parseEquality();    // Equality (==, !=)
    Expr *parseRelational();  // Relational (<, >, <=, >=)
    Expr *parseAdditive();    // Additive (+, -)
    Expr *parseTerm();        // Multiplicative (*, /, %) - بالاترین اولویت ریاضی
    Expr *parseFactor();      // پرانتز، اعداد، متغیرها
    Expr *parseFinal();
};

#endif