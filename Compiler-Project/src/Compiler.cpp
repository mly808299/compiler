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

double getCurrentTimeMs() {
    static auto ProgramStart = std::chrono::high_resolution_clock::now();
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

    getCurrentTimeMs(); // Start timer

    llvm::errs() << "--------------------------------------------------\n";
    llvm::errs() << "🚀 Starting Compilation Process...\n";
    llvm::errs() << "--------------------------------------------------\n";

    // 1. Lexer & Parser
    double T1_Start = getCurrentTimeMs();

    Lexer Lex(Input);
    Parser Parser(Lex);
    Block *Tree = Parser.parse();

    double T1_End = getCurrentTimeMs();

    if (!Tree || Parser.hasError())
    {
        llvm::errs() << "❌ Syntax errors occurred\n";
        return 1;
    }
    printPhaseLog("Lexing & Parsing", T1_Start, T1_End);

    // --- 2. Observability Files ---

    // A. AST JSON
    std::error_code EC;
    llvm::raw_fd_ostream JsonFile("ast.json", EC, llvm::sys::fs::OF_None);
    if (!EC) {
        ASTDumper Dumper(JsonFile);
        Dumper.dump(Tree);
    }

    // B. AST Visualization (AST.dot)
    std::error_code EC2;
    llvm::raw_fd_ostream DotFile("AST.dot", EC2, llvm::sys::fs::OF_None);
    if (!EC2) {
        GraphGenerator GraphGen(DotFile);
        GraphGen.generateAST(Tree);
    }

    // C. Call Graph (CallGraph.dot) - بخش 3
    std::error_code EC3;
    llvm::raw_fd_ostream CallFile("CallGraph.dot", EC3, llvm::sys::fs::OF_None);
    if (!EC3) {
        GraphGenerator GraphGen(CallFile);
        GraphGen.generateCallGraph(Tree);
    }

    // D. Control Flow Graph (CFG.dot) <--- این بخش جدید است
    std::error_code EC4;
    llvm::raw_fd_ostream CfgFile("CFG.dot", EC4, llvm::sys::fs::OF_None);
    if (!EC4) {
        GraphGenerator GraphGen(CfgFile);
        GraphGen.generateCFG(Tree);
    }
    // ------------------------------

    // 3. Semantic Analysis
    double T2_Start = getCurrentTimeMs();
    Sema Semantic;
    if (Semantic.semantic(Tree, Input))
    {
        llvm::errs() << "❌ Semantic errors occurred\n";
        return 1;
    }
    double T2_End = getCurrentTimeMs();
    printPhaseLog("Semantic Analysis", T2_Start, T2_End);

    // 4. Code Generation
    double T3_Start = getCurrentTimeMs();
    CodeGen CodeGenerator;
    CodeGenerator.compile(Tree);
    double T3_End = getCurrentTimeMs();
    printPhaseLog("Code Generation", T3_Start, T3_End);

    // Final Report
    double TotalDuration = T3_End;
    llvm::errs() << "\n📊 [Advanced Tracing Report]\n";
    llvm::errs() << "--------------------------------------------------\n";
    llvm::errs() << "🔸 Total AST Nodes Created : " << AST::NodeCount << "\n";
    llvm::errs() << "🔸 Total Execution Time    : " << llvm::format("%.3f", TotalDuration) << " ms\n";
    llvm::errs() << "--------------------------------------------------\n";

    return 0;
}