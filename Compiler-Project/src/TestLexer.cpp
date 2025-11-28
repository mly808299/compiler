#include "Lexer.h"
#include "llvm/Support/raw_ostream.h"

int main() {
    // نمونه کد زبان جدید
    llvm::StringRef Input =
        "var x int = 10;\n"
        "ADD x 5 2;\n"
        "if (x > 10) { print(x); }";

    Lexer L(Input);
    Token Tok;

    llvm::outs() << "Testing Lexer...\n";
    while (true) {
        L.next(Tok);
        if (Tok.getKind() == Token::eoi) break;

        // چاپ نوع و متن توکن
        llvm::outs() << "Token Kind: " << Tok.getKind()
                     << ", Text: '" << Tok.getText() << "'\n";
    }
    return 0;
}