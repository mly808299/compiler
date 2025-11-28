#include "Lexer.h"
#include <iostream>
#include <string>

// یک تابع کمکی برای تبدیل نوع توکن به متن (برای خوانایی خروجی)
std::string getTokenName(Token::TokenKind kind) {
    switch (kind) {
        case Token::ident: return "Identifier";
        case Token::number: return "Number";
        case Token::string_literal: return "String";
        case Token::underscore: return "UNDERSCORE (_)";
        case Token::KW_var: return "Keyword: var";
        case Token::KW_ADD: return "Keyword: ADD";
        case Token::KW_match: return "Keyword: match";
        case Token::KW_foreach: return "Keyword: foreach";
        case Token::KW_to_int: return "TypeCast: to_int";
        case Token::assign: return "Operator: =";
        case Token::l_brace: return "{";
        case Token::r_brace: return "}";
        default: return "Other/Symbol";
    }
}

int main() {
    // 1. ورودی تستی شامل تمام ویژگی‌های جدید زبان
    llvm::StringRef inputCode =
        "/* Phase 1 Test Code */\n"          // خط 1: کامنت چند خطی
        "var x int = 10;\n"                  // خط 2: تعریف متغیر
        "// This is a single line comment\n" // خط 3: کامنت تک خطی (باید نادیده گرفته شود)
        "\n"                                 // خط 4: خط خالی
        "ADD res x 5;\n"                     // خط 5: دستور ریاضی جدید
        "to_int(3.14);\n"                    // خط 6: تبدیل نوع
        "\n"
        "match x {\n"                        // خط 8: پترن مچینگ
        "    1 -> print(\"One\");\n"         // خط 9
        "    _ -> print(\"Default\");\n"     // خط 10: تست آندرلاین (_)
        "}";

    // 2. ساخت لکسر
    Lexer Lex(inputCode);
    Token tok;

    std::cout << "---------------------------------------------------" << std::endl;
    std::cout << " Line |   Token Text   |      Token Type" << std::endl;
    std::cout << "---------------------------------------------------" << std::endl;

    // 3. حلقه پردازش توکن‌ها
    while (true) {
        Lex.next(tok);

        // اگر به پایان رسیدیم، خارج شو
        if (tok.is(Token::eoi)) break;

        // چاپ اطلاعات توکن
        std::cout << "  " << tok.getLine() << "   |   "
                  << tok.getText().str();

        // تنظیم فاصله برای زیبایی خروجی
        int padding = 15 - tok.getText().str().length();
        for(int i=0; i<padding; i++) std::cout << " ";

        std::cout << "| " << getTokenName(tok.getKind()) << std::endl;
    }

    std::cout << "---------------------------------------------------" << std::endl;
    std::cout << "Test Finished Successfully!" << std::endl;

    return 0;
}