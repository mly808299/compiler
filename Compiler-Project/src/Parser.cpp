#include "Parser.h"

Block *Parser::parse() {
    Block *ProgramBlock = new Block(0, 0);
    while (Tok.getKind() != Token::eoi) {
        AST *Stmt = parseStatement();
        if (Stmt) ProgramBlock->addStatement(Stmt);
        else if (HasError) return nullptr;
    }
    return ProgramBlock;
}

Block *Parser::parseBlock() {
    int L = Tok.getLine(), C = Tok.getCol();
    if (!consume(Token::l_brace)) { error(); return nullptr; }
    Block *B = new Block(L, C);
    while (!Tok.is(Token::r_brace) && !Tok.is(Token::eoi)) {
        B->addStatement(parseStatement());
    }
    consume(Token::r_brace);
    return B;
}

AST *Parser::parseStatement() {
    switch (Tok.getKind()) {
        case Token::KW_var: return parseDeclaration();
        case Token::KW_array: return parseArrayDeclaration();
        case Token::KW_if: return parseIf();
        case Token::KW_for:
        case Token::KW_foreach: return parseLoop();
        case Token::KW_match: return parseMatch(); // <--- اضافه شد
        case Token::KW_print: return parsePrint();
        case Token::l_brace: return parseBlock();

        // Math commands (ADD, SUB, PLE, INC...)
        case Token::KW_ADD: case Token::KW_SUB: case Token::KW_MUL: case Token::KW_DIV:
        case Token::KW_MOD: case Token::KW_INC: case Token::KW_DEC: case Token::KW_PLE:
        case Token::KW_MIE: case Token::KW_AND: case Token::KW_OR:
            return parseMathCommand();

        default:
            // اگر دستور با یک شناسه شروع شد (مثلاً تابع صدا زده شد)، فعلاً ارور
            // اما در زبان شما دستورات با کلمات کلیدی شروع می‌شوند.
            error(); return nullptr;
    }
}

Declaration *Parser::parseDeclaration() {
    int L = Tok.getLine(), C = Tok.getCol();
    advance(); // consume 'var'
    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Name = Tok.getText();
    advance();

    llvm::StringRef TypeVal;
    if (Tok.isOneOf(Token::KW_int, Token::KW_float, Token::KW_bool, Token::KW_array, Token::KW_string)) {
        TypeVal = Tok.getText();
        advance();
    } else {
        error(); return nullptr;
    }

    Expr *Init = nullptr;
    if (consume(Token::assign)) Init = parseExpr();

    consume(Token::semicolon);
    return new Declaration(L, C, Name, TypeVal, Init);
}

Declaration *Parser::parseArrayDeclaration() {
    int L = Tok.getLine(), C = Tok.getCol();
    advance(); // consume 'array'
    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Name = Tok.getText();
    advance();

    Expr *Init = nullptr;
    if (consume(Token::assign)) Init = parseExpr(); // می‌تواند ArrayLiteral یا RangeExpr باشد

    if (consume(Token::semicolon)) {} // Optional semicolon for flexibility

    return new Declaration(L, C, Name, "array", Init);
}

// پارس کردن دستورات ریاضی جدید: ADD x y z
AST *Parser::parseMathCommand() {
    int L = Tok.getLine(), C = Tok.getCol();
    Token::TokenKind OpKind = Tok.getKind();
    advance(); // consume operator (ADD, INC...)

    if (expect(Token::ident)) return nullptr;
    llvm::StringRef Target = Tok.getText();
    advance();

    // Unary: INC x, DEC x
    if (OpKind == Token::KW_INC || OpKind == Token::KW_DEC) {
        consume(Token::semicolon);
        UnaryStmt::OpType Op = (OpKind == Token::KW_INC) ? UnaryStmt::INC : UnaryStmt::DEC;
        return new UnaryStmt(L, C, Target, Op);
    }

    // Compound: PLE x 5 (x += 5)
    if (OpKind == Token::KW_PLE || OpKind == Token::KW_MIE) {
        Expr *Val = parseExpr();
        consume(Token::semicolon);
        CompoundStmt::OpType Op = (OpKind == Token::KW_PLE) ? CompoundStmt::PLE : CompoundStmt::MIE;
        return new CompoundStmt(L, C, Target, Val, Op);
    }

    // Binary: ADD x y z (x = y + z)
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
    // ساختار انتساب: Target = Op1 (BinOp) Op2
    return new Assignment(L, C, Target, new BinaryOp(L, C, BinOp, Op1, Op2));
}

IfStmt *Parser::parseIf() {
    int L = Tok.getLine(), C = Tok.getCol();
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
    return new IfStmt(L, C, Cond, Then, Elifs, Else);
}

// Match: match x { 1 -> print("One"), _ -> print("Def") }
// Match: match x { 1 -> print("One"), _ -> print("Def") }
MatchStmt *Parser::parseMatch() {
    int L = Tok.getLine(), C = Tok.getCol();
    advance(); // رد کردن کلمه match
    Expr *Target = parseExpr(); // متغیری که باید بررسی شود (مثلاً x)

    if (!consume(Token::l_brace)) { error(); return nullptr; }

    llvm::SmallVector<std::pair<Expr*, AST*>, 8> Cases;

    // تا زمانی که به } یا پایان فایل نرسیدیم، کیس‌ها را بخوان
    while (!Tok.is(Token::r_brace) && !Tok.is(Token::eoi)) {
        Expr *Pattern = nullptr;

        // حالت اول: پترن پیش‌فرض (_)
        if (Tok.is(Token::underscore)) {
            Pattern = new Final(Tok.getLine(), Tok.getCol(), Final::Underscore, "_");
            advance();
        }
        // حالت دوم: عدد، رشته یا شناسه (تغییر مهم: استفاده از parseFinal)
        else {
            Pattern = parseFinal();
        }

        // بررسی وجود فلش -> (که از دو توکن - و > تشکیل شده)
        if (!consume(Token::minus) || !consume(Token::gt)) {
            llvm::errs() << "Parser Error: Expected '->' at line " << Tok.getLine() << "\n";
            HasError = true;
            return nullptr;
        }

        // دستور سمت راست (Body)
        AST *Body = parseStatement();
        Cases.push_back({Pattern, Body});

        consume(Token::comma); // ویرگول بین کیس‌ها (اختیاری)
    }

    consume(Token::r_brace);
    return new MatchStmt(L, C, Target, Cases);
}

AST *Parser::parseLoop() {
    int L = Tok.getLine(), C = Tok.getCol();
    if (Tok.getKind() == Token::KW_for) {
        advance(); consume(Token::l_paren);
        AST *Init = parseStatement(); // var i int = 0;
        Expr *Cond = parseExpr();
        consume(Token::semicolon);
        AST *Step = parseStatement(); // INC i;
        consume(Token::r_paren);
        Block *Body = parseBlock();
        return new ForStmt(L, C, Init, Cond, Step, Body);
    } else { // foreach
        advance(); consume(Token::l_paren);
        if (expect(Token::ident)) return nullptr;
        llvm::StringRef Iter = Tok.getText(); advance();

        if (expect(Token::KW_in)) return nullptr;
        advance(); // in

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
    Expr *Left = parseTerm();
    while (Tok.isOneOf(Token::plus, Token::minus,
                       Token::eq, Token::neq, Token::lt, Token::gt, Token::lte, Token::gte,
                       Token::land, Token::lor)) {
        int L = Tok.getLine(), C = Tok.getCol();
        BinaryOp::Operator Op;
        if (Tok.is(Token::plus)) Op = BinaryOp::Plus;
        else if (Tok.is(Token::minus)) Op = BinaryOp::Minus;
        else if (Tok.is(Token::eq)) Op = BinaryOp::Eq;
        else if (Tok.is(Token::neq)) Op = BinaryOp::Neq;
        else if (Tok.is(Token::lt)) Op = BinaryOp::Lt;
        else if (Tok.is(Token::gt)) Op = BinaryOp::Gt;
        else if (Tok.is(Token::lte)) Op = BinaryOp::Lte;
        else if (Tok.is(Token::gte)) Op = BinaryOp::Gte;
        else if (Tok.is(Token::land)) Op = BinaryOp::And;
        else if (Tok.is(Token::lor)) Op = BinaryOp::Or;

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
    return parseFinal();
}

Expr *Parser::parseFinal() {
    int L = Tok.getLine(), C = Tok.getCol();

    if (consume(Token::l_paren)) {
        Expr *E = parseExpr();
        consume(Token::r_paren);
        return E;
    }

    // در تابع parseFinal:

    if (Tok.getKind() == Token::number) {
        llvm::StringRef Val = Tok.getText();
        advance();

        // --- تغییر جدید: اگر نقطه داشت، نوعش Float است ---
        if (Val.find('.') != llvm::StringRef::npos) {
            return new Final(L, C, Final::Float, Val);
        }
        // -----------------------------------------------

        return new Final(L, C, Final::Number, Val);
    }

    if (Tok.getKind() == Token::ident) {
        llvm::StringRef Val = Tok.getText(); advance();
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

    // Array Literal: [1, 2, 3] OR Comprehension: [x * 2 for x in list]
    if (consume(Token::l_square)) {
        Expr *FirstExpr = parseExpr();

        // اگر بعد از اولین عبارت، کلمه for آمد، یعنی Comprehension است
        if (consume(Token::KW_for)) {
            // [Expr for Iter in List if Cond]
            if (expect(Token::ident)) return nullptr;
            llvm::StringRef IterName = Tok.getText(); advance();

            if (expect(Token::KW_in)) return nullptr;
            advance();

            if (expect(Token::ident)) return nullptr;
            llvm::StringRef ListName = Tok.getText(); advance();

            Expr *Cond = nullptr;
            if (consume(Token::KW_if)) {
                Cond = parseExpr();
            }
            consume(Token::r_square);
            return new RangeExpr(L, C, FirstExpr, IterName, ListName, Cond);
        }
        else {
            // آرایه معمولی: [1, 2, 3]
            llvm::SmallVector<Expr*, 8> Values;
            Values.push_back(FirstExpr);
            while (consume(Token::comma)) {
                Values.push_back(parseExpr());
            }
            if (!consume(Token::r_square)) { error(); return nullptr; }
            return new ArrayLiteral(L, C, Values);
        }
    }

    // Builtin functions
    if (Tok.isOneOf(Token::KW_length, Token::KW_index, Token::KW_max,
                    Token::KW_abs, Token::KW_find, Token::KW_to_int,
                    Token::KW_to_float, Token::KW_to_bool)) {
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
        return new BuiltinCall(L, C, FuncName, Args);
    }

    error(); return nullptr;
}