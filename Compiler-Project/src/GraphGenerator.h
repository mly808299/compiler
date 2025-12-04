#ifndef GRAPHGENERATOR_H
#define GRAPHGENERATOR_H

#include "AST.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <map>
#include <set>

class GraphGenerator : public ASTVisitor {
    llvm::raw_ostream &OS;
    int NodeCounter = 0;
    int ParentID = -1;
    int LastNodeID = -1;

    enum Mode { DRAW_AST, DRAW_CFG, COLLECT_CALLS };
    Mode CurrentMode = DRAW_AST;

    std::string CurrentFunction = "main";
    std::set<std::pair<std::string, std::string>> CallGraphEdges;

    int createNode(std::string Label, std::string Shape = "box", std::string Color = "black") {
        if (CurrentMode == COLLECT_CALLS) return -1;

        int CurrentID = ++NodeCounter;
        OS << "  Node" << CurrentID << " [label=\"" << Label << "\", shape=" << Shape << ", color=" << Color << "];\n";

        if (CurrentMode == DRAW_AST) {
            if (ParentID != -1) {
                OS << "  Node" << ParentID << " -> Node" << CurrentID << ";\n";
            }
        } else if (CurrentMode == DRAW_CFG) {
            if (LastNodeID != -1) {
                OS << "  Node" << LastNodeID << " -> Node" << CurrentID << " [weight=10];\n";
            }
            LastNodeID = CurrentID;
        }
        return CurrentID;
    }

public:
    GraphGenerator(llvm::raw_ostream &OS) : OS(OS) {}

    void generateAST(AST *Tree) {
        CurrentMode = DRAW_AST;
        NodeCounter = 0; ParentID = -1;
        OS << "digraph AST {\n node [fontname=\"Arial\"];\n";
        if (Tree) Tree->accept(*this);
        OS << "}\n";
    }

    void generateCallGraph(AST *Tree) {
        CurrentMode = COLLECT_CALLS;
        CallGraphEdges.clear();
        OS << "digraph CallGraph {\n node [shape=ellipse, style=filled, fillcolor=lightblue];\n main [shape=box, fillcolor=orange];\n";
        if (Tree) Tree->accept(*this);
        for (const auto &edge : CallGraphEdges) {
            OS << "  \"" << edge.first << "\" -> \"" << edge.second << "\";\n";
        }
        OS << "}\n";
    }

    void generateCFG(AST *Tree) {
        CurrentMode = DRAW_CFG;
        NodeCounter = 0; LastNodeID = -1;
        OS << "digraph CFG {\n node [shape=box, fontname=\"Arial\"];\n";
        OS << "  Node0 [label=\"Start\", shape=circle, style=filled, fillcolor=green];\n";
        LastNodeID = 0;
        if (Tree) Tree->accept(*this);
        OS << "}\n";
    }

    // --- Visit Methods ---

    void visit(AST &Node) override {}

    void visit(Block &Node) override {
        if (CurrentMode == DRAW_AST) {
            int SavedParent = ParentID;
            ParentID = createNode("Block", "folder");
            for (auto *Stmt : Node.getStatements()) if (Stmt) Stmt->accept(*this);
            ParentID = SavedParent;
        } else {
            for (auto *Stmt : Node.getStatements()) if (Stmt) Stmt->accept(*this);
        }
    }

    void visit(Declaration &Node) override {
        int SavedParent = ParentID;
        std::string label = "Decl: " + Node.getName().str();
        ParentID = createNode(label, "component");
        if (CurrentMode == DRAW_AST && Node.getInit()) Node.getInit()->accept(*this);
        ParentID = SavedParent;
    }

    void visit(Assignment &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("Assign: " + Node.getName().str(), "parallelogram");
        if (CurrentMode == DRAW_AST) {
            if (Node.getIndex()) Node.getIndex()->accept(*this);
            Node.getValue()->accept(*this);
        }
        ParentID = SavedParent;
    }

    void visit(BinaryOp &Node) override {
        if (CurrentMode == DRAW_CFG) return;
        int SavedParent = ParentID;
        std::string opStr = "Op";
        switch(Node.getOperator()) {
            case BinaryOp::Plus: opStr = "+"; break;
            case BinaryOp::Minus: opStr = "-"; break;
            case BinaryOp::Mul: opStr = "*"; break;
            case BinaryOp::Div: opStr = "/"; break;
            case BinaryOp::Mod: opStr = "%"; break;
            case BinaryOp::And: opStr = "&&"; break;
            case BinaryOp::Or: opStr = "||"; break;
            case BinaryOp::Eq: opStr = "=="; break;
            case BinaryOp::Neq: opStr = "!="; break;
            case BinaryOp::Lt: opStr = "<"; break;
            case BinaryOp::Gt: opStr = ">"; break;
            case BinaryOp::Lte: opStr = "<="; break;
            case BinaryOp::Gte: opStr = ">="; break;
        }
        ParentID = createNode("Op: " + opStr, "circle");
        Node.getLeft()->accept(*this);
        Node.getRight()->accept(*this);
        ParentID = SavedParent;
    }

    void visit(IfStmt &Node) override {
        int SavedParent = ParentID;
        int EntryID = LastNodeID;

        int CondID = createNode("IF Condition", "diamond", "lightyellow");

        if (CurrentMode == DRAW_AST) Node.getCond()->accept(*this);

        // Then Branch
        LastNodeID = CondID;
        int ThenID = createNode("Then", "plaintext"); // نشانه شاخه
        if (CurrentMode == DRAW_AST) {
             ParentID = ThenID; // اتصال فرزندان به نود Then
        }
        Node.getThen()->accept(*this);
        int EndThenID = LastNodeID;

        // Else Branch
        int EndElseID = -1;
        if (Node.getElse()) {
            LastNodeID = CondID;
            ParentID = SavedParent; // ریست کردن والد برای شاخه Else در گراف درختی
            int ElseID = createNode("Else", "plaintext");
            if (CurrentMode == DRAW_AST) ParentID = ElseID;
            Node.getElse()->accept(*this);
            EndElseID = LastNodeID;
        }

        // Merge (CFG Only)
        if (CurrentMode == DRAW_CFG) {
            int MergeID = ++NodeCounter;
            OS << "  Node" << MergeID << " [label=\"End IF\", shape=point];\n";
            OS << "  Node" << EndThenID << " -> Node" << MergeID << ";\n";
            if (EndElseID != -1) OS << "  Node" << EndElseID << " -> Node" << MergeID << ";\n";
            else OS << "  Node" << CondID << " -> Node" << MergeID << " [label=\"false\"];\n";
            LastNodeID = MergeID;
        }
        ParentID = SavedParent;
    }

    void visit(ForStmt &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("FOR Loop", "hexagon", "lightcyan");

        if (CurrentMode == DRAW_AST) {
            if (Node.getInit()) Node.getInit()->accept(*this);
            if (Node.getCond()) Node.getCond()->accept(*this);
            if (Node.getStep()) Node.getStep()->accept(*this);
        }

        int LoopHeader = LastNodeID;
        Node.getBody()->accept(*this);

        if (CurrentMode == DRAW_CFG) {
            OS << "  Node" << LastNodeID << " -> Node" << LoopHeader << " [label=\"loop\"];\n";
        }
        ParentID = SavedParent;
    }

    void visit(ForEachStmt &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("FOREACH " + Node.getIterator().str(), "hexagon", "lightcyan");
        int LoopHeader = LastNodeID;

        Node.getBody()->accept(*this);

        if (CurrentMode == DRAW_CFG) {
             OS << "  Node" << LastNodeID << " -> Node" << LoopHeader << " [label=\"next\"];\n";
        }
        ParentID = SavedParent;
    }

    // --- تغییر اصلی اینجاست (رفع ارور getText) ---
    void visit(MatchStmt &Node) override {
        int SavedParent = ParentID;

        // 1. نود MATCH را بدون اسم متغیر می‌سازیم تا ارور ندهد
        int MatchEntry = createNode("MATCH", "Mdiamond", "lightyellow");

        // 2. در حالت AST، متغیر هدف را به عنوان فرزند رسم می‌کنیم
        if (CurrentMode == DRAW_AST) {
            Node.getTarget()->accept(*this);
        }

        int MergeID = -1;
        if (CurrentMode == DRAW_CFG) {
            MergeID = ++NodeCounter;
            OS << "  Node" << MergeID << " [label=\"End Match\", shape=point];\n";
        }

        for (auto &Case : Node.getCases()) {
            if (CurrentMode == DRAW_CFG) LastNodeID = MatchEntry;

            int CaseID = createNode("CASE", "box");
            ParentID = CaseID;

            if (CurrentMode == DRAW_AST) {
                 Case.first->accept(*this); // Pattern
            }

            Case.second->accept(*this); // Body

            if (CurrentMode == DRAW_CFG) {
                OS << "  Node" << LastNodeID << " -> Node" << MergeID << ";\n";
            }
        }
        if (CurrentMode == DRAW_CFG) LastNodeID = MergeID;
        ParentID = SavedParent;
    }
    // ---------------------------------------------

    void visit(PrintStmt &Node) override {
        if (CurrentMode == COLLECT_CALLS) CallGraphEdges.insert({CurrentFunction, "print"});
        int SavedParent = ParentID;
        ParentID = createNode("PRINT", "invtrapezium");
        if (CurrentMode == DRAW_AST) Node.getArg()->accept(*this);
        ParentID = SavedParent;
    }

    void visit(Final &Node) override {
        if(CurrentMode==DRAW_AST) {
            std::string rawVal = Node.getValue().str();
            std::string escapedVal;
            for (char c : rawVal) {
                if (c == '"') escapedVal += "\\\"";
                else if (c == '\\') escapedVal += "\\\\";
                else escapedVal += c;
            }
            createNode(escapedVal, "ellipse");
        }
    }

    void visit(BuiltinCall &Node) override {
        if (CurrentMode == COLLECT_CALLS) CallGraphEdges.insert({CurrentFunction, Node.getName()});
        int SavedParent = ParentID;
        ParentID = createNode("Call " + Node.getName(), "component");
        if (CurrentMode == DRAW_AST) {
            for(auto *Arg : Node.getArgs()) Arg->accept(*this);
        }
        ParentID = SavedParent;
    }

    void visit(RangeExpr &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("Comprehension", "note");
        if (CurrentMode == DRAW_AST) {
            Node.getTargetExpr()->accept(*this);
            if (Node.getCondition()) Node.getCondition()->accept(*this);
        }
        ParentID = SavedParent;
    }

    void visit(ArrayLiteral &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("Array", "box3d");
        if (CurrentMode == DRAW_AST) {
            for(auto *Val : Node.getValues()) Val->accept(*this);
        }
        ParentID = SavedParent;
    }

    void visit(CompoundStmt &Node) override {
        int SavedParent = ParentID;
        std::string label = Node.getName().str();
        label += (Node.getOperator() == CompoundStmt::PLE) ? " += " : " -= ";
        ParentID = createNode(label, "parallelogram");
        if (CurrentMode == DRAW_AST) {
             if (Node.getIndex()) Node.getIndex()->accept(*this);
             Node.getValue()->accept(*this);
        }
        ParentID = SavedParent;
    }

    void visit(UnaryStmt &Node) override {
        int SavedParent = ParentID;
        std::string label = Node.getName().str();
        label += (Node.getOperator() == UnaryStmt::INC) ? "++" : "--";
        ParentID = createNode(label, "parallelogram");
        if (CurrentMode == DRAW_AST && Node.getIndex()) Node.getIndex()->accept(*this);
        ParentID = SavedParent;
    }

    void visit(ArrayAccess &Node) override {
        if(CurrentMode==DRAW_AST) {
             int SavedParent = ParentID;
             ParentID = createNode("Index", "box");
             Node.getIndex()->accept(*this);
             ParentID = SavedParent;
        }
    }
};

#endif