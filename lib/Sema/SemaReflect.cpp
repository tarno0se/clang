//===--- SemaReflect.cpp - Semantic Analysis for Reflection ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for reflection.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/PartialDiagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/ParsedTemplate.h"
#include "clang/Sema/ParsedReflection.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/SemaInternal.h"
#include "TypeLocBuilder.h"
using namespace clang;
using namespace sema;

ParsedReflectionOperand Sema::ActOnReflectedType(TypeResult T) {
  // Cheat by funneling this back through template argument processing
  // because it's possible to end up with deduction guides.
  ParsedTemplateArgument Arg = ActOnTemplateTypeArgument(T);
  if (Arg.getKind() == ParsedTemplateArgument::Template)
    return ActOnReflectedTemplate(Arg);
  assert(Arg.getKind() == ParsedTemplateArgument::Type);

  // llvm::outs() << "GOT TYPE\n";
  // T.get().get()->dump();
  return ParsedReflectionOperand(T.get(), Arg.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedTemplate(ParsedTemplateArgument T) {
  assert(T.getKind() == ParsedTemplateArgument::Template);
  const CXXScopeSpec& SS = T.getScopeSpec();
  ParsedTemplateTy Temp = T.getAsTemplate();
  
  // llvm::outs() << "GOT TEMPLATE\n";
  // Temp.get().getAsTemplateDecl()->dump();
  return ParsedReflectionOperand(SS, Temp, T.getLocation());
}

ParsedReflectionOperand Sema::ActOnReflectedNamespace(CXXScopeSpec &SS,
                                                      SourceLocation &Loc,
                                                      Decl *D) {
  // llvm::outs() << "GOT NAMESPACE\n";
  // D->dump();
  return ParsedReflectionOperand(SS, D, Loc);
}

ParsedReflectionOperand Sema::ActOnReflectedNamespace(SourceLocation Loc) {
  // llvm::outs() << "GOT NAMESPACE\n";
  // D->dump();
  // Clang uses TUDecl in place of having a global namespace.
  return ParsedReflectionOperand(Context.getTranslationUnitDecl(), Loc);
}

ParsedReflectionOperand Sema::ActOnReflectedExpression(Expr *E) {
  
  // llvm::outs() << "GOT EXPRESSION\n";
  // E->dump();
  return ParsedReflectionOperand(E, E->getBeginLoc());
}

/// Returns a constant expression that encodes the value of the reflection.
/// The type of the reflection is meta::reflection, an enum class.
ExprResult Sema::ActOnCXXReflectExpr(SourceLocation Loc,
                                     ParsedReflectionOperand Ref, 
                                     SourceLocation LP, 
                                     SourceLocation RP) {
  // Translated the parsed reflection operand into an AST operand.
  switch (Ref.getKind()) {
  case ParsedReflectionOperand::Invalid:
    break;
  case ParsedReflectionOperand::Type: {
    QualType Arg = Ref.getAsType().get();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::Template: {
    TemplateName Arg = Ref.getAsTemplate().get();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  case ParsedReflectionOperand::GlobalNamespace:
  case ParsedReflectionOperand::Namespace: {
    ReflectedNamespace RNS = Ref.getAsNamespace();
    const CXXScopeSpec SS = Ref.getScopeSpec();

    bool IsQualified = !SS.isEmpty();
    if (IsQualified) {
      QualifiedNamespaceName *QNS
        = new (Context) QualifiedNamespaceName(RNS, SS.getScopeRep());
      return BuildCXXReflectExpr(Loc, NamespaceName(QNS), LP, RP);
    }

    return BuildCXXReflectExpr(Loc, NamespaceName(RNS), LP, RP);
  }
  case ParsedReflectionOperand::Expression: {
    Expr *Arg = Ref.getAsExpr();
    return BuildCXXReflectExpr(Loc, Arg, LP, RP);
  }
  }

  return ExprError();
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, QualType T,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, T, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, TemplateName N,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, N, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, NamespaceName N,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, N, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, Expr *E,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, E, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, Decl *D,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, D, LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(SourceLocation Loc, CXXBaseSpecifier *B,
                                     SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::Create(Context, Context.MetaInfoTy, Loc, B, LP, RP);
}

ExprResult Sema::BuildInvalidCXXReflectExpr(SourceLocation Loc,
                                         SourceLocation LP, SourceLocation RP) {
  return CXXReflectExpr::CreateInvalid(Context, Context.MetaInfoTy, Loc,
                                       LP, RP);
}

ExprResult Sema::BuildCXXReflectExpr(APValue Reflection, SourceLocation Loc) {
  assert(Reflection.isReflection());

  switch (Reflection.getReflectionKind()) {
  case RK_invalid:
    return BuildInvalidCXXReflectExpr(/*Loc=*/Loc, /*LP=*/SourceLocation(),
                                      /*RP=*/SourceLocation());
  case RK_declaration: {
    auto ReflOp = const_cast<Decl *>(Reflection.getReflectedDeclaration());
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }

  case RK_type: {
    auto ReflOp = Reflection.getReflectedType();
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }

  case RK_expression: {
    auto ReflOp = const_cast<Expr *>(Reflection.getReflectedExpression());
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }

  case RK_base_specifier: {
    auto ReflOp = const_cast<CXXBaseSpecifier *>(
                                        Reflection.getReflectedBaseSpecifier());
    return BuildCXXReflectExpr(/*Loc=*/Loc, ReflOp, /*LP=*/SourceLocation(),
                               /*RP=*/SourceLocation());
  }
  }

  llvm_unreachable("invalid reflection kind");
}

static bool SetType(QualType& Ret, QualType T) {
  Ret = T;
  return true;
}

static bool SetCStrType(QualType& Ret, ASTContext &Ctx) {
  return SetType(Ret, Ctx.getPointerType(Ctx.getConstType(Ctx.CharTy)));
}

// Check that the argument has the right type. Ignore references and
// cv-qualifiers on the expression.
static bool CheckReflectionOperand(Sema &SemaRef, Expr *E) {
  // Get the type of the expression.
  QualType Source = E->getType();
  if (Source->isReferenceType())
    Source = Source->getPointeeType();
  Source = Source.getUnqualifiedType();
  Source = SemaRef.Context.getCanonicalType(Source);

  if (Source != SemaRef.Context.MetaInfoTy) {
    SemaRef.Diag(E->getBeginLoc(), diag::err_expression_not_reflection);
    return false;
  }

  return true;
}

// Gets the type and query from Arg. Returns false on error.
static bool GetTypeAndQuery(Sema &SemaRef, Expr *&Arg, QualType &ResultTy, 
                            ReflectionQuery& Query) {
  // Guess these values initially.
  ResultTy = SemaRef.Context.DependentTy;
  Query = RQ_unknown;

  if (Arg->isTypeDependent() || Arg->isValueDependent())
    return true;

  // Convert to an rvalue.
  ExprResult Conv = SemaRef.DefaultLvalueConversion(Arg);
  if (Conv.isInvalid())
    return false;
  Arg = Conv.get();

  // FIXME: Should what type should this actually be?
  QualType T = Arg->getType();
  if (T->isIntegralType(SemaRef.Context)) {
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_wrong_type);
    return false;
  }

  // Evaluate the query operand
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!Arg->EvaluateAsRValue(Result, SemaRef.Context)) {
    // FIXME: This is not the right error.
    SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_not_constant);
    for (PartialDiagnosticAt PD : Diags)
      SemaRef.Diag(PD.first, PD.second);
    return false;
  }

  Query = static_cast<ReflectionQuery>(Result.Val.getInt().getExtValue());

  if (isPredicateQuery(Query)) 
    return SetType(ResultTy, SemaRef.Context.BoolTy);
  if (isTraitQuery(Query))
    return SetType(ResultTy, SemaRef.Context.UnsignedIntTy);
  if (isAssociatedReflectionQuery(Query))
    return SetType(ResultTy, SemaRef.Context.MetaInfoTy);
  if (isNameQuery(Query))
    return SetCStrType(ResultTy, SemaRef.Context);

  SemaRef.Diag(Arg->getExprLoc(), diag::err_reflection_query_invalid);
  return false;
}

ExprResult Sema::ActOnCXXReflectionTrait(SourceLocation KWLoc,
                                         SmallVectorImpl<Expr *> &Args,
                                         SourceLocation LParenLoc,
                                         SourceLocation RParenLoc) {
  // FIXME: There are likely unary and n-ary queries.
  if (Args.size() != 2) {
    Diag(KWLoc, diag::err_reflection_wrong_arity) << (int)Args.size();
    return ExprError();
  }

  // Get the type of the query. Note that this will convert Args[0].
  QualType Ty;
  ReflectionQuery Query;
  if (!GetTypeAndQuery(*this, Args[0], Ty, Query))
    return ExprError();

  // Convert the remaining operands to rvalues.
  for (std::size_t I = 1; I < Args.size(); ++I) {
    ExprResult Arg = DefaultLvalueConversion(Args[I]);
    if (Arg.isInvalid())
      return ExprError();
    Args[I] = Arg.get();
  }

  return new (Context) CXXReflectionTraitExpr(Context, Ty, Query, Args, 
                                              KWLoc, LParenLoc, RParenLoc);
}

static bool HasDependentParts(SmallVectorImpl<Expr *>& Parts) {
 return std::any_of(Parts.begin(), Parts.end(), [](const Expr *E) {
   return E->isTypeDependent() || E->isValueDependent();
 });
}

static QualType DeduceCanonicalType(Sema &S, Expr *E) {
  QualType Ty = E->getType();
  if (AutoType *D = Ty->getContainedAutoType()) {
    Ty = D->getDeducedType();
    if (!Ty.getTypePtr())
      llvm_unreachable("Undeduced value reflection");
  }
  return S.Context.getCanonicalType(Ty);
}

ExprResult Sema::ActOnCXXReflectPrintLiteral(SourceLocation KWLoc,
                                             SmallVectorImpl<Expr *> &Args,
                                             SourceLocation LParenLoc,
                                             SourceLocation RParenLoc) {
  if (HasDependentParts(Args))
    return new (Context) CXXReflectPrintLiteralExpr(
        Context, Context.DependentTy, Args, KWLoc, LParenLoc, RParenLoc);

  for (std::size_t I = 0; I < Args.size(); ++I) {
    Expr *&E = Args[I];

    assert(!E->isTypeDependent() && !E->isValueDependent()
        && "Dependent element");

    /// Convert to rvalue.
    ExprResult Conv = DefaultFunctionArrayLvalueConversion(E);
    if (Conv.isInvalid())
      return ExprError();
    E = Conv.get();

    // Get the canonical type of the expression.
    QualType T = DeduceCanonicalType(*this, E);

    // Ensure we're working with a valid operand.
    if (T.isCXXStringLiteralType())
      continue;

    if (T->isIntegerType())
      continue;

    Diag(E->getExprLoc(), diag::err_reflect_print_literal_wrong_operand_type);
    return ExprError();
  }

  return new (Context) CXXReflectPrintLiteralExpr(
    Context, Context.IntTy, Args, KWLoc, LParenLoc, RParenLoc);
}

ExprResult Sema::ActOnCXXReflectPrintReflection(SourceLocation KWLoc,
                                                Expr *Reflection,
                                                SourceLocation LParenLoc,
                                                SourceLocation RParenLoc) {
  if (Reflection->isTypeDependent() || Reflection->isValueDependent())
    return new (Context) CXXReflectPrintReflectionExpr(
        Context, Context.DependentTy, Reflection, KWLoc, LParenLoc, RParenLoc);

  ExprResult Arg = DefaultLvalueConversion(Reflection);
  if (Arg.isInvalid())
    return ExprError();

  if (!CheckReflectionOperand(*this, Reflection))
    return ExprError();

  return new (Context) CXXReflectPrintReflectionExpr(
      Context, Context.IntTy, Arg.get(), KWLoc, LParenLoc, RParenLoc);
}

ExprResult Sema::ActOnCXXReflectDumpReflection(SourceLocation KWLoc,
                                               Expr *Reflection,
                                               SourceLocation LParenLoc,
                                               SourceLocation RParenLoc) {
  if (Reflection->isTypeDependent() || Reflection->isValueDependent())
    return new (Context) CXXReflectDumpReflectionExpr(
        Context, Context.DependentTy, Reflection, KWLoc, LParenLoc, RParenLoc);

  ExprResult Arg = DefaultLvalueConversion(Reflection);
  if (Arg.isInvalid())
    return ExprError();

  if (!CheckReflectionOperand(*this, Reflection))
    return ExprError();

  return new (Context) CXXReflectDumpReflectionExpr(
      Context, Context.IntTy, Arg.get(), KWLoc, LParenLoc, RParenLoc);
}

/// Handle a call to \c __compiler_error.
ExprResult Sema::ActOnCXXCompilerErrorExpr(Expr *MessageExpr,
                                           SourceLocation BuiltinLoc,
                                           SourceLocation RParenLoc) {
  if (DiagnoseUnexpandedParameterPack(MessageExpr))
    return ExprError();

  return BuildCXXCompilerErrorExpr(MessageExpr, BuiltinLoc, RParenLoc);
}

/// Build a \c __compiler_error expression.
ExprResult Sema::BuildCXXCompilerErrorExpr(Expr *MessageExpr,
                                           SourceLocation BuiltinLoc,
                                           SourceLocation RParenLoc) {
  assert(MessageExpr != nullptr);

  ExprResult Converted = DefaultFunctionArrayLvalueConversion(MessageExpr);
  if (Converted.isInvalid())
    return ExprError();
  MessageExpr = Converted.get();

  // Get the canonical type of the expression.
  QualType T = DeduceCanonicalType(*this, MessageExpr);

  // Ensure we're working with a valid operand.
  if (!T.isCXXStringLiteralType()) {
    SourceLocation &&Loc = MessageExpr->getExprLoc();
    Diag(Loc, diag::err_compiler_error_wrong_operand_type);
    return ExprError();
  }

  return CXXCompilerErrorExpr::Create(Context, Context.VoidTy, MessageExpr,
                                      BuiltinLoc, RParenLoc);
}

static Reflection EvaluateReflection(Sema &S, Expr *E) {
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!E->EvaluateAsRValue(Result, S.Context)) {
    S.Diag(E->getExprLoc(), diag::reflection_not_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      S.Diag(PD.first, PD.second);
    return Reflection();
  }

  return Reflection(S.Context, Result.Val);
}

ExprResult Sema::ActOnCXXIdExprExpr(SourceLocation KWLoc,
                                    Expr *Refl,
                                    SourceLocation LParenLoc,
                                    SourceLocation RParenLoc,
                                    SourceLocation EllipsisLoc)
{
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return new (Context) CXXIdExprExpr(Context.DependentTy, Refl, KWLoc,
                                       LParenLoc, LParenLoc);

  Reflection R = EvaluateReflection(*this, Refl);
  if (R.isInvalid())
    return ExprError();

  if (R.isExpression()) {
    if (const DeclRefExpr *Ref = dyn_cast<DeclRefExpr>(R.getAsExpression())) {
      Decl *ReferencedDecl = const_cast<NamedDecl *>(Ref->getFoundDecl());
      ReferencedDecl->markUsed(Context);

      return const_cast<DeclRefExpr *>(Ref);
    }
  }

  // FIXME: Emit a better error diagnostic.
  Diag(Refl->getExprLoc(), diag::err_expression_not_value_reflection);
  return ExprError();
}

static Expr *ReflectionToValueExpr(Sema &S, const Reflection &R,
                                   SourceLocation SL) {
  Expr *Eval = nullptr;

  if (R.isDeclaration()) {
    // If this is a value declaration, the build a DeclRefExpr to evaluate.
    // Otherwise, it's not evaluable.
    if (const ValueDecl *VD = dyn_cast<ValueDecl>(R.getAsDeclaration())) {
      QualType T = VD->getType();
      ExprValueKind VK = Expr::getValueKindForType(T);
      ValueDecl *NCVD = const_cast<ValueDecl *>(VD);
      Eval = S.BuildDeclRefExpr(NCVD, T, VK, SL).get();
    }
  } else if (R.isExpression()) {
    // Just evaluate the expression.
    Eval = const_cast<Expr *>(R.getAsExpression());
  } else {
    // This is not a value.
    return Eval;
  }

  // If the expression we're going to evaluate is a reference to a field.
  // Adjust this to be a pointer to that field.
  if (DeclRefExpr *Ref = dyn_cast_or_null<DeclRefExpr>(Eval)) {
    if (const FieldDecl *F = dyn_cast<FieldDecl>(Ref->getDecl())) {
      QualType Ty = F->getType();
      const Type *Cls = S.Context.getTagDeclType(F->getParent()).getTypePtr();
      Ty = S.Context.getMemberPointerType(Ty, Cls);
      Eval = new (S.Context) UnaryOperator(Ref, UO_AddrOf, Ty, VK_RValue,
                                           OK_Ordinary, Ref->getExprLoc(),
                                           false);
    }
  }

  return Eval;
}

enum RangeKind {
  RK_Array,
  RK_Range,
  RK_Tuple,
  RK_Struct,
  RK_Unknown,
};

/// A facility used to determine the begin and end of a constexpr range
/// or array.
struct ExpansionContextBuilder {
  ExpansionContextBuilder(Sema &S, Scope *CS, Expr *Range)
    : SemaRef(S), CurScope(CS), Range(Range)
    {}

  /// Construct calls to std::begin(Range) and std::end(Range)
  /// Returns true on error.
  bool BuildCalls();

  Expr *getRangeBeginCall() const { return RangeBegin; }
  Expr *getRangeEndCall() const { return RangeEnd; }

  Expr *getRange() const { return Range; }

  RangeKind getKind() const { return Kind; }
private:
  bool BuildArrayCalls();
  bool BuildRangeCalls();

private:
  Sema &SemaRef;

  /// The scope in which analysis will be performed.
  Scope *CurScope;

  /// Calls to std::begin(range) and std::end(range), respectively.
  Expr *RangeBegin = nullptr;
  Expr *RangeEnd = nullptr;

  /// The Range that we are constructing an expansion context from.
  Expr *Range;

  RangeKind Kind;
};

bool
ExpansionContextBuilder::BuildCalls()
{
  SourceLocation Loc;

  // If the range is dependent, mark it and return. We'll transform it later.
  if (Range->getType()->isDependentType() || Range->isValueDependent()) {
    Kind = RK_Unknown;
    return false;
  }

  if (Range->getType()->isConstantArrayType()) {
    Kind = RK_Array;
    return BuildArrayCalls();
  }

  Kind = RK_Range;

  // Get the std namespace for std::begin and std::end
  NamespaceDecl *Std = SemaRef.getOrCreateStdNamespace();

  // Get the info for a call to std::begin
  DeclarationNameInfo BeginNameInfo(
      &SemaRef.Context.Idents.get("begin"), Loc);
  LookupResult BeginCallLookup(SemaRef, BeginNameInfo, Sema::LookupOrdinaryName);
  if (!SemaRef.LookupQualifiedName(BeginCallLookup, Std))
    return true;
  if (BeginCallLookup.getResultKind() != LookupResult::FoundOverloaded)
    return true;
  UnresolvedLookupExpr *BeginFn =
    UnresolvedLookupExpr::Create(SemaRef.Context, /*NamingClass=*/nullptr,
                                 NestedNameSpecifierLoc(), BeginNameInfo,
                                 /*ADL=*/true, /*Overloaded=*/true,
                                 BeginCallLookup.begin(),
                                 BeginCallLookup.end());

  // Get the info for a call to std::end
  DeclarationNameInfo EndNameInfo(
      &SemaRef.Context.Idents.get("end"), Loc);
  LookupResult EndCallLookup(SemaRef, EndNameInfo, Sema::LookupOrdinaryName);
  if (!SemaRef.LookupQualifiedName(EndCallLookup, Std))
    return true;
  if (EndCallLookup.getResultKind() != LookupResult::FoundOverloaded)
    return true;
  UnresolvedLookupExpr *EndFn =
    UnresolvedLookupExpr::Create(SemaRef.Context, /*NamingClass=*/nullptr,
                                 NestedNameSpecifierLoc(), EndNameInfo,
                                 /*ADL=*/true, /*Overloaded=*/true,
                                 EndCallLookup.begin(), 
                                 EndCallLookup.end());

  // Build the actual calls
  Expr *Args[] = {Range};
  ExprResult BeginCall =
    SemaRef.ActOnCallExpr(CurScope, BeginFn, Loc, Args, Loc);
  if (BeginCall.isInvalid())
    return true;
  ExprResult EndCall =
    SemaRef.ActOnCallExpr(CurScope, EndFn, Loc, Args, Loc);
  if (EndCall.isInvalid())
    return true;

  RangeBegin = BeginCall.get();
  RangeEnd = EndCall.get();

  return false;
}

bool
ExpansionContextBuilder::BuildArrayCalls()
{
  // For an array arr, RangeBegin is arr[0]
  IntegerLiteral *ZeroIndex =
    IntegerLiteral::Create(SemaRef.Context, llvm::APSInt::getUnsigned(0),
                           SemaRef.Context.getSizeType(), SourceLocation());

  ExprResult BeginAccessor =
    SemaRef.ActOnArraySubscriptExpr(CurScope, Range, SourceLocation(),
                                    ZeroIndex, SourceLocation());
  if (BeginAccessor.isInvalid())
    return true;

  RangeBegin = BeginAccessor.get();

  // For an array of size N, RangeEnd is N (not arr[N])
  // Get the canonical type here, as some transformations may
  // have been applied earlier on.
  ConstantArrayType const *ArrayTy = cast<ConstantArrayType>(
    Range->getType()->getCanonicalTypeInternal());
  llvm::APSInt Last(ArrayTy->getSize(), true);
  IntegerLiteral *LastIndex =
    IntegerLiteral::Create(SemaRef.Context, Last,
                           SemaRef.Context.getSizeType(), SourceLocation());
  RangeEnd = LastIndex;
  return false;
}

/// Traverse a C++ Constexpr Range
struct RangeTraverser {
  RangeTraverser(Sema &SemaRef, RangeKind Kind, Expr *RangeBegin,
                 Expr *RangeEnd)
    : SemaRef(SemaRef), Current(RangeBegin), RangeEnd(RangeEnd),
    Kind(Kind), I()
    {}

  /// Current == RangeEnd
  explicit operator bool();

  /// Dereference and evaluate the current value as a constant expression.
  Expr *operator*();

  /// Call std::next(Current, 1) if this is a constexpr range,
  /// Increment the array subscript if it is an array.
  RangeTraverser &operator++();

  /// Get the range kind.
  RangeKind getKind() const { return Kind; }

private:
  Sema &SemaRef;

  /// The current element in the traversal
  Expr *Current;

  /// One-past-the-end iterator (std::end) of the range we are traversing.
  Expr *RangeEnd = nullptr;

  /// The kind of product type we are traversing.
  RangeKind Kind;

  /// An integer Index that keeps track of the current element.
  std::size_t I;
};

static ExprResult
BuildSubscriptAccess(Sema &SemaRef, Expr *Current, std::size_t Index)
{
  ASTContext &Context = SemaRef.Context;

  // The subscript.
  IntegerLiteral *E =
    IntegerLiteral::Create(Context, llvm::APSInt::getUnsigned(Index),
                           Context.getSizeType(), SourceLocation());

  // Get the actual array base to create an incremented subscript access.
  ArraySubscriptExpr *CurrentSubscript =
    static_cast<ArraySubscriptExpr*>(Current);
  Expr *Base = CurrentSubscript->getBase();

  ExprResult RangeAccessor =
    SemaRef.ActOnArraySubscriptExpr(SemaRef.getCurScope(), Base,
                                    SourceLocation(), E, SourceLocation());
  if (RangeAccessor.isInvalid())
    return ExprError();
  return RangeAccessor;
}

// Build a call to std::next(Arg, Induction) for the range traverser
static ExprResult
BuildNextCall(Sema &SemaRef, Expr *Arg, Expr *Induction)
{
  NamespaceDecl *Std = SemaRef.getOrCreateStdNamespace();
  SourceLocation Loc = SourceLocation();

  DeclarationNameInfo NextNameInfo(
    &SemaRef.Context.Idents.get("next"), Loc);
  LookupResult NextCallLookup(SemaRef, NextNameInfo, Sema::LookupOrdinaryName);
  if (!SemaRef.LookupQualifiedName(NextCallLookup, Std))
    return ExprError();
  if (NextCallLookup.getResultKind() != LookupResult::FoundOverloaded)
    return ExprError();

  UnresolvedLookupExpr *NextFn =
    UnresolvedLookupExpr::Create(SemaRef.Context, /*NamingClass=*/nullptr,
                                 NestedNameSpecifierLoc(), NextNameInfo,
                                 /*ADL=*/true, /*Overloaded=*/true,
                                 NextCallLookup.begin(),
                                 NextCallLookup.end());
  Expr *Args[] = {Arg, Induction};
  ExprResult NextCall =
    SemaRef.ActOnCallExpr(SemaRef.getCurScope(), NextFn, Loc, Args, Loc);
  if (NextCall.isInvalid())
    return ExprError();

  return NextCall;
}

// Dereference a call to an iterator (as built in BuildNextCall)
static ExprResult
BuildDeref(Sema &SemaRef, Expr *NextCall)
{
  ExprResult NextDeref =
    SemaRef.ActOnUnaryOp(SemaRef.getCurScope(), SourceLocation(),
                         tok::star, NextCall);
  if (NextDeref.isInvalid())
    return ExprError();
  return NextDeref;
}

RangeTraverser::operator bool()
{
  switch (Kind) {
  case RK_Array:
  // If this is an array, we will simply see if we have iterated
  // N times.
    return I == cast<IntegerLiteral>(RangeEnd)->getValue();

  case RK_Range: {
    Expr::EvalResult EqualRes;
    ExprResult Equal =
      SemaRef.ActOnBinOp(SemaRef.getCurScope(), SourceLocation(), tok::equalequal,
                         Current, RangeEnd);
    Equal.get()->EvaluateAsConstantExpr(EqualRes, Expr::EvaluateForCodeGen,
                                        SemaRef.Context);

    return EqualRes.Val.getInt() == 1;
  }
  default:
    llvm_unreachable("Invalid Range Kind.");
  }
}

Expr *
RangeTraverser::operator*()
{
  Expr *Deref = nullptr;
  switch (Kind) {
  case RK_Range:
    Deref = BuildDeref(SemaRef, Current).get();
    break;
  case RK_Array:
    Deref = Current;
    break;
  default:
    break;
  }

  ExprResult RvalueDeref = SemaRef.DefaultLvalueConversion(Deref);

  assert(!RvalueDeref.isInvalid() && "Could not dereference range member.");
  return RvalueDeref.get();
}

RangeTraverser &
RangeTraverser::operator++()
{
  IntegerLiteral *Index =
  IntegerLiteral::Create(SemaRef.Context, llvm::APSInt::getUnsigned(1),
                         SemaRef.Context.getSizeType(), SourceLocation());
  const auto done = operator bool();
  if (!done) {
    ++I;
    ExprResult Next;
    switch (Kind) {
    case RK_Range:
      Next = BuildNextCall(SemaRef, Current, Index);
      break;
    case RK_Array:
      Next = BuildSubscriptAccess(SemaRef, Current, I);
      break;
    default:
      break;
    }

    if (Next.isInvalid())
      llvm_unreachable("Could not build next call.");
    Current = Next.get();
  }

  return *this;
}

static ExprResult
getAsCXXValueOfExpr(Sema &SemaRef, Expr *Expression,
                    SourceLocation EllipsisLoc = SourceLocation())
{
  // llvm::outs() << "The expression:\n";
  // Expression->dump();
  return SemaRef.ActOnCXXValueOfExpr (SourceLocation(), Expression,
                                      SourceLocation(), SourceLocation(),
                                      EllipsisLoc);
}

static ExprResult
getAsCXXIdExprExpr(Sema &SemaRef, Expr *Expression,
                   SourceLocation EllipsisLoc = SourceLocation())
{
  return SemaRef.ActOnCXXIdExprExpr(SourceLocation(), Expression,
                                    SourceLocation(), SourceLocation(),
                                    EllipsisLoc);
}

static ExprResult
getAsCXXReflectedDeclname(Sema &SemaRef, Expr *Expression)
{
  llvm::SmallVector<Expr *, 1> Parts = {Expression};

  UnqualifiedId Result;
  if (SemaRef.BuildDeclnameId(Parts, Result, SourceLocation(), SourceLocation()))
    return ExprError();

  CXXScopeSpec TempSS;
  ExprResult BuiltExpr =
    SemaRef.ActOnIdExpression(SemaRef.getCurScope(), TempSS,
                              SourceLocation(), Result,
                              /*HasTrailingLParen=*/false,
                              /*IsAddresOfOperand=*/false);

  if (BuiltExpr.isInvalid())
    return ExprError();
  return BuiltExpr;

}

static QualType
getAsCXXReflectedType(Sema &SemaRef, Expr *Expression)
{
  return SemaRef.BuildReflectedType(SourceLocation(), Expression);
}

bool
Sema::ActOnVariadicReifier(llvm::SmallVectorImpl<Expr *> &Expressions,
                           SourceLocation KWLoc, IdentifierInfo *KW,
                           Expr *Range, SourceLocation LParenLoc,
                           SourceLocation EllipsisLoc, SourceLocation RParenLoc)
{
  // ExpansionStatementBuilder Bldr(*this, getCurScope(), BFRK_Build, Range);
  ExpansionContextBuilder CtxBldr(*this, getCurScope(), Range);
  if (CtxBldr.BuildCalls())
    ; // TODO: Diag << failed to build calls
  // StmtResult ExpansionResult = (Bldr.BuildUninstantiated());
  // CXXExpansionStmt *Expansion = cast<CXXExpansionStmt>(ExpansionResult.get());

  ExprResult C;

  // If the expansion is dependent, we cannot expand on it yet.
  // Just return an empty reifier containing the Range and EllipsisLoc.
  if (CtxBldr.getKind() == RK_Unknown) {
    C = ActOnCXXDependentVariadicReifierExpr(Range, KWLoc, KW, LParenLoc,
                                             EllipsisLoc, RParenLoc);
    Expressions.push_back(C.get());
    return false;
  }
  // Traverse the range now and add the exprs to the vector
  RangeTraverser Traverser(*this,
                           CtxBldr.getKind(),
                           CtxBldr.getRangeBeginCall(),
                           CtxBldr.getRangeEndCall());

  while (!Traverser) {
    switch (KW->getTokenID()) {
    case tok::kw_valueof:
      C = getAsCXXValueOfExpr(*this, *Traverser);
      break;
    case tok::kw_idexpr:
      C = getAsCXXIdExprExpr(*this, *Traverser);
      break;
    case tok::kw_unqualid:
      C = getAsCXXReflectedDeclname(*this, *Traverser);
      break;
    case tok::kw_typename:
      Diag(KWLoc, diag::err_invalid_reifier_context) << 3 << 0;
      return true;
    case tok::kw_namespace:
      Diag(KWLoc, diag::err_namespace_as_variadic_reifier);
      return true;
    default: // silence warning
      break;
    }

    if (!C.isInvalid() && C.isUsable())
      Expressions.push_back(C.get());
    else
      return true;

    ++Traverser;
  }

  return false;
}

bool
Sema::ActOnVariadicReifier(llvm::SmallVectorImpl<QualType> &Types,
                           SourceLocation KWLoc, Expr *Range,
                           SourceLocation LParenLoc, SourceLocation EllipsisLoc,
                           SourceLocation RParenLoc)
{
  ExpansionContextBuilder CtxBldr(*this, getCurScope(), Range);
  CtxBldr.BuildCalls();

  if (CtxBldr.getKind() == RK_Unknown) {
    QualType T =
      Context.getCXXDependentVariadicReifierType(Range, KWLoc,
                                                 RParenLoc, EllipsisLoc);
    // TODO: remove this flag from qualtype
    // T.isVariadicReifier = true;
    // CXXDependentVariadicReifierType T(Context, Range);
    Types.push_back(T);
    return false;
  }

  // Traverse the range now and add the exprs to the vector
  RangeTraverser Traverser(*this,
                           CtxBldr.getKind(),
                           CtxBldr.getRangeBeginCall(),
                           CtxBldr.getRangeEndCall());

  while(!Traverser) {
    QualType T = getAsCXXReflectedType(*this, *Traverser);

    if (T.isNull())
      return true;

    Types.push_back(T);

    ++Traverser;
  }

  return false;
}

ExprResult Sema::ActOnCXXValueOfExpr(SourceLocation KWLoc,
                                     Expr *Refl,
                                     SourceLocation LParenLoc,
                                     SourceLocation RParenLoc,
                                     SourceLocation EllipsisLoc)
{
  if (Refl->isTypeDependent() || Refl->isValueDependent())
    return new (Context) CXXValueOfExpr(Context.DependentTy, Refl, KWLoc,
                                        LParenLoc, LParenLoc, EllipsisLoc);

  if (!CheckReflectionOperand(*this, Refl))
    return ExprError();

  Reflection R = EvaluateReflection(*this, Refl);
  if (R.isInvalid())
    return ExprError();

  Expr *Eval = ReflectionToValueExpr(*this, R, KWLoc);
  if (!Eval) {
    Diag(Refl->getExprLoc(), diag::err_expression_not_value_reflection);
    return ExprError();
  }

  // Evaluate the resulting expression.
  SmallVector<PartialDiagnosticAt, 4> Diags;
  Expr::EvalResult Result;
  Result.Diag = &Diags;
  if (!Eval->EvaluateAsAnyValue(Result, Context)) {
    Diag(Eval->getExprLoc(), diag::reflection_reflects_non_constant_expression);
    for (PartialDiagnosticAt PD : Diags)
      Diag(PD.first, PD.second);
    return ExprError();
  }

  return new (Context) CXXConstantExpr(Eval, std::move(Result.Val));
}

ExprResult Sema::ActOnCXXDependentVariadicReifierExpr(Expr *Range,
                                                      SourceLocation KWLoc,
                                                      IdentifierInfo *KW,
                                                      SourceLocation LParenLoc,
                                                      SourceLocation EllipsisLoc,
                                                      SourceLocation RParenLoc)
{
  // If the dependent reifier isn't dependent, something has gone wrong.
  assert((Range->isTypeDependent() || Range->isValueDependent()) &&
         "Marked a non-dependent variadic reifier as dependent.");

  return new (Context) CXXDependentVariadicReifierExpr(Context.DependentTy,
                                                       Range, KWLoc, KW,
                                                       LParenLoc, LParenLoc,
                                                       EllipsisLoc);
}


static bool AppendStringValue(Sema& S, llvm::raw_ostream& OS,
                              const APValue& Val) {
  // Extracting the string value from the LValue.
  //
  // FIXME: We probably want something like EvaluateAsString in the Expr class.
  APValue::LValueBase Base = Val.getLValueBase();
  if (Base.is<const Expr *>()) {
    const Expr *BaseExpr = Base.get<const Expr *>();
    assert(isa<StringLiteral>(BaseExpr) && "Not a string literal");
    const StringLiteral *Str = cast<StringLiteral>(BaseExpr);
    OS << Str->getString();
  } else {
    llvm_unreachable("Use of string variable not implemented");
    // const ValueDecl *D = Base.get<const ValueDecl *>();
    // return Error(E->getMessage());
  }
  return true;
}

static bool AppendCharacterArray(Sema& S, llvm::raw_ostream &OS, Expr *E,
                                 QualType T) {
  assert(T->isArrayType() && "Not an array type");
  const ArrayType *ArrayTy = cast<ArrayType>(T.getTypePtr());

  // Check that the type is 'const char[N]' or 'char[N]'.
  QualType ElemTy = ArrayTy->getElementType();
  if (!ElemTy->isCharType()) {
    S.Diag(E->getBeginLoc(), diag::err_reflected_id_invalid_operand_type) << T;
    return false;
  }

  // Evaluate the expression.
  Expr::EvalResult Result;
  if (!E->EvaluateAsLValue(Result, S.Context)) {
    // FIXME: Include notes in the diagnostics.
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }

  return AppendStringValue(S, OS, Result.Val);
}

static bool AppendCharacterPointer(Sema& S, llvm::raw_ostream &OS, Expr *E,
                                   QualType T) {
  assert(T->isPointerType() && "Not a pointer type");
  const PointerType* PtrTy = cast<PointerType>(T.getTypePtr());

  // Check for 'const char*'.
  QualType ElemTy = PtrTy->getPointeeType();
  if (!ElemTy->isCharType() || !ElemTy.isConstQualified()) {
    S.Diag(E->getBeginLoc(), diag::err_reflected_id_invalid_operand_type) << T;
    return false;
  }

  // Try evaluating the expression as an rvalue and then extract the result.
  Expr::EvalResult Result;
  if (!E->EvaluateAsRValue(Result, S.Context)) {
    // FIXME: This is not the right error.
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }

  return AppendStringValue(S, OS, Result.Val);
}

static bool AppendInteger(Sema& S, llvm::raw_ostream &OS, Expr *E, QualType T) {
  llvm::APSInt N;
  if (!E->EvaluateAsInt(N, S.Context)) {
    S.Diag(E->getBeginLoc(), diag::err_expr_not_ice) << 1;
    return false;
  }
  OS << N;
  return true;
}

static inline bool
AppendReflectedDecl(Sema &S, llvm::raw_ostream &OS, const Expr *ReflExpr,
                    const Decl *D) {
  // If this is a named declaration, append its identifier.
  if (!isa<NamedDecl>(D)) {
    // FIXME: Improve diagnostics.
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflection_not_named);
    return false;
  }
  const NamedDecl *ND = cast<NamedDecl>(D);

  // FIXME: What if D has a special name? For example operator==?
  // What would we append in that case?
  DeclarationName Name = ND->getDeclName();
  if (!Name.isIdentifier()) {
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflected_id_not_an_identifer) << Name;
    return false;
  }

  OS << ND->getName();
  return true;
}

static inline bool
AppendReflectedType(Sema& S, llvm::raw_ostream &OS, const Expr *ReflExpr,
                    const QualType &QT) {
  const Type *T = QT.getTypePtr();

  // If this is a class type, append its identifier.
  if (auto *RC = T->getAsCXXRecordDecl())
    OS << RC->getName();
  else if (const BuiltinType *BT = T->getAs<BuiltinType>())
    OS << BT->getName();
  else {
    S.Diag(ReflExpr->getBeginLoc(), diag::err_reflected_id_not_an_identifer)
      << QualType(T, 0);
    return false;
  }
  return true;
}

static bool
AppendReflection(Sema& S, llvm::raw_ostream &OS, Expr *E) {
  Reflection Refl = EvaluateReflection(S, E);

  switch (Refl.getKind()) {
  case RK_invalid:
    llvm_unreachable("Should already be validated");

  case RK_type: {
    const QualType QT = Refl.getAsType();
    return AppendReflectedType(S, OS, E, QT);
  }

  case RK_declaration: {
    const Decl *D = Refl.getAsDeclaration();
    return AppendReflectedDecl(S, OS, E, D);
  }

  case RK_expression: {
    const Expr *RE = Refl.getAsExpression();
    if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(RE))
      return AppendReflectedDecl(S, OS, E, DRE->getDecl());
    break;
  }

  case RK_base_specifier:
    break;
  }

  llvm_unreachable("Unsupported reflection type");
}

/// Constructs a new identifier from the expressions in Parts. Returns nullptr
/// on error.
DeclarationNameInfo Sema::BuildReflectedIdName(SourceLocation BeginLoc,
                                               SmallVectorImpl<Expr *> &Parts,
                                               SourceLocation EndLoc) {

  // If any components are dependent, we can't compute the name.
  if (HasDependentParts(Parts)) {
    DeclarationName Name
      = Context.DeclarationNames.getCXXReflectedIdName(Parts.size(), &Parts[0]);
    DeclarationNameInfo NameInfo(Name, BeginLoc);
    NameInfo.setCXXReflectedIdNameRange({BeginLoc, EndLoc});
    return NameInfo;
  }

  SmallString<256> Buf;
  llvm::raw_svector_ostream OS(Buf);
  for (std::size_t I = 0; I < Parts.size(); ++I) {
    Expr *E = Parts[I];

    assert(!E->isTypeDependent() && !E->isValueDependent()
        && "Dependent name component");

    // Get the type of the reflection.
    QualType T = DeduceCanonicalType(*this, E);

    SourceLocation ExprLoc = E->getBeginLoc();

    // Evaluate the sub-expression (depending on type) in order to compute
    // a string part that will constitute a declaration name.
    if (T->isConstantArrayType()) {
      if (!AppendCharacterArray(*this, OS, E, T))
        return DeclarationNameInfo();
    }
    else if (T->isPointerType()) {
      if (!AppendCharacterPointer(*this, OS, E, T))
        return DeclarationNameInfo();
    }
    else if (T->isIntegerType()) {
      if (I == 0) {
        // An identifier cannot start with an integer value.
        Diag(ExprLoc, diag::err_reflected_id_with_integer_prefix);
        return DeclarationNameInfo();
      }
      if (!AppendInteger(*this, OS, E, T))
        return DeclarationNameInfo();
    }
    else if (CheckReflectionOperand(*this, E)) {
      if (!AppendReflection(*this, OS, E))
        return DeclarationNameInfo();
    }
    else {
      Diag(ExprLoc, diag::err_reflected_id_invalid_operand_type) << T;
      return DeclarationNameInfo();
    }
  }

  // FIXME: Should we always return a declaration name?
  IdentifierInfo *Id = &PP.getIdentifierTable().get(Buf);
  DeclarationName Name = Context.DeclarationNames.getIdentifier(Id);
  return DeclarationNameInfo(Name, BeginLoc);
}

/// Handle construction of non-dependent identifiers, and test to
/// see if the identifier is a template.
bool Sema::BuildInitialDeclnameId(SourceLocation BeginLoc, CXXScopeSpec SS,
                                  const DeclarationName &Name,
                                  SourceLocation TemplateKWLoc,
                                  TemplateNameKind &TNK,
                                  TemplateTy &Template,
                                  UnqualifiedId &Result) {
  if (Name.getNameKind() == DeclarationName::CXXReflectedIdName)
    return false;

  Result.setIdentifier(Name.getAsIdentifierInfo(), BeginLoc);

  bool MemberOfUnknownSpecialization;

  TNK = isTemplateName(getCurScope(), SS, TemplateKWLoc.isValid(),
                       Result,
                       /*ObjectType=*/nullptr, // FIXME: This is most likely wrong
                       /*EnteringContext=*/false, Template,
                       MemberOfUnknownSpecialization);

  return false;
}

/// Handle construction of dependent identifiers, and additionally complete
/// template identifiers.
bool Sema::CompleteDeclnameId(SourceLocation BeginLoc, CXXScopeSpec SS,
                              const DeclarationName &Name,
                              SourceLocation TemplateKWLoc,
                              TemplateNameKind TNK, TemplateTy Template,
                              SourceLocation LAngleLoc,
                              ASTTemplateArgsPtr TemplateArgsPtr,
                              SourceLocation RAngleLoc,
                           SmallVectorImpl<TemplateIdAnnotation *> &CleanupList,
                              UnqualifiedId &Result, SourceLocation EndLoc) {
  if (Name.getNameKind() == DeclarationName::CXXReflectedIdName) {
    auto *ReflectedId = new (Context) ReflectedIdentifierInfo();
    ReflectedId->setNameComponents(Name.getCXXReflectedIdArguments());

    if (LAngleLoc.isValid()) {
      ReflectedId->setTemplateKWLoc(TemplateKWLoc);
      ReflectedId->setLAngleLoc(LAngleLoc);
      ReflectedId->setTemplateArgs(TemplateArgsPtr);
      ReflectedId->setRAngleLoc(RAngleLoc);
    }

    Result.setReflectedId(BeginLoc, ReflectedId, EndLoc);
  } else if (TNK) {
    TemplateIdAnnotation *TemplateIdAnnotation = TemplateIdAnnotation::Create(
          SS, TemplateKWLoc, /*TemplateNameLoc=*/BeginLoc,
          Name.getAsIdentifierInfo(), /*OperatorKind=*/OO_None,
          /*OpaqueTemplateName=*/Template, /*TemplateKind=*/TNK,
          /*LAngleLoc=*/LAngleLoc, /*RAngleLoc=*/RAngleLoc, TemplateArgsPtr,
          CleanupList);

    Result.setTemplateId(TemplateIdAnnotation);
  }

  return false;
}

ExprResult Sema::ActOnCXXUnreflexprExpression(SourceLocation Loc,
                                              Expr *Reflection) {
  return BuildCXXUnreflexprExpression(Loc, Reflection);
}

ExprResult Sema::BuildCXXUnreflexprExpression(SourceLocation Loc,
                                              Expr *E) {
  // TODO: Process the reflection E, UnresolveLookupExpr

  // Don't act on dependent expressions, just preserve them.
  // if (E->isTypeDependent() || E->isValueDependent())
  //   return new (Context) CXXUnreflexprExpr(E, Context.DependentTy,
  //                                          VK_RValue, OK_Ordinary, Loc);

  // The operand must be a reflection.
  // if (!CheckReflectionOperand(*this, E))
  //   return ExprError();

  // TODO: Process the reflection E, into DeclRefExpr

  // CXXUnreflexprExpr *Val =
  //     new (Context) CXXUnreflexprExpr(E, E->getType(), E->getValueKind(),
  //                                     E->getObjectKind(), Loc);

  // return Val;
  return ExprError();
}

/// Evaluates the given expression and yields the computed type.
QualType Sema::BuildReflectedType(SourceLocation TypenameLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return Context.getReflectedType(E, Context.DependentTy);

  // The operand must be a reflection.
  if (!CheckReflectionOperand(*this, E))
    return QualType();

  Reflection Refl = EvaluateReflection(*this, E);
  if (Refl.isInvalid())
    return QualType();

  if (!Refl.isType()) {
    Diag(E->getExprLoc(), diag::err_expression_not_type_reflection);
    return QualType();
  }

  // Get the type of the reflected entity.
  QualType Reflected = Refl.getAsType();

  return Context.getReflectedType(E, Reflected);
}

/// Evaluates the given expression and yields the computed type.
TypeResult Sema::ActOnReflectedTypeSpecifier(SourceLocation TypenameLoc,
                                             Expr *E) {
  QualType T = BuildReflectedType(TypenameLoc, E);
  if (T.isNull())
    return TypeResult(true);

  // FIXME: Add parens?
  TypeLocBuilder TLB;
  ReflectedTypeLoc TL = TLB.push<ReflectedTypeLoc>(T);
  TL.setNameLoc(TypenameLoc);
  TypeSourceInfo *TSI = TLB.getTypeSourceInfo(Context, T);
  return CreateParsedType(T, TSI);
}

static ParsedTemplateArgument
BuildDependentTemplarg(SourceLocation KWLoc, Expr *ReflExpr) {
  void *OpaqueReflexpr = reinterpret_cast<void *>(ReflExpr);
  return ParsedTemplateArgument(ParsedTemplateArgument::Dependent,
                                OpaqueReflexpr, KWLoc);
}

static ParsedTemplateArgument
BuildReflectedTypeTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *ReflExpr, const QualType &QT) {
  llvm::outs() << "Templarg - Type\n";
  const Type *T = QT.getTypePtr();
  void *OpaqueT = reinterpret_cast<void *>(const_cast<Type *>(T));
  return ParsedTemplateArgument(ParsedTemplateArgument::Type, OpaqueT, KWLoc);
}

static ParsedTemplateArgument
BuildReflectedDeclTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *ReflExpr, const Decl *D) {
  llvm::outs() << "Templarg - Decl\n";
  if (const TemplateDecl *TDecl = dyn_cast<TemplateDecl>(D)) {
    using TemplateTy = OpaquePtr<TemplateName>;
    TemplateTy Template = TemplateTy::make(
        TemplateName(const_cast<TemplateDecl *>(TDecl)));

    return ParsedTemplateArgument(ParsedTemplateArgument::Template,
                                  Template.getAsOpaquePtr(),
                                  KWLoc);
  }

  llvm_unreachable("Unsupported reflected declaration type");
}

static ParsedTemplateArgument
BuildReflectedExprTemplarg(Sema &S, SourceLocation KWLoc,
                           Expr *E, const Expr *RE) {
  llvm::outs() << "Templarg - Expr\n";
  void *OpaqueRE = reinterpret_cast<void *>(const_cast<Expr *>(RE));
  return ParsedTemplateArgument(ParsedTemplateArgument::NonType, OpaqueRE,
                                KWLoc);
}

ParsedTemplateArgument
Sema::ActOnReflectedTemplateArgument(SourceLocation KWLoc, Expr *E) {
  if (E->isTypeDependent() || E->isValueDependent())
    return BuildDependentTemplarg(KWLoc, E);

  if (!CheckReflectionOperand(*this, E))
    return ParsedTemplateArgument();

  Reflection Refl = EvaluateReflection(*this, E);

  switch (Refl.getKind()) {
  case RK_invalid:
    return ParsedTemplateArgument();

  case RK_type: {
    const QualType QT = Refl.getAsType();
    return BuildReflectedTypeTemplarg(*this, KWLoc, E, QT);
  }

  case RK_declaration: {
    const Decl *D = Refl.getAsDeclaration();
    return BuildReflectedDeclTemplarg(*this, KWLoc, E, D);
  }

  case RK_expression: {
    const Expr *RE = Refl.getAsExpression();
    return BuildReflectedExprTemplarg(*this, KWLoc, E, RE);
  }

  case RK_base_specifier:
    break;
  }

  llvm_unreachable("Unsupported reflection type");
}

ExprResult
Sema::ActOnDependentMemberExpr(Expr *BaseExpr, QualType BaseType,
                               SourceLocation OpLoc, bool IsArrow,
                               Expr *IdExpr) {
  // TODO The non-reflection variant of this provides diagnostics
  // for some situations even in the dependent context. This is
  // something we should consider implementing for the reflection
  // variant.
  assert(BaseType->isDependentType() ||
         IdExpr->isTypeDependent() ||
         IdExpr->isValueDependent());

  // Get the type being accessed in BaseType.  If this is an arrow, the BaseExpr
  // must have pointer type, and the accessed type is the pointee.
  return CXXDependentScopeMemberExpr::Create(Context, BaseExpr, BaseType,
                                             IsArrow, OpLoc, IdExpr);
}


ExprResult Sema::ActOnMemberAccessExpr(Expr *Base,
                                       SourceLocation OpLoc,
                                       tok::TokenKind OpKind,
                                       Expr *IdExpr) {
  bool IsArrow = (OpKind == tok::arrow);

  if (IdExpr->isTypeDependent() || IdExpr->isValueDependent()) {
    return ActOnDependentMemberExpr(Base, Base->getType(), OpLoc, IsArrow,
                                    IdExpr);
  }

  return BuildMemberReferenceExpr(Base, Base->getType(), OpLoc, IsArrow,
                                  IdExpr);
}

static MemberExpr *BuildMemberExpr(
    Sema &SemaRef, ASTContext &C, Expr *Base, bool isArrow,
    SourceLocation OpLoc, const ValueDecl *Member, QualType Ty, ExprValueKind VK,
    ExprObjectKind OK) {
  assert((!isArrow || Base->isRValue()) && "-> base must be a pointer rvalue");
  ValueDecl *ModMember = const_cast<ValueDecl *>(Member);

  // TODO: See if we can improve the source code location information here
  DeclarationNameInfo DeclNameInfo(Member->getDeclName(), SourceLocation());
  DeclAccessPair DeclAccessPair = DeclAccessPair::make(ModMember,
                                                       ModMember->getAccess());

  MemberExpr *E = MemberExpr::Create(
      C, Base, isArrow, OpLoc, /*QualifierLoc=*/NestedNameSpecifierLoc(),
      /*TemplateKWLoc=*/SourceLocation(), ModMember, DeclAccessPair,
      DeclNameInfo, /*TemplateArgs=*/nullptr, Ty, VK, OK);
  SemaRef.MarkMemberReferenced(E);
  return E;
}

// TODO there is a lot of logic that has been duplicated between
// this and the non-reflection variant of BuildMemberReferenceExpr,
// we should examine how we might be able to reduce said duplication.
ExprResult
Sema::BuildMemberReferenceExpr(Expr *BaseExpr, QualType BaseExprType,
                               SourceLocation OpLoc, bool IsArrow,
                               Expr *IdExpr) {
  assert(isa<DeclRefExpr>(IdExpr) && "must be a DeclRefExpr");

  // C++1z [expr.ref]p2:
  //   For the first option (dot) the first expression shall be a glvalue [...]
  if (!IsArrow && BaseExpr && BaseExpr->isRValue()) {
    ExprResult Converted = TemporaryMaterializationConversion(BaseExpr);
    if (Converted.isInvalid())
      return ExprError();
    BaseExpr = Converted.get();
  }

  const Decl *D = cast<DeclRefExpr>(IdExpr)->getDecl();

  if (const FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
    // x.a is an l-value if 'a' has a reference type. Otherwise:
    // x.a is an l-value/x-value/pr-value if the base is (and note
    //   that *x is always an l-value), except that if the base isn't
    //   an ordinary object then we must have an rvalue.
    ExprValueKind VK = VK_LValue;
    ExprObjectKind OK = OK_Ordinary;
    if (!IsArrow) {
      if (BaseExpr->getObjectKind() == OK_Ordinary)
        VK = BaseExpr->getValueKind();
      else
        VK = VK_RValue;
    }
    if (VK != VK_RValue && FD->isBitField())
      OK = OK_BitField;

    // Figure out the type of the member; see C99 6.5.2.3p3, C++ [expr.ref]
    QualType MemberType = FD->getType();
    if (const ReferenceType *Ref = MemberType->getAs<ReferenceType>()) {
      MemberType = Ref->getPointeeType();
      VK = VK_LValue;
    } else {
      QualType BaseType = BaseExpr->getType();
      if (IsArrow) BaseType = BaseType->getAs<PointerType>()->getPointeeType();

      Qualifiers BaseQuals = BaseType.getQualifiers();

      // GC attributes are never picked up by members.
      BaseQuals.removeObjCGCAttr();

      // CVR attributes from the base are picked up by members,
      // except that 'mutable' members don't pick up 'const'.
      if (FD->isMutable()) BaseQuals.removeConst();

      Qualifiers MemberQuals =
          Context.getCanonicalType(MemberType).getQualifiers();

      assert(!MemberQuals.hasAddressSpace());

      Qualifiers Combined = BaseQuals + MemberQuals;
      if (Combined != MemberQuals)
        MemberType = Context.getQualifiedType(MemberType, Combined);
    }

    return BuildMemberExpr(*this, Context, BaseExpr, IsArrow, OpLoc,
                           FD, MemberType, VK, OK);
  }

  if (const CXXMethodDecl *MemberFn = dyn_cast<CXXMethodDecl>(D)) {
    QualType MemberTy;
    ExprValueKind VK;
    if (MemberFn->isInstance()) {
      MemberTy = Context.BoundMemberTy;
      VK = VK_RValue;
    } else {
      MemberTy = MemberFn->getType();
      VK = VK_LValue;
    }

    return BuildMemberExpr(*this, Context, BaseExpr, IsArrow, OpLoc,
                           MemberFn, MemberTy, VK, OK_Ordinary);
  }

  llvm_unreachable("Unsupported decl type");
}

ExprResult Sema::ActOnCXXConcatenateExpr(SmallVectorImpl<Expr *>& Parts,
                                         SourceLocation KWLoc,
                                         SourceLocation LParenLoc,
                                         SourceLocation RParenLoc) {
  return BuildCXXConcatenateExpr(Parts, KWLoc);
}

static bool CheckConcatenateOperand(QualType T) {
  if (T.isCXXStringLiteralType())
    return true;
  if (T->isIntegerType())
    return true;

  return false;
}

static bool CheckConcatenateOperand(Sema &SemaRef, Expr *E) {
  QualType T = E->getType();
  if (CheckConcatenateOperand(T))
    return true;

  SemaRef.Diag(E->getExprLoc(), diag::err_concatenate_wrong_operand_type);
  return false;
}

ExprResult Sema::BuildCXXConcatenateExpr(SmallVectorImpl<Expr *>& Parts,
                                         SourceLocation KWLoc) {
  // The type of the expression is 'const char *'.
  QualType StrTy = Context.getPointerType(Context.getConstType(Context.CharTy));

  // Look for dependent arguments.
  for (Expr *E : Parts) {
    if (E->isTypeDependent() || E->isValueDependent())
      return new (Context) CXXConcatenateExpr(Context, StrTy, KWLoc, Parts);
  }

  // Convert operands to rvalues.
  SmallVector<Expr*, 4> Converted;
  for (Expr *E : Parts) {
    // Decay arrays first.
    ExprResult R = DefaultFunctionArrayLvalueConversion(E);
    if (R.isInvalid())
      return ExprError();
    E = R.get();

    // Check that the operand (type) is acceptable.
    if (!CheckConcatenateOperand(*this, E))
      return ExprError();

    Converted.push_back(E);
  }

  return new (Context) CXXConcatenateExpr(Context, StrTy, KWLoc, Converted);
}

DiagnosticsEngine &Sema::FakeParseScope::getDiagnostics() {
  return SemaRef.PP.getDiagnostics();
}
