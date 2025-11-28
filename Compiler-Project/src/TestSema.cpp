#include "Lexer.h"
#include "Parser.h"
#include "Sema.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <string>

void runTest(std::string testName, llvm::StringRef inputCode, bool expectError) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "TEST: " << testName << std::endl;

    Lexer Lex(inputCode);
    Parser Par(Lex);
    Block *Program = Par.parse();

    if (Par.hasError() || !Program) {
        std::cout << "❌ PARSER ERROR" << std::endl;
        return;
    }

    Sema Semantic;
    bool hasSemanticError = Semantic.semantic(Program);

    if (hasSemanticError == expectError) {
        std::cout << "✅ SUCCESS" << std::endl;
    } else {
        std::cout << "❌ FAILURE: " << (expectError ? "Expected error but got none." : "Unexpected error occurred.") << std::endl;
    }
}

int main() {
    // 1. اصلاح شده: تعریف res قبل از استفاده
    runTest("Valid Code",
        "var x int = 10;\n"
        "var y int = 20;\n"
        "var res int;\n"  // <--- خط جدید
        "ADD res x y;\n"
        "print(res);",
        false
    );

    runTest("Undefined Variable",
        "var x int = 10;\n"
        "var res int;\n" // اضافه شده تا فقط z خطا بدهد
        "ADD res x z;",
        true
    );

    runTest("Redeclaration",
        "var x int = 5;\n"
        "var x bool = true;",
        true
    );

    runTest("Type Mismatch",
        "var x int = true;",
        true
    );

    runTest("Invalid Math",
        "var x bool = true;\n"
        "INC x;",
        true
    );

    // 2. اصلاح شده: تعریف res برای اینکه ارور تقسیم دیده شود
    runTest("Division by Zero",
        "var x int = 10;\n"
        "var res int;\n" // <--- خط جدید
        "DIV res x 0;",
        true
    );

    runTest("Loop Scope",
        "array list = [1, 2];\n"
        "foreach (i in list) {\n"
        "    print(i);\n"
        "}\n"
        "print(i);",
        true
    );

    return 0;
}