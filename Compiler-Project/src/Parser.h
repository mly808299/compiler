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
        advance(); // Load first token
    }

    bool hasError() const { return HasError; }
    Block *parse(); // Returns the main program block

private:
    AST *parseStatement();
    Block *parseBlock();

    Declaration *parseDeclaration();      // var x int = ...
    Declaration *parseArrayDeclaration(); // array arr = ...

    AST *parseMathCommand(); // ADD, SUB, PLE, INC ...

    IfStmt *parseIf();
    AST *parseLoop(); // Handles both for and foreach
    MatchStmt *parseMatch(); // Handles match statement
    PrintStmt *parsePrint();

    Expr *parseExpr();
    Expr *parseTerm();
    Expr *parseFactor();
    Expr *parseFinal(); // Handles numbers, idents, calls, list comprehensions
};

#endif