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
        // در حالت جمع‌آوری کال‌گراف، نود گرافیکی نمی‌سازیم
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

    void visit(AST &Node) override {}

    void visit(Block &Node) override {
        // در تمام حالت‌ها باید داخل بلاک برویم
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

        // اصلاح: پیمایش همیشه انجام شود (برای پیدا کردن length در var x = length())
        if (Node.getInit()) Node.getInit()->accept(*this);

        ParentID = SavedParent;
    }

    void visit(Assignment &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("Assign: " + Node.getName().str(), "parallelogram");

        // اصلاح: پیمایش همیشه انجام شود
        if (Node.getIndex()) Node.getIndex()->accept(*this);
        Node.getValue()->accept(*this);

        ParentID = SavedParent;
    }

    void visit(BinaryOp &Node) override {
        if (CurrentMode == DRAW_CFG) return;

        int SavedParent = ParentID;
        std::string opStr = "Op";
        switch(Node.getOperator()) {
            case BinaryOp::Plus: opStr = "+"; break;
            case BinaryOp::Minus: opStr = "-"; break;
                // ... بقیه عملگرها ...
            default: opStr = "Op"; break;
        }
        ParentID = createNode("Op: " + opStr, "circle");

        // پیمایش فرزندان
        Node.getLeft()->accept(*this);
        Node.getRight()->accept(*this);

        ParentID = SavedParent;
    }

    void visit(IfStmt &Node) override {
        int SavedParent = ParentID;
        int CondID = createNode("IF Condition", "diamond", "lightyellow");

        // شرط همیشه پیمایش شود (شاید داخل شرط تابع باشد)
        Node.getCond()->accept(*this);

        int PrevLastNode = LastNodeID;
        LastNodeID = CondID;
        int ThenID = createNode("Then", "plaintext");
        if (CurrentMode == DRAW_AST) ParentID = ThenID;
        Node.getThen()->accept(*this);
        int EndThenID = LastNodeID;

        std::vector<int> EndPoints;
        EndPoints.push_back(EndThenID);
        int CurrentCondID = CondID;

        for (auto &Elif : Node.getElifs()) {
            LastNodeID = CurrentCondID;
            int ElifCondID = createNode("ELIF Condition", "diamond", "lightyellow");
            if (CurrentMode == DRAW_CFG) {
                OS << "  Node" << CurrentCondID << " -> Node" << ElifCondID << " [label=\"false\"];\n";
            }

            // شرط Elif پیمایش شود
            Elif.first->accept(*this);

            LastNodeID = ElifCondID;
            int ElifBodyID = createNode("Elif Body", "plaintext");
            if (CurrentMode == DRAW_AST) ParentID = ElifBodyID;
            Elif.second->accept(*this);
            EndPoints.push_back(LastNodeID);
            CurrentCondID = ElifCondID;
        }

        if (Node.getElse()) {
            LastNodeID = CurrentCondID;
            int ElseID = createNode("Else", "plaintext");
            if (CurrentMode == DRAW_CFG) {
                OS << "  Node" << CurrentCondID << " -> Node" << ElseID << " [label=\"false\"];\n";
            }
            if (CurrentMode == DRAW_AST) ParentID = ElseID;
            Node.getElse()->accept(*this);
            EndPoints.push_back(LastNodeID);
        } else {
            EndPoints.push_back(CurrentCondID);
        }

        if (CurrentMode == DRAW_CFG) {
            int MergeID = ++NodeCounter;
            OS << "  Node" << MergeID << " [label=\"End IF\", shape=point];\n";
            for (int endID : EndPoints) {
                OS << "  Node" << endID << " -> Node" << MergeID << ";\n";
            }
            LastNodeID = MergeID;
        }
        ParentID = SavedParent;
    }

    void visit(ForStmt &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("FOR Loop", "hexagon", "lightcyan");

        // همیشه پیمایش شوند
        if (Node.getInit()) Node.getInit()->accept(*this);
        if (Node.getCond()) Node.getCond()->accept(*this);
        if (Node.getStep()) Node.getStep()->accept(*this);

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

    void visit(MatchStmt &Node) override {
        int SavedParent = ParentID;
        int MatchEntry = createNode("MATCH", "Mdiamond", "lightyellow");

        // همیشه پیمایش شود
        Node.getTarget()->accept(*this);

        int MergeID = -1;
        if (CurrentMode == DRAW_CFG) {
            MergeID = ++NodeCounter;
            OS << "  Node" << MergeID << " [label=\"End Match\", shape=point];\n";
        }

        for (auto &Case : Node.getCases()) {
            if (CurrentMode == DRAW_CFG) LastNodeID = MatchEntry;

            int CaseID = createNode("CASE", "box");
            ParentID = CaseID;

            // همیشه پیمایش شود
            Case.first->accept(*this);
            Case.second->accept(*this);

            if (CurrentMode == DRAW_CFG) {
                OS << "  Node" << LastNodeID << " -> Node" << MergeID << ";\n";
            }
        }
        if (CurrentMode == DRAW_CFG) LastNodeID = MergeID;
        ParentID = SavedParent;
    }

    void visit(PrintStmt &Node) override {
        if (CurrentMode == COLLECT_CALLS) CallGraphEdges.insert({CurrentFunction, "print"});

        int SavedParent = ParentID;
        ParentID = createNode("PRINT", "invtrapezium");

        // اصلاح: همیشه پیمایش شود (شاید داخل پرینت تابع صدا زده شود)
        Node.getArg()->accept(*this);

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
        // ثبت در CallGraph
        if (CurrentMode == COLLECT_CALLS) CallGraphEdges.insert({CurrentFunction, Node.getName()});

        int SavedParent = ParentID;
        ParentID = createNode("Call " + Node.getName(), "component");

        // همیشه آرگومان‌ها را بگردیم (شاید تودرتو باشند)
        for(auto *Arg : Node.getArgs()) Arg->accept(*this);

        ParentID = SavedParent;
    }

    void visit(RangeExpr &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("Comprehension", "note");

        // همیشه پیمایش
        Node.getTargetExpr()->accept(*this);
        if (Node.getCondition()) Node.getCondition()->accept(*this);

        ParentID = SavedParent;
    }

    void visit(ArrayLiteral &Node) override {
        int SavedParent = ParentID;
        ParentID = createNode("Array", "box3d");

        // همیشه پیمایش
        for(auto *Val : Node.getValues()) Val->accept(*this);

        ParentID = SavedParent;
    }

    void visit(CompoundStmt &Node) override {
        int SavedParent = ParentID;
        std::string label = Node.getName().str();
        label += (Node.getOperator() == CompoundStmt::PLE) ? " += " : " -= ";
        ParentID = createNode(label, "parallelogram");

        // همیشه پیمایش
        if (Node.getIndex()) Node.getIndex()->accept(*this);
        Node.getValue()->accept(*this);

        ParentID = SavedParent;
    }

    void visit(UnaryStmt &Node) override {
        int SavedParent = ParentID;
        std::string label = Node.getName().str();
        label += (Node.getOperator() == UnaryStmt::INC) ? "++" : "--";
        ParentID = createNode(label, "parallelogram");

        // همیشه پیمایش
        if (Node.getIndex()) Node.getIndex()->accept(*this);

        ParentID = SavedParent;
    }

    void visit(ArrayAccess &Node) override {
        if(CurrentMode==DRAW_AST) {
            int SavedParent = ParentID;
            ParentID = createNode("Index", "box");
            Node.getIndex()->accept(*this);
            ParentID = SavedParent;
        } else {
            // در حالت‌های دیگر هم ایندکس چک شود
            Node.getIndex()->accept(*this);
        }
    }
};

#endif