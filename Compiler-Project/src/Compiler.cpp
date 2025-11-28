#include "CodeGen.h"
#include "Parser.h"
#include "Sema.h"
// #include "ASTDumper.h" // اضافه شد برای تولید JSON
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
// #include "llvm/Support/FileSystem.h" // اضافه شد برای نوشتن فایل
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

    // // 2. Observability: تولید فایل ast.json
    // std::error_code EC;
    // llvm::raw_fd_ostream JsonFile("ast.json", EC);
    // if (!EC) {
    //     ASTDumper Dumper(JsonFile);
    //     Dumper.dump(Tree);
    //     // llvm::outs() << "AST dumped to ast.json\n"; // (اختیاری: چاپ پیام موفقیت)
    // } else {
    //     llvm::errs() << "Could not create ast.json: " << EC.message() << "\n";
    // }

    // 3. Semantic Analysis
    Sema Semantic;
    // تغییر مهم: پاس دادن متن کد (Input) برای نمایش خطای زیبا
    if (Semantic.semantic(Tree, Input))
    {
        llvm::errs() << "Semantic errors occurred\n";
        return 1;
    }

    // 4. Code Generation
    CodeGen CodeGenerator;
    CodeGenerator.compile(Tree);

    return 0;
}