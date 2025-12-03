// ASTUtils.h
#ifndef ASTUTILS_H
#define ASTUTILS_H

#include "AST.h"

int countASTNodes(AST *node) {
    if (!node) return 0;
    int count = 1; // خود نود
    if (auto children = node->getChildren(); !children.empty()) {
        for (auto* c : children) count += countASTNodes(c);
    }
    return count;
}

#endif
