// #include "Parser.h"
//
// Block *Parser::parse() {
//     Block *ProgramBlock = new Block();
//     while (Tok.getKind() != Token::eoi) {
//         AST *Stmt = parseStatement();
//         if (Stmt) ProgramBlock->addStatement(Stmt);
//         else if (HasError) return nullptr;
//     }
//     return ProgramBlock;
// }
//
// Block *Parser::parseBlock() {
//     if (!consume(Token::l_brace)) { error(); return nullptr; }
//     Block *B = new Block();
//     while (!Tok.is(Token::r_brace) && !Tok.is(Token::eoi)) {
//         B->addStatement(parseStatement());
//     }
//     consume(Token::r_brace);
//     return B;
// }
//
// AST *Parser::parseStatement() {
//     switch (Tok.getKind()) {
//         case Token::KW_var: return parseDeclaration();
//         case Token::KW_if: return parseIf();
//         case Token::KW_for:
//         case Token::KW_foreach: return parseLoop();
//         case Token::KW_print: return parsePrint();
//         case Token::l_brace: return parseBlock();
//
//         case Token::KW_ADD: case Token::KW_SUB: case Token::KW_MUL: case Token::KW_DIV:
//         case Token::KW_MOD: case Token::KW_INC: case Token::KW_DEC: case Token::KW_PLE:
//         case Token::KW_MIE: case Token::KW_AND: case Token::KW_OR:
//             return parseMathCommand();
//
//         default:
//             error(); return nullptr;
//     }
// }
//
// Declaration *Parser::parseDeclaration() {
//     advance(); // var
//     if (expect(Token::ident)) return nullptr;
//     llvm::StringRef Name = Tok.getText();
//     advance();
//
//     llvm::StringRef TypeVal;
//     if (Tok.isOneOf(Token::KW_int, Token::KW_float, Token::KW_bool, Token::KW_array)) {
//         TypeVal = Tok.getText();
//         advance();
//     } else {
//         error(); return nullptr;
//     }
//
//     Expr *Init = nullptr;
//     if (consume(Token::assign)) Init = parseExpr();
//
//     consume(Token::semicolon);
//     return new Declaration(Name, TypeVal, Init);
// }
//
// AST *Parser::parseMathCommand() {
//     Token::TokenKind OpKind = Tok.getKind();
//     advance();
//
//     if (expect(Token::ident)) return nullptr;
//     llvm::StringRef Target = Tok.getText();
//     advance();
//
//     if (OpKind == Token::KW_INC || OpKind == Token::KW_DEC) {
//         consume(Token::semicolon);
//         BinaryOp::Operator Op = (OpKind == Token::KW_INC) ? BinaryOp::Plus : BinaryOp::Minus;
//         return new Assignment(Target, new BinaryOp(Op, new Final(Final::Ident, Target), new Final(Final::Number, "1")));
//     }
//
//     if (OpKind == Token::KW_PLE || OpKind == Token::KW_MIE) {
//         Expr *Val = parseExpr();
//         consume(Token::semicolon);
//         BinaryOp::Operator Op = (OpKind == Token::KW_PLE) ? BinaryOp::Plus : BinaryOp::Minus;
//         return new Assignment(Target, new BinaryOp(Op, new Final(Final::Ident, Target), Val));
//     }
//
//     Expr *Op1 = parseExpr();
//     Expr *Op2 = parseExpr();
//     consume(Token::semicolon);
//
//     BinaryOp::Operator BinOp;
//     switch (OpKind) {
//         case Token::KW_ADD: BinOp = BinaryOp::Plus; break;
//         case Token::KW_SUB: BinOp = BinaryOp::Minus; break;
//         case Token::KW_MUL: BinOp = BinaryOp::Mul; break;
//         case Token::KW_DIV: BinOp = BinaryOp::Div; break;
//         case Token::KW_MOD: BinOp = BinaryOp::Mod; break;
//         case Token::KW_AND: BinOp = BinaryOp::And; break;
//         case Token::KW_OR:  BinOp = BinaryOp::Or; break;
//         default: BinOp = BinaryOp::Plus;
//     }
//     return new Assignment(Target, new BinaryOp(BinOp, Op1, Op2));
// }
//
// IfStmt *Parser::parseIf() {
//     advance(); // if
//     if (consume(Token::l_paren)) {}
//     Expr *Cond = parseExpr();
//     if (consume(Token::r_paren)) {}
//     Block *Then = parseBlock();
//
//     llvm::SmallVector<std::pair<Expr*, Block*>, 4> Elifs;
//     Block *Else = nullptr;
//
//     // Handle elif
//     while (Tok.getKind() == Token::KW_elif) {
//         advance();
//         if (consume(Token::l_paren)) {}
//         Expr *ElifCond = parseExpr();
//         if (consume(Token::r_paren)) {}
//         Block *ElifBlock = parseBlock();
//         Elifs.push_back({ElifCond, ElifBlock});
//     }
//
//     // Handle else
//     if (consume(Token::KW_else)) {
//         Else = parseBlock();
//     }
//
//     return new IfStmt(Cond, Then, Elifs, Else);
// }
//
// AST *Parser::parseLoop() {
//     if (Tok.getKind() == Token::KW_for) {
//         advance(); // for
//         consume(Token::l_paren);
//
//         // Init: var i int = 0;
//         AST *Init = parseStatement();
//
//         // Cond: i < n
//         Expr *Cond = parseExpr();
//         consume(Token::semicolon);
//
//         // Step: INC i
//         AST *Step = parseStatement();
//         // Note: INC i consumes semicolon, but in 'for' syntax it might be tricky.
//         // Assuming input.txt uses "INC i;" as step which consumes semicolon.
//         // If there is an extra paren, consume it.
//         consume(Token::r_paren);
//
//         Block *Body = parseBlock();
//         return new ForStmt(Init, Cond, Step, Body);
//     } else {
//         advance(); // foreach
//         consume(Token::l_paren);
//         if (expect(Token::ident)) return nullptr;
//         llvm::StringRef Iter = Tok.getText(); advance();
//
//         if (expect(Token::KW_in)) return nullptr;
//         advance(); // in
//
//         if (expect(Token::ident)) return nullptr;
//         llvm::StringRef Col = Tok.getText(); advance();
//
//         consume(Token::r_paren);
//         Block *Body = parseBlock();
//         return new ForEachStmt(Iter, Col, Body);
//     }
// }
//
// PrintStmt *Parser::parsePrint() {
//     advance(); // print
//     consume(Token::l_paren);
//     Expr *Val = parseExpr();
//     consume(Token::r_paren);
//     consume(Token::semicolon);
//     return new PrintStmt(Val);
// }
//
// // ---- اصلاح مهم: اضافه کردن عملگرهای مقایسه‌ای به پارسر ----
//
// Expr *Parser::parseExpr() {
//     Expr *Left = parseTerm();
//     // لیست تمام عملگرهایی که می‌توانند بین دو عبارت بیایند
//     while (Tok.isOneOf(Token::plus, Token::minus,
//                        Token::eq, Token::neq,
//                        Token::lt, Token::gt,
//                        Token::lte, Token::gte,
//                        Token::land, Token::lor)) {
//
//         BinaryOp::Operator Op;
//         if (Tok.is(Token::plus)) Op = BinaryOp::Plus;
//         else if (Tok.is(Token::minus)) Op = BinaryOp::Minus;
//         else if (Tok.is(Token::eq)) Op = BinaryOp::Eq;
//         else if (Tok.is(Token::neq)) Op = BinaryOp::Neq;
//         else if (Tok.is(Token::lt)) Op = BinaryOp::Lt;
//         else if (Tok.is(Token::gt)) Op = BinaryOp::Gt;
//         else if (Tok.is(Token::lte)) Op = BinaryOp::Lte; // <=
//         else if (Tok.is(Token::gte)) Op = BinaryOp::Gte; // >=
//         else if (Tok.is(Token::land)) Op = BinaryOp::And; // &&
//         else if (Tok.is(Token::lor)) Op = BinaryOp::Or;   // ||
//
//         advance();
//         Expr *Right = parseTerm();
//         Left = new BinaryOp(Op, Left, Right);
//     }
//     return Left;
// }
//
// Expr *Parser::parseTerm() {
//     Expr *Left = parseFactor();
//     while (Tok.isOneOf(Token::star, Token::slash, Token::mod)) {
//         BinaryOp::Operator Op;
//         if (Tok.is(Token::star)) Op = BinaryOp::Mul;
//         else if (Tok.is(Token::slash)) Op = BinaryOp::Div;
//         else Op = BinaryOp::Mod;
//         advance();
//         Expr *Right = parseFactor();
//         Left = new BinaryOp(Op, Left, Right);
//     }
//     return Left;
// }
//
// Expr *Parser::parseFactor() {
//     return parseFinal();
// }
//
// Expr *Parser::parseFinal() {
//     if (consume(Token::l_paren)) {
//         Expr *E = parseExpr();
//         consume(Token::r_paren);
//         return E;
//     }
//     if (Tok.getKind() == Token::number) {
//         llvm::StringRef Val = Tok.getText(); advance();
//         return new Final(Final::Number, Val);
//     }
//     if (Tok.getKind() == Token::ident) {
//         llvm::StringRef Val = Tok.getText(); advance();
//         return new Final(Final::Ident, Val);
//     }
//     if (Tok.getKind() == Token::KW_true || Tok.getKind() == Token::KW_false) {
//         llvm::StringRef Val = Tok.getText(); advance();
//         return new Final(Final::Bool, Val);
//     }
//     if (consume(Token::l_square)) {
//         llvm::SmallVector<Expr*, 8> Values;
//         if (!Tok.is(Token::r_square)) {
//             Values.push_back(parseExpr());
//             while (consume(Token::comma)) {
//                 Values.push_back(parseExpr());
//             }
//         }
//         if (!consume(Token::r_square)) { error(); return nullptr; }
//         return new ArrayLiteral(Values);
//     }
//     if (Tok.isOneOf(Token::KW_length, Token::KW_index, Token::KW_max, Token::KW_abs)) {
//         std::string FuncName = Tok.getText().str();
//         advance();
//         consume(Token::l_paren);
//         llvm::SmallVector<Expr*, 4> Args;
//         if (!Tok.is(Token::r_paren)) {
//             Args.push_back(parseExpr());
//             while (consume(Token::comma)) {
//                 Args.push_back(parseExpr());
//             }
//         }
//         consume(Token::r_paren);
//         return new BuiltinCall(FuncName, Args);
//     }
//
//     error(); return nullptr;
// }
#include "Parser.h"

Block *Parser::parse() {
    Block *ProgramBlock = new Block();
    while (Tok.getKind() != Token::eoi) {
        AST *Stmt = parseStatement();
        if (Stmt) ProgramBlock->addStatement(Stmt);
        else if (HasError) return nullptr;
    }
    return ProgramBlock;
}

Block *Parser::parseBlock() {
    if (!consume(Token::l_brace)) { error(); return nullptr; }
    Block *B = new Block();
    while (!Tok.is(Token::r_brace) && !Tok.is(Token::eoi)) {
        B->addStatement(parseStatement());
    }
    consume(Token::r_brace);
    return B;
}

AST *Parser::parseStatement() {
    switch (Tok.getKind()) {
        case Token::KW_var: return parseDeclaration();
        case Token::KW_array: return parseArrayDeclaration(); // <--- اضافه شده برای سینتکس جدید
        case Token::KW_if: return parseIf();
        case Token::KW_for:
        case Token::KW_foreach: return parseLoop();
        case Token::KW_print: return parsePrint();
        case Token::l_brace: return parseBlock();

        case Token::KW_ADD: case Token::KW_SUB: case Token::KW_MUL: case Token::KW_DIV:
        case Token::KW_MOD: case Token::KW_INC: case Token::KW_DEC: case Token::KW_PLE:
        case Token::KW_MIE: case Token::KW_AND: case Token::KW_OR:
            return parseMathCommand();

        default:
            error(); return nullptr;
    }
}

// تابع جدید برای پارس کردن: array arr = [1, 2];
Declaration *Parser::parseArrayDeclaration() {
    advance(); // consume 'array'

    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Name = Tok.getText();
    advance();

    Expr *Init = nullptr;
    if (consume(Token::assign)) {
        Init = parseExpr();
    }

    if (expect(Token::semicolon)) return nullptr;
    advance();

    // ایجاد نود Declaration با تایپ 'array'
    return new Declaration(Name, "array", Init);
}

Declaration *Parser::parseDeclaration() {
    advance(); // var
    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Name = Tok.getText();
    advance();

    llvm::StringRef TypeVal;
    if (Tok.isOneOf(Token::KW_int, Token::KW_float, Token::KW_bool, Token::KW_array)) {
        TypeVal = Tok.getText();
        advance();
    } else {
        error(); return nullptr;
    }

    Expr *Init = nullptr;
    if (consume(Token::assign)) Init = parseExpr();

    consume(Token::semicolon);
    return new Declaration(Name, TypeVal, Init);
}

AST *Parser::parseMathCommand() {
    Token::TokenKind OpKind = Tok.getKind();
    advance();

    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Target = Tok.getText();
    advance();

    if (OpKind == Token::KW_INC || OpKind == Token::KW_DEC) {
        consume(Token::semicolon);
        BinaryOp::Operator Op = (OpKind == Token::KW_INC) ? BinaryOp::Plus : BinaryOp::Minus;
        return new Assignment(Target, new BinaryOp(Op, new Final(Final::Ident, Target), new Final(Final::Number, "1")));
    }

    if (OpKind == Token::KW_PLE || OpKind == Token::KW_MIE) {
        Expr *Val = parseExpr();
        consume(Token::semicolon);
        BinaryOp::Operator Op = (OpKind == Token::KW_PLE) ? BinaryOp::Plus : BinaryOp::Minus;
        return new Assignment(Target, new BinaryOp(Op, new Final(Final::Ident, Target), Val));
    }

    Expr *Op1 = parseExpr();
    Expr *Op2 = parseExpr();
    consume(Token::semicolon);

    BinaryOp::Operator BinOp;
    switch (OpKind) {
        case Token::KW_ADD: BinOp = BinaryOp::Plus; break;
        case Token::KW_SUB: BinOp = BinaryOp::Minus; break;
        case Token::KW_MUL: BinOp = BinaryOp::Mul; break;
        case Token::KW_DIV: BinOp = BinaryOp::Div; break;
        case Token::KW_MOD: BinOp = BinaryOp::Mod; break;
        case Token::KW_AND: BinOp = BinaryOp::And; break;
        case Token::KW_OR:  BinOp = BinaryOp::Or; break;
        default: BinOp = BinaryOp::Plus;
    }
    return new Assignment(Target, new BinaryOp(BinOp, Op1, Op2));
}

IfStmt *Parser::parseIf() {
    advance(); // if
    if (consume(Token::l_paren)) {}
    Expr *Cond = parseExpr();
    if (consume(Token::r_paren)) {}
    Block *Then = parseBlock();

    llvm::SmallVector<std::pair<Expr*, Block*>, 4> Elifs;
    while (Tok.getKind() == Token::KW_elif) {
        advance();
        if (consume(Token::l_paren)) {}
        Expr *ElifCond = parseExpr();
        if (consume(Token::r_paren)) {}
        Block *ElifBlock = parseBlock();
        Elifs.push_back({ElifCond, ElifBlock});
    }

    Block *Else = nullptr;
    if (consume(Token::KW_else)) {
        Else = parseBlock();
    }
    return new IfStmt(Cond, Then, Elifs, Else);
}

AST *Parser::parseLoop() {
    if (Tok.getKind() == Token::KW_for) {
        advance(); consume(Token::l_paren);
        AST *Init = parseStatement();
        Expr *Cond = parseExpr();
        consume(Token::semicolon);
        AST *Step = parseStatement();
        consume(Token::r_paren);
        Block *Body = parseBlock();
        return new ForStmt(Init, Cond, Step, Body);
    } else {
        advance(); consume(Token::l_paren);
        if (expect(Token::ident)) return nullptr;
        llvm::StringRef Iter = Tok.getText(); advance();

        if (expect(Token::KW_in)) return nullptr;
        advance(); // in

        if (expect(Token::ident)) return nullptr;
        llvm::StringRef Col = Tok.getText(); advance();

        consume(Token::r_paren);
        Block *Body = parseBlock();
        return new ForEachStmt(Iter, Col, Body);
    }
}

PrintStmt *Parser::parsePrint() {
    advance(); consume(Token::l_paren);
    Expr *Val = parseExpr();
    consume(Token::r_paren); consume(Token::semicolon);
    return new PrintStmt(Val);
}

Expr *Parser::parseExpr() {
    Expr *Left = parseTerm();
    while (Tok.isOneOf(Token::plus, Token::minus, Token::eq, Token::neq, Token::lt, Token::gt, Token::lte, Token::gte)) {
        BinaryOp::Operator Op;
        if (Tok.is(Token::plus)) Op = BinaryOp::Plus;
        else if (Tok.is(Token::minus)) Op = BinaryOp::Minus;
        else if (Tok.is(Token::eq)) Op = BinaryOp::Eq;
        else if (Tok.is(Token::neq)) Op = BinaryOp::Neq;
        else if (Tok.is(Token::lt)) Op = BinaryOp::Lt;
        else if (Tok.is(Token::gt)) Op = BinaryOp::Gt;
        else if (Tok.is(Token::lte)) Op = BinaryOp::Lte;
        else if (Tok.is(Token::gte)) Op = BinaryOp::Gte;
        advance();
        Expr *Right = parseTerm();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseTerm() {
    Expr *Left = parseFactor();
    while (Tok.isOneOf(Token::star, Token::slash, Token::mod)) {
        BinaryOp::Operator Op;
        if (Tok.is(Token::star)) Op = BinaryOp::Mul;
        else if (Tok.is(Token::slash)) Op = BinaryOp::Div;
        else Op = BinaryOp::Mod;
        advance();
        Expr *Right = parseFactor();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseFactor() {
    return parseFinal();
}

Expr *Parser::parseFinal() {
    if (consume(Token::l_paren)) {
        Expr *E = parseExpr();
        consume(Token::r_paren);
        return E;
    }
    if (Tok.getKind() == Token::number) {
        llvm::StringRef Val = Tok.getText(); advance();
        return new Final(Final::Number, Val);
    }
    if (Tok.getKind() == Token::ident) {
        llvm::StringRef Val = Tok.getText(); advance();
        return new Final(Final::Ident, Val);
    }
    if (Tok.getKind() == Token::KW_true || Tok.getKind() == Token::KW_false) {
        llvm::StringRef Val = Tok.getText(); advance();
        return new Final(Final::Bool, Val);
    }
    if (consume(Token::l_square)) {
        llvm::SmallVector<Expr*, 8> Values;
        if (!Tok.is(Token::r_square)) {
            Values.push_back(parseExpr());
            while (consume(Token::comma)) {
                Values.push_back(parseExpr());
            }
        }
        if (!consume(Token::r_square)) { error(); return nullptr; }
        return new ArrayLiteral(Values);
    }
    if (Tok.isOneOf(Token::KW_length, Token::KW_index, Token::KW_max, Token::KW_abs)) {
        std::string FuncName = Tok.getText().str();
        advance();
        consume(Token::l_paren);
        llvm::SmallVector<Expr*, 4> Args;
        if (!Tok.is(Token::r_paren)) {
            Args.push_back(parseExpr());
            while (consume(Token::comma)) {
                Args.push_back(parseExpr());
            }
        }
        consume(Token::r_paren);
        return new BuiltinCall(FuncName, Args);
    }

    error(); return nullptr;
}