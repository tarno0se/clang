#include "clang/AST/Attr.h"
#include "clang/AST/ExprCXX.h"

namespace clang {

UserDefinedAttr *UserDefinedAttr::clone(ASTContext &C) const {
  auto *A = new (C) UserDefinedAttr(getLocation(), C, Expr);
  A->Inherited = Inherited;
  A->IsPackExpansion = IsPackExpansion;
  A->Implicit = Implicit;
  return A;
}

void UserDefinedAttr::printPretty(raw_ostream &OS, const PrintingPolicy &Policy) const {
  Expr->getType().dump(OS);
}

APValue UserDefinedAttr::getValue() const {
  return Expr->getValue();
}
Expr *UserDefinedAttr::getExpression() const {
  return Expr->getExpression();
}

}

