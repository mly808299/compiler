#ifndef ASTUTILS_H
#define ASTUTILS_H

#include "AST.h"

// حذف getChildren() و استفاده از روش موجود در نودها
// این فایل می‌تواند برای توابع کمکی دیگر AST استفاده شود

namespace ASTUtils {

// مثال: شمارش نودها در یک بلوک
inline int countNodes(AST *node) {
    if (!node) return 0;
    int count = 1; // خود نود
    if (auto *block = dynamic_cast<Block*>(node)) {
        for (auto *stmt : block->getStatements())
            count += countNodes(stmt);
    }
    // می‌توانید برای سایر نودها نیز بازدید اضافه کنید
    return count;
}

}

#endif
