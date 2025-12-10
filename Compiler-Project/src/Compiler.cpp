#include "CodeGen.h"
#include "Parser.h"
#include "Sema.h"
#include "ASTDumper.h"
#include "GraphGenerator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <chrono>
#include <iostream>

int AST::NodeCount = 0;

static llvm::cl::opt<std::string>
        Input(llvm::cl::Positional,
              llvm::cl::desc("<input expression>"),
              llvm::cl::init(""));

static std::chrono::high_resolution_clock::time_point ProgramStart;

double getCurrentTimeMs() {
    auto Now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> Elapsed = Now - ProgramStart;
    return Elapsed.count();
}

void printPhaseLog(const std::string &PhaseName, double StartMs, double EndMs) {
    llvm::errs() << "   -> " << PhaseName << ":\n"
                 << "        Start : " << llvm::format("%8.3f", StartMs) << " ms\n"
                 << "        End   : " << llvm::format("%8.3f", EndMs) << " ms\n"
                 << "        Dur   : " << llvm::format("%8.3f", EndMs - StartMs) << " ms\n";
}

int main(int argc, const char **argv)
{
    llvm::InitLLVM X(argc, argv);
    llvm::cl::ParseCommandLineOptions(argc, argv, "Simple Compiler\n");
    ProgramStart = std::chrono::high_resolution_clock::now();

    llvm::errs() << "--------------------------------------------------\n";
    llvm::errs() << "🚀 Starting Compilation Process...\n";
    llvm::errs() << "--------------------------------------------------\n";

    // --- Phase 1: Lexing & Parsing ---
    double T1_Start = getCurrentTimeMs();

    Lexer Lex(Input);
    Parser Parser(Lex);
    Block *Tree = Parser.parse(); // سعی می‌کند درخت را بسازد حتی با خطا

    double T1_End = getCurrentTimeMs();
    printPhaseLog("Lexing & Parsing", T1_Start, T1_End);

    // اگر درخت ساخته نشد (خطای خیلی حاد)، خروج
    if (!Tree) {
        llvm::errs() << "❌ Fatal Parsing Error: Could not build AST.\n";
        return 1;
    }

    // --- Phase 2: Observability (همیشه انجام شود تا بتوانیم خطا را دیباگ کنیم) ---
    std::error_code EC;
    llvm::raw_fd_ostream JsonFile("ast.json", EC, llvm::sys::fs::OF_None);
    if (!EC) { ASTDumper Dumper(JsonFile); Dumper.dump(Tree); }

    std::error_code EC4;
    llvm::raw_fd_ostream CfgFile("CFG.dot", EC4, llvm::sys::fs::OF_None);
    if (!EC4) { GraphGenerator GraphGen(CfgFile); GraphGen.generateCFG(Tree); }

    // --- Phase 3: Semantic Analysis ---
    double T2_Start = getCurrentTimeMs();

    Sema Semantic;
    bool SemaHasError = Semantic.semantic(Tree, Input); // خطاها را چاپ می‌کند اما ادامه می‌دهد

    double T2_End = getCurrentTimeMs();
    printPhaseLog("Semantic Analysis", T2_Start, T2_End);

    // --- Decision Point: Stop if ANY errors found ---
    // اگر پارسر یا سما خطا داشتند، اینجا متوقف می‌شویم تا CodeGen اجرا نشود
    if (Parser.hasError() || SemaHasError) {
        llvm::errs() << "\n❌ Compilation Failed due to "
                     << (Parser.hasError() ? "Syntax" : "")
                     << ((Parser.hasError() && SemaHasError) ? " and " : "")
                     << (SemaHasError ? "Semantic" : "")
                     << " errors.\n";
        return 1;
    }

    // --- Phase 4: Code Generation ---
    double T3_Start = getCurrentTimeMs();

    CodeGen CodeGenerator;
    CodeGenerator.compile(Tree);

    double T3_End = getCurrentTimeMs();
    printPhaseLog("Code Generation", T3_Start, T3_End);

    // --- Final Report ---
    double TotalDuration = T3_End;
    llvm::errs() << "\n📊 [Advanced Tracing Report]\n";
    llvm::errs() << "--------------------------------------------------\n";
    llvm::errs() << "🔸 Total AST Nodes Created : " << AST::NodeCount << "\n";
    llvm::errs() << "🔸 Total Execution Time    : " << llvm::format("%.3f", TotalDuration) << " ms\n";
    llvm::errs() << "--------------------------------------------------\n";

    return 0;
}