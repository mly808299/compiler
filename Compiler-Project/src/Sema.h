#ifndef SEMA_H
#define SEMA_H

#include "AST.h"
#include "Lexer.h"
#include "llvm/Support/raw_ostream.h"

class Sema {
public:
  // تغییر: اضافه کردن پارامتر InputCode
  bool semantic(AST *Tree, llvm::StringRef InputCode);
};

#endif