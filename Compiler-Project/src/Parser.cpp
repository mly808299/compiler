#include "Parser.h"

Block *Parser::parse() {
    Block *ProgramBlock = new Block(0, 0);
    while (Tok.getKind() != Token::eoi) {
        AST *Stmt = parseStatement();
        if (Stmt) {
            ProgramBlock->addStatement(Stmt);
        } else {
            // Error recovery: skip until valid start of statement
            if (Tok.getKind() != Token::eoi) advance();
        }
    }
    return ProgramBlock;
}

Block *Parser::parseBlock() {
    int L = Tok.getLine(), C = Tok.getCol();
    if (!consume(Token::l_brace)) { error(); return nullptr; }
    Block *B = new Block(L, C);
    while (!Tok.is(Token::r_brace) && !Tok.is(Token::eoi)) {
        AST *Stmt = parseStatement();
        if (Stmt) B->addStatement(Stmt);
        else advance();
    }
    if (Tok.is(Token::eoi)) {
        llvm::errs() << "Parser Error: Missing '}' at end of file\n";
        HasError = true;
    } else {
        consume(Token::r_brace);
    }
    return B;
}

AST *Parser::parseStatement() {
    switch (Tok.getKind()) {
        case Token::KW_var: return parseDeclaration();
        case Token::KW_array: return parseArrayDeclaration();
        case Token::KW_if: return parseIf();
        case Token::KW_for:
        case Token::KW_foreach: return parseLoop();
        case Token::KW_match: return parseMatch();
        case Token::KW_print: return parsePrint();
        case Token::l_brace: return parseBlock();

            // Math commands
        case Token::KW_ADD: case Token::KW_SUB: case Token::KW_MUL: case Token::KW_DIV:
        case Token::KW_MOD: case Token::KW_INC: case Token::KW_DEC: case Token::KW_PLE:
        case Token::KW_MIE: case Token::KW_AND: case Token::KW_OR:
            return parseMathCommand();

        case Token::ident: {
            int L = Tok.getLine(), C = Tok.getCol();
            llvm::StringRef Name = Tok.getText();
            advance();
            Expr *Index = nullptr;
            if (consume(Token::l_square)) {
                Index = parseExpr();
                if (!consume(Token::r_square)) { error(); return nullptr; }
            }
            if (consume(Token::plus_plus)) {
                consume(Token::semicolon);
                return new UnaryStmt(L, C, Name, UnaryStmt::INC, Index);
            }
            if (consume(Token::minus_minus)) {
                consume(Token::semicolon);
                return new UnaryStmt(L, C, Name, UnaryStmt::DEC, Index);
            }
            error(); return nullptr;
        }

            // اگر elif یا else به تنهایی (بدون if) دیده شوند، خطاست
        case Token::KW_elif:
        case Token::KW_else:
            llvm::errs() << "Parser Error: '" << Tok.getText() << "' without matching 'if' at line " << Tok.getLine() << "\n";
            HasError = true;
            return nullptr;

        default: error(); return nullptr;
    }
}

Declaration *Parser::parseDeclaration() {
    advance();
    int L = Tok.getLine(), C = Tok.getCol();
    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Name = Tok.getText();
    advance();
    llvm::StringRef TypeVal;
    if (Tok.isOneOf(Token::KW_int, Token::KW_float, Token::KW_bool, Token::KW_array, Token::KW_string)) {
        TypeVal = Tok.getText();
        advance();
    } else { error(); return nullptr; }
    Expr *Init = nullptr;
    if (consume(Token::assign)) Init = parseExpr();
    consume(Token::semicolon);
    return new Declaration(L, C, Name, TypeVal, Init);
}

Declaration *Parser::parseArrayDeclaration() {
    advance();
    int L = Tok.getLine(), C = Tok.getCol();
    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Name = Tok.getText();
    advance();
    Expr *Init = nullptr;
    if (consume(Token::assign)) Init = parseExpr();
    if (consume(Token::semicolon)) {}
    return new Declaration(L, C, Name, "array", Init);
}

AST *Parser::parseMathCommand() {
    Token::TokenKind OpKind = Tok.getKind();
    advance();
    int L = Tok.getLine(), C = Tok.getCol();
    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Target = Tok.getText();
    advance();
    Expr *IndexExpr = nullptr;
    if (consume(Token::l_square)) {
        IndexExpr = parseExpr();
        if (!consume(Token::r_square)) { error(); return nullptr; }
    }
    if (OpKind == Token::KW_INC || OpKind == Token::KW_DEC) {
        consume(Token::semicolon);
        UnaryStmt::OpType Op = (OpKind == Token::KW_INC) ? UnaryStmt::INC : UnaryStmt::DEC;
        return new UnaryStmt(L, C, Target, Op, IndexExpr);
    }
    if (OpKind == Token::KW_PLE || OpKind == Token::KW_MIE) {
        Expr *Val = parseExpr();
        consume(Token::semicolon);
        CompoundStmt::OpType Op = (OpKind == Token::KW_PLE) ? CompoundStmt::PLE : CompoundStmt::MIE;
        return new CompoundStmt(L, C, Target, Val, Op, IndexExpr);
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
    return new Assignment(L, C, Target, new BinaryOp(L, C, BinOp, Op1, Op2), IndexExpr);
}

// --- اصلاح شده: تابع parseIf با پشتیبانی صحیح از elif و else if ---
IfStmt *Parser::parseIf() {
    int L = Tok.getLine(), C = Tok.getCol();
    advance(); // skip 'if'

    if (consume(Token::l_paren)) {}
    Expr *Cond = parseExpr();
    if (consume(Token::r_paren)) {}

    Block *Then = parseBlock();

    llvm::SmallVector<std::pair<Expr*, Block*>, 4> Elifs;
    Block *Else = nullptr;

    while (true) {
        // حالت ۱: elif (...) { ... }
        if (Tok.is(Token::KW_elif)) {
            advance(); // consume 'elif'
            if (consume(Token::l_paren)) {}
            Expr *ElifCond = parseExpr();
            if (consume(Token::r_paren)) {}
            Block *ElifBlock = parseBlock();
            Elifs.push_back({ElifCond, ElifBlock});
            continue;
        }

        // حالت ۲: else ...
        if (Tok.is(Token::KW_else)) {
            advance(); // consume 'else'

            // حالت ۲-الف: else if (...) { ... }
            if (Tok.is(Token::KW_if)) {
                advance(); // consume 'if'
                if (consume(Token::l_paren)) {}
                Expr *ElifCond = parseExpr();
                if (consume(Token::r_paren)) {}
                Block *ElifBlock = parseBlock();
                Elifs.push_back({ElifCond, ElifBlock});
                continue; // ادامه بده چون شاید باز هم else if باشد
            }
                // حالت ۲-ب: else { ... } (پایان زنجیره)
            else {
                Else = parseBlock();
                break; // تمام شد
            }
        }

        // نه elif بود نه else -> خروج
        break;
    }

    return new IfStmt(L, C, Cond, Then, Elifs, Else);
}
// -----------------------------------------------------------------

MatchStmt *Parser::parseMatch() {
    int L = Tok.getLine(), C = Tok.getCol();
    advance();
    Expr *Target = parseExpr();
    if (!consume(Token::l_brace)) { error(); return nullptr; }
    llvm::SmallVector<std::pair<Expr*, AST*>, 8> Cases;
    while (!Tok.is(Token::r_brace) && !Tok.is(Token::eoi)) {
        Expr *Pattern = nullptr;
        if (Tok.is(Token::underscore)) {
            Pattern = new Final(Tok.getLine(), Tok.getCol(), Final::Underscore, "_");
            advance();
        } else {
            Pattern = parseFinal();
        }
        if (!consume(Token::minus) || !consume(Token::gt)) {
            llvm::errs() << "Parser Error: Expected '->' at line " << Tok.getLine() << "\n";
            HasError = true; return nullptr;
        }
        AST *Body = parseStatement();
        Cases.push_back({Pattern, Body});
        consume(Token::comma);
    }
    consume(Token::r_brace);
    return new MatchStmt(L, C, Target, Cases);
}

AST *Parser::parseLoop() {
    int L = Tok.getLine(), C = Tok.getCol();
    if (Tok.getKind() == Token::KW_for) {
        advance(); consume(Token::l_paren);
        AST *Init = nullptr;
        if (Tok.getKind() == Token::KW_int) {
            int DL = Tok.getLine(), DC = Tok.getCol();
            advance();
            if (expect(Token::ident)) return nullptr;
            llvm::StringRef Name = Tok.getText(); advance();
            if (expect(Token::assign)) return nullptr;
            advance();
            Expr *Val = parseExpr();
            consume(Token::semicolon);
            Init = new Declaration(DL, DC, Name, "int", Val);
        } else {
            Init = parseStatement();
        }
        Expr *Cond = parseExpr();
        consume(Token::semicolon);
        AST *Step = nullptr;
        if (Tok.getKind() == Token::ident) {
            int SL = Tok.getLine(), SC = Tok.getCol();
            llvm::StringRef Name = Tok.getText();
            advance();
            Expr *Index = nullptr;
            if (consume(Token::l_square)) {
                Index = parseExpr();
                consume(Token::r_square);
            }
            if (consume(Token::plus_plus)) {
                Step = new UnaryStmt(SL, SC, Name, UnaryStmt::INC, Index);
            }
            else if (consume(Token::minus_minus)) {
                Step = new UnaryStmt(SL, SC, Name, UnaryStmt::DEC, Index);
            }
        } else {
            Step = parseStatement();
        }
        consume(Token::r_paren);
        Block *Body = parseBlock();
        return new ForStmt(L, C, Init, Cond, Step, Body);
    } else {
        advance(); consume(Token::l_paren);
        if (expect(Token::ident)) return nullptr;
        llvm::StringRef Iter = Tok.getText(); advance();
        if (expect(Token::KW_in)) return nullptr;
        advance();
        if (expect(Token::ident)) return nullptr;
        llvm::StringRef Col = Tok.getText(); advance();
        consume(Token::r_paren);
        Block *Body = parseBlock();
        return new ForEachStmt(L, C, Iter, Col, Body);
    }
}

PrintStmt *Parser::parsePrint() {
    int L = Tok.getLine(), C = Tok.getCol();
    advance(); consume(Token::l_paren);
    Expr *Val = parseExpr();
    consume(Token::r_paren); consume(Token::semicolon);
    return new PrintStmt(L, C, Val);
}

Expr *Parser::parseExpr() {
    Expr *Left = parseLogicAnd();
    while (Tok.is(Token::lor)) {
        int L = Tok.getLine(), C = Tok.getCol();
        advance();
        Expr *Right = parseLogicAnd();
        Left = new BinaryOp(L, C, BinaryOp::Or, Left, Right);
    }
    return Left;
}

Expr *Parser::parseLogicAnd() {
    Expr *Left = parseEquality();
    while (Tok.is(Token::land)) {
        int L = Tok.getLine(), C = Tok.getCol();
        advance();
        Expr *Right = parseEquality();
        Left = new BinaryOp(L, C, BinaryOp::And, Left, Right);
    }
    return Left;
}

Expr *Parser::parseEquality() {
    Expr *Left = parseRelational();
    while (Tok.isOneOf(Token::eq, Token::neq)) {
        int L = Tok.getLine(), C = Tok.getCol();
        BinaryOp::Operator Op = Tok.is(Token::eq) ? BinaryOp::Eq : BinaryOp::Neq;
        advance();
        Expr *Right = parseRelational();
        Left = new BinaryOp(L, C, Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseRelational() {
    Expr *Left = parseAdditive();
    while (Tok.isOneOf(Token::lt, Token::gt, Token::lte, Token::gte)) {
        int L = Tok.getLine(), C = Tok.getCol();
        BinaryOp::Operator Op;
        if (Tok.is(Token::lt)) Op = BinaryOp::Lt;
        else if (Tok.is(Token::gt)) Op = BinaryOp::Gt;
        else if (Tok.is(Token::lte)) Op = BinaryOp::Lte;
        else Op = BinaryOp::Gte;
        advance();
        Expr *Right = parseAdditive();
        Left = new BinaryOp(L, C, Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseAdditive() {
    Expr *Left = parseTerm();
    while (Tok.isOneOf(Token::plus, Token::minus)) {
        int L = Tok.getLine(), C = Tok.getCol();
        BinaryOp::Operator Op = Tok.is(Token::plus) ? BinaryOp::Plus : BinaryOp::Minus;
        advance();
        Expr *Right = parseTerm();
        Left = new BinaryOp(L, C, Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseTerm() {
    Expr *Left = parseFactor();
    while (Tok.isOneOf(Token::star, Token::slash, Token::mod)) {
        int L = Tok.getLine(), C = Tok.getCol();
        BinaryOp::Operator Op;
        if (Tok.is(Token::star)) Op = BinaryOp::Mul;
        else if (Tok.is(Token::slash)) Op = BinaryOp::Div;
        else Op = BinaryOp::Mod;
        advance();
        Expr *Right = parseFactor();
        Left = new BinaryOp(L, C, Op, Left, Right);
    }
    return Left;
}

Expr *Parser::parseFactor() {
    if (Tok.is(Token::minus)) {
        int L = Tok.getLine(), C = Tok.getCol();
        advance();
        Expr *Right = parseFactor();
        return new BinaryOp(L, C, BinaryOp::Minus, new Final(L, C, Final::Number, "0"), Right);
    }
    return parseFinal();
}

Expr *Parser::parseFinal() {
    int L = Tok.getLine(), C = Tok.getCol();
    if (consume(Token::l_paren)) {
        Expr *E = parseExpr();
        consume(Token::r_paren);
        return E;
    }
    if (Tok.getKind() == Token::number) {
        llvm::StringRef Val = Tok.getText();
        advance();
        if (Val.find('.') != llvm::StringRef::npos) return new Final(L, C, Final::Float, Val);
        return new Final(L, C, Final::Number, Val);
    }
    if (Tok.getKind() == Token::ident) {
        llvm::StringRef Val = Tok.getText();
        advance();
        if (consume(Token::l_square)) {
            Expr *Idx = parseExpr();
            if (!consume(Token::r_square)) { error(); return nullptr; }
            return new ArrayAccess(L, C, Val, Idx);
        }
        return new Final(L, C, Final::Ident, Val);
    }
    if (Tok.getKind() == Token::string_literal) {
        llvm::StringRef Val = Tok.getText(); advance();
        return new Final(L, C, Final::String, Val);
    }
    if (Tok.getKind() == Token::KW_true || Tok.getKind() == Token::KW_false) {
        llvm::StringRef Val = Tok.getText(); advance();
        return new Final(L, C, Final::Bool, Val);
    }
    if (consume(Token::l_square)) {
        Expr *FirstExpr = parseExpr();
        if (consume(Token::KW_for)) {
            if (expect(Token::ident)) return nullptr;
            llvm::StringRef IterName = Tok.getText(); advance();
            if (expect(Token::KW_in)) return nullptr;
            advance();
            if (expect(Token::ident)) return nullptr;
            llvm::StringRef ListName = Tok.getText(); advance();
            Expr *Cond = nullptr;
            if (consume(Token::KW_if)) { Cond = parseExpr(); }
            consume(Token::r_square);
            return new RangeExpr(L, C, FirstExpr, IterName, ListName, Cond);
        }
        else {
            llvm::SmallVector<Expr*, 8> Values;
            Values.push_back(FirstExpr);
            while (consume(Token::comma)) { Values.push_back(parseExpr()); }
            if (!consume(Token::r_square)) { error(); return nullptr; }
            return new ArrayLiteral(L, C, Values);
        }
    }
    if (Tok.isOneOf(Token::KW_length, Token::KW_index, Token::KW_max, Token::KW_abs, Token::KW_find, Token::KW_to_int, Token::KW_to_float, Token::KW_to_bool)) {
        std::string FuncName = Tok.getText().str();
        advance();
        consume(Token::l_paren);
        llvm::SmallVector<Expr*, 4> Args;
        if (!Tok.is(Token::r_paren)) {
            Args.push_back(parseExpr());
            while (consume(Token::comma)) { Args.push_back(parseExpr()); }
        }
        consume(Token::r_paren);
        return new BuiltinCall(L, C, FuncName, Args);
    }
    error(); return nullptr;
}