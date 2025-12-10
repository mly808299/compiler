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

    // [تغییر]: ارسال Input به پارسر برای چاپ خطا همراه با Snippet
    Parser Parser(Lex, Input);

    Block *Tree = Parser.parse();

    double T1_End = getCurrentTimeMs();
    printPhaseLog("Lexing & Parsing", T1_Start, T1_End);

    if (!Tree) {
        llvm::errs() << "❌ Fatal Parsing Error: Could not build AST.\n";
        return 1;
    }

    // --- Phase 2: Observability (AST + CFG + CallGraph) ---
    // 1. AST (JSON)
    std::error_code EC;
    // [تغییر]: نام فایل به ast.json_11 تغییر کرد
    llvm::raw_fd_ostream JsonFile("ast.json", EC, llvm::sys::fs::OF_None);
    if (!EC) { ASTDumper Dumper(JsonFile); Dumper.dump(Tree); }

    // 2. CFG (DOT)
    std::error_code EC2;
    llvm::raw_fd_ostream CfgFile("CFG.dot", EC2, llvm::sys::fs::OF_None);
    if (!EC2) { GraphGenerator GraphGen(CfgFile); GraphGen.generateCFG(Tree); }

    // 3. CallGraph (DOT)
    std::error_code EC3;
    llvm::raw_fd_ostream CgFile("CallGraph.dot", EC3, llvm::sys::fs::OF_None);
    if (!EC3) { GraphGenerator GraphGen(CgFile); GraphGen.generateCallGraph(Tree); }

    // --- Phase 3: Semantic Analysis ---
    double T2_Start = getCurrentTimeMs();

    Sema Semantic;
    bool SemaHasError = Semantic.semantic(Tree, Input);

    double T2_End = getCurrentTimeMs();
    printPhaseLog("Semantic Analysis", T2_Start, T2_End);

    // بررسی نهایی خطاها قبل از تولید کد
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