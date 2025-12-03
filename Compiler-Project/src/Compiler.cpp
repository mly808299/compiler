#include "CodeGen.h"
#include "Parser.h"
#include "Sema.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/raw_ostream.h"

// دریافت ورودی از آرگومان‌های خط فرمان
static llvm::cl::opt<std::string>
    Input(llvm::cl::Positional,
          llvm::cl::desc("<input expression>"),
          llvm::cl::init(""));

int main(int argc, const char **argv)
{
    llvm::InitLLVM X(argc, argv);
    llvm::cl::ParseCommandLineOptions(argc, argv, "Simple Compiler\n");

    // 1. Lexer & Parser
    Lexer Lex(Input);
    Parser Parser(Lex);

    Block *Tree = Parser.parse();

    if (!Tree || Parser.hasError())
    {
        llvm::errs() << "Syntax errors occurred\n";
        return 1;
    }

    // ===== حذف saveAst برای جلوگیری از خطا =====
    // saveAst(Tree, "ast.json");

    // 2. Semantic Analysis
    Sema Semantic;
    if (Semantic.semantic(Tree, Input))
    {
        llvm::errs() << "Semantic errors occurred\n";
        return 1;
    }

    // 3. Code Generation
    CodeGen CodeGenerator;
    CodeGenerator.compile(Tree);

    return 0;
}
