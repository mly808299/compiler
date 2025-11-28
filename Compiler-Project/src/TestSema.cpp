#include "Sema.h"
#include "Parser.h"
#include "Lexer.h"
#include "llvm/Support/raw_ostream.h"

void runTest(const char* testName, llvm::StringRef input, bool expectError) {
    llvm::outs() << "--------------------------------------------------\n";
    llvm::outs() << "Running Test: " << testName << "\n";
    llvm::outs() << "Code:\n" << input << "\n";

    Lexer L(input);
    Parser P(L);
    AST *Tree = P.parse();

    if (P.hasError() || !Tree) {
        llvm::errs() << "[Parser Failed] - Cannot run semantic check.\n";
        return;
    }

    Sema S;
    bool hasSemanticError = S.semantic(Tree);

    if (hasSemanticError && expectError) {
        llvm::outs() << "RESULT: PASS (Error detected as expected)\n";
    } else if (!hasSemanticError && !expectError) {
        llvm::outs() << "RESULT: PASS (No error, code is valid)\n";
    } else if (hasSemanticError && !expectError) {
        llvm::outs() << "RESULT: FAIL (Unexpected error detected)\n";
    } else {
        llvm::outs() << "RESULT: FAIL (Expected error but none found)\n";
    }
}

int main() {
    // 1. Valid Code
    runTest("Valid Declaration & Assignment",
        "var x int = 10; var y int; ADD y x 5;",
        false);

    // 2. Undeclared Variable
    runTest("Undeclared Variable Usage",
        "var x int = 10; ADD y x 5;",
        true);

    // 3. Redeclaration
    runTest("Redeclaration of Variable",
        "var x int; var x float;",
        true);

    // 4. Type Mismatch (Int vs Bool)
    runTest("Type Mismatch Assignment",
        "var x int; var b bool = true; ADD x b 5;",
        true);

    // 5. Division by Zero
    runTest("Division By Zero Literal",
        "var x int = 10; DIV x 10 0;",
        true);

    // 6. Logical Operator Type Check
    runTest("Logical Operator on Int",
        "var x int = 5; var y int = 6; AND x y y;",
        true);

    // 7. If Condition Check
    runTest("If Condition Non-Bool",
        "var x int = 5; if (x) { print(x); }",
        true);

    // 8. Valid If Condition
    runTest("Valid If Condition",
        "var x int = 5; if (x > 2) { print(x); }",
        false);

    return 0;
}