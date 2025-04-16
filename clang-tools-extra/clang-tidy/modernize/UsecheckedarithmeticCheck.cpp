#include "UseCheckedArithmeticCheck.h"
#include "../utils/IncludeInserter.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void UseCheckedArithmeticCheck::registerPPCallbacks(
    const SourceManager &SM, Preprocessor *PP, Preprocessor *ModuleExpanderPP) {
  IncludeInserter.registerPreprocessor(PP);
}

StatementMatcher makeCastNonAssignmentMatcher() {
    return binaryOperator(hasAnyOperatorName("+", "-", "*"),
                          hasParent(expr(hasType(isInteger()), unless(hasType(isConstQualified()))).bind("parent-expr")),
                          hasLHS(ignoringImpCasts(hasType(isInteger()))),
                          hasLHS(ignoringImpCasts(expr().bind("argOne"))),
                          hasRHS(ignoringImpCasts(hasType(isInteger()))),
                          hasRHS(ignoringImpCasts(expr().bind("argTwo"))))
           .bind("Non-AssignmentOp");

}

StatementMatcher makeGenericNonAssignmentMatcher() {
    return binaryOperator(hasAnyOperatorName("+", "-", "*"),
                          hasLHS(ignoringImpCasts(hasType(isInteger()))),
                          hasLHS(ignoringImpCasts(expr().bind("argOne"))),
                          hasRHS(ignoringImpCasts(hasType(isInteger()))),
                          hasRHS(ignoringImpCasts(expr().bind("argTwo"))))
           .bind("Non-AssignmentOp");

}

StatementMatcher makeUnaryMatcher() {
  return unaryOperator(hasAnyOperatorName("++", "--"),
                       hasUnaryOperand(ignoringImpCasts(hasType(isInteger()))),
                       hasUnaryOperand(ignoringImpCasts(anyOf(
                           expr().bind("arg"), integerLiteral().bind("arg")))))
      .bind("UnaryOp");
}

StatementMatcher makeAssignmentMatcher() {
  return binaryOperation(
             hasAnyOperatorName("+=", "-=", "*="), isAssignmentOperator(),
             hasLHS(ignoringImpCasts(hasType(isInteger()))),
             hasLHS(ignoringImpCasts(
                 anyOf(expr().bind("dest"), integerLiteral().bind("dest")))),
             hasRHS(ignoringImpCasts(hasType(isInteger()))),
             hasRHS(ignoringImpCasts(
                 anyOf(expr().bind("arg"), integerLiteral().bind("arg")))))
      .bind("AssignmentOp");
}

std::string getCkdFunction(llvm::StringRef opStr) {
  if (opStr == "+" || opStr == "++" || opStr == "+=") {
    return "ckd_add";
  } else if (opStr == "-" || opStr == "--" || opStr == "-=") {
    return "ckd_sub";
  } else if (opStr == "*" || opStr == "*=") {
    return "ckd_mul";
  } else {
    llvm::errs() << "Unknown operation to convert: " << opStr << "\n";
    return "";
  }
}

void UseCheckedArithmeticCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(traverse(TK_AsIs, makeCastNonAssignmentMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeGenericNonAssignmentMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeAssignmentMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeUnaryMatcher()), this);
}

std::string getExprSourceString(const Expr *expr, const SourceManager &sm,
                                const LangOptions &langOpts) {
  const auto range = expr->getSourceRange();

  const auto start = sm.getSpellingLoc(range.getBegin());
  const auto end = sm.getSpellingLoc(range.getEnd());

  return Lexer::getSourceText(CharSourceRange::getTokenRange(start, end), sm,
                              langOpts)
      .str();
}

void UseCheckedArithmeticCheck::fixAssignmentOp(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedExpr =
      Result.Nodes.getNodeAs<BinaryOperator>("AssignmentOp");

  const auto resultType = MatchedExpr->getType().getAsString();

  const SourceManager &sm = *Result.SourceManager;
  const LangOptions &langOpts = Result.Context->getLangOpts();
  std::string destSource, argSource;
  std::string destType, argType;

  const clang::Expr *dest;
  const clang::Expr *arg;

  // If it's an expression
  if (Result.Nodes.getNodeAs<Expr>("dest")) {
    dest = Result.Nodes.getNodeAs<Expr>("dest");
  } else { // if it's an integer literal
    dest = Result.Nodes.getNodeAs<IntegerLiteral>("dest");
  }

  // Same logic for argTwo
  if (Result.Nodes.getNodeAs<Expr>("arg")) {
    arg = Result.Nodes.getNodeAs<Expr>("arg");
  } else {
    arg = Result.Nodes.getNodeAs<IntegerLiteral>("arg");
  }

  // Get appropriate checked function
  const std::string ckdFunc = getCkdFunction(MatchedExpr->getOpcodeStr());

  if (ckdFunc == "") {
    llvm::errs() << "Error converting operation:" << MatchedExpr->getOpcodeStr()
                 << "\n";
    return;
  }

  // Extract the source for dest and arg
  destSource = getExprSourceString(dest, sm, langOpts);
  argSource = getExprSourceString(arg, sm, langOpts);

  // Get types of dest and source
  destType = dest->getType().getAsString();
  argType = arg->getType().getAsString();


  auto replacement = "({ " + destType + "* dest = " + "&" + destSource + ";\n";
  replacement += argType + " arg = " + argSource + ";\n";
  replacement += "if(" + ckdFunc + "(dest, *dest, arg)) {";
  replacement += HandleCode + "\n};";
  replacement += "*dest;})";

  DiagnosticBuilder Diag =
      diag(MatchedExpr->getBeginLoc(), "assignment operation can be rewritten to use checked arithmetic");

  Diag << FixItHint::CreateReplacement(MatchedExpr->getSourceRange(),
                                       replacement);

  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      "<stdckdint.h>");
  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      HandleImport);
  return;
}

void UseCheckedArithmeticCheck::fixUnaryOp(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedExpr = Result.Nodes.getNodeAs<UnaryOperator>("UnaryOp");

  const SourceManager &sm = *Result.SourceManager;
  const LangOptions &langOpts = Result.Context->getLangOpts();
  std::string argSource, argType;

  const clang::Expr *arg = Result.Nodes.getNodeAs<Expr>("arg");

  // Extract the source with this expression
  argSource = getExprSourceString(arg, sm, langOpts);
  argType = arg->getType().getAsString();

  const std::string ckdFunc =
      getCkdFunction(MatchedExpr->getOpcodeStr(MatchedExpr->getOpcode()));

  if (ckdFunc == "") {
    llvm::errs() << "Error converting operation.\n";
    return;
  }

  std::string replacement;

  switch (MatchedExpr->getOpcode()) {
  case clang::UO_PreInc:
  case clang::UO_PreDec:
    // handle fix for prefix
    replacement = "({ " + argType + "* tmp = &" + argSource + ";\n";
    replacement += "if(" + ckdFunc + "(tmp, *tmp, 1)) {\n";
    replacement += HandleCode + "\n};";
    replacement += "*tmp;})";

    break;

  case clang::UO_PostInc:
  case clang::UO_PostDec:
    // handle fix for postfix
    replacement = "({ " + argType + "* tmp = &" + argSource + ";\n";
    replacement += argType + " oldTmp = *tmp;\n";
    replacement += "if(" + ckdFunc + "(tmp, *tmp, 1)) {\n";
    replacement += HandleCode + "\n};";
    replacement += "oldTmp;})";

    break;

  default:
    llvm::errs() << "Unknown unary operator: "
                 << MatchedExpr->getOpcodeStr(MatchedExpr->getOpcode()) << "\n";
    return;
  }

  DiagnosticBuilder Diag =
      diag(MatchedExpr->getBeginLoc(), "unary operation can be rewritten to use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedExpr->getSourceRange(),
                                       replacement);

  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      "<stdckdint.h>");
  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      HandleImport);
  return;
}

void UseCheckedArithmeticCheck::fixNonAssignmentOp(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedExpr =
      Result.Nodes.getNodeAs<BinaryOperator>("Non-AssignmentOp");

  std::string resultType;

  // If there is a cast above this node we need to use that type as the dest
  if(Result.Nodes.getNodeAs<Expr>("parent-expr")) {
    const auto result = Result.Nodes.getNodeAs<Expr>("parent-expr");
    resultType = result->getType().getAsString();

  } else { // otherwise we can just use the type it's going into
      resultType = MatchedExpr->getType().getAsString();
  }

  const SourceManager &sm = *Result.SourceManager;
  const LangOptions &langOpts = Result.Context->getLangOpts();
  std::string argOneSource, argTwoSource;
  std::string argOneType, argTwoType;

  const clang::Expr *argOne;
  const clang::Expr *argTwo;

  argOne = Result.Nodes.getNodeAs<Expr>("argOne");
  argTwo = Result.Nodes.getNodeAs<Expr>("argTwo");

  // Extract the source with this expression
  argOneSource = getExprSourceString(argOne, sm, langOpts);
  argTwoSource = getExprSourceString(argTwo, sm, langOpts);

  argOneType = argOne->getType().getAsString();
  argTwoType = argTwo->getType().getAsString();

  const std::string ckdFunc = getCkdFunction(MatchedExpr->getOpcodeStr());

  if (ckdFunc == "") {
    llvm::errs() << "Error converting operation:" << MatchedExpr->getOpcodeStr()
                 << "\n";
    return;
  }

  auto replacement = "({ " + resultType + " dest;\n";
  replacement += argOneType + " argOne = " + argOneSource + ";\n";
  replacement += argTwoType + " argTwo = " + argTwoSource + ";\n";
  replacement += "if(" + ckdFunc + "(&dest, argOne, argTwo)) {";
  replacement += HandleCode + "\n};";
  replacement += "dest;})";

  DiagnosticBuilder Diag =
      diag(MatchedExpr->getBeginLoc(), "non-assignment operation can be rewritten to use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedExpr->getSourceRange(),
                                       replacement);

  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      "<stdckdint.h>");
  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      HandleImport);
  return;
}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {

  if (HandleImport == "__unset") {
    HandleImport = "<assert.h>";
  }

  if (HandleCode == "__unset") {
    HandleCode = "assert(0);";
  }

  if (Result.Nodes.getNodeAs<BinaryOperator>("Non-AssignmentOp")) {
    fixNonAssignmentOp(Result);
  } else if (Result.Nodes.getNodeAs<BinaryOperator>("AssignmentOp")) {
    fixAssignmentOp(Result);
  } else if (Result.Nodes.getNodeAs<UnaryOperator>("UnaryOp")) {
    fixUnaryOp(Result);
  }

  return;
}
} // namespace clang::tidy::modernize
