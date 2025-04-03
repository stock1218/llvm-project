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

StatementMatcher makeNonAssignmentMatcher() {
  return binaryOperation(
          hasAnyOperatorName("+", "-", "*"),
          hasLHS(ignoringImpCasts(hasType(isInteger()))),
          hasLHS(ignoringImpCasts(anyOf(expr().bind("argOne"), integerLiteral().bind("argOne")))),
          hasRHS(ignoringImpCasts(hasType(isInteger()))),
          hasRHS(ignoringImpCasts(anyOf(expr().bind("argTwo"), integerLiteral().bind("argTwo")))))
        .bind("Non-AssignmentOp");
}

StatementMatcher makeUnaryMatcher() {
  return unaryOperator(
          hasAnyOperatorName("++", "--"),
          hasUnaryOperand(ignoringImpCasts(hasType(isInteger()))),
          hasUnaryOperand(ignoringImpCasts(anyOf(expr().bind("arg"), integerLiteral().bind("argO")))))
        .bind("UnaryOp");
}

StatementMatcher makeAssignmentMatcher() {
  return binaryOperation(
          hasAnyOperatorName("+=", "-=", "*="),
          isAssignmentOperator(),
          hasLHS(ignoringImpCasts(hasType(isInteger()))),
          hasLHS(ignoringImpCasts(anyOf(expr().bind("dest"), integerLiteral().bind("dest")))),
          hasRHS(ignoringImpCasts(hasType(isInteger()))),
          hasRHS(ignoringImpCasts(anyOf(expr().bind("arg"), integerLiteral().bind("arg")))))
        .bind("AssignmentOp");
}


std::string getCkdFunction(llvm::StringRef opStr) {
  if (opStr == "+" || "++" || "+=") {
    return "ckd_add";
  } else if (opStr == "-" || "--" || "-=") {
    return "ckd_sub";
  } else if (opStr == "*" || "*=") {
    return "ckd_mul";
  } else {
    llvm::errs() << "Unknown operation to convert: " << opStr << "\n";
    return "";
  }
}

void UseCheckedArithmeticCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(traverse(TK_AsIs, makeNonAssignmentMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeAssignmentMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeUnaryMatcher()), this);
}

std::string getExprSourceString(const Expr* expr, const SourceManager& sm, const LangOptions& langOpts) {
    const auto range = expr->getSourceRange();

    const auto start = sm.getSpellingLoc(range.getBegin());
    const auto end = sm.getSpellingLoc(range.getEnd());

    return Lexer::getSourceText(CharSourceRange::getTokenRange(start, end), sm, langOpts).str();
}

void UseCheckedArithmeticCheck::fixAssignmentOp(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedExpr =
      Result.Nodes.getNodeAs<BinaryOperator>("AssignmentOp");

  const auto resultType = MatchedExpr->getType().getAsString();

  const SourceManager &sm = *Result.SourceManager;
  const LangOptions &langOpts = Result.Context->getLangOpts();
  std::string argOneSource;

  const clang::Expr* arg;

  // If it's an expression
  if(Result.Nodes.getNodeAs<Expr>("arg")) {
      arg = Result.Nodes.getNodeAs<Expr>("arg");
  } else { // if it's an integer literal
      arg = Result.Nodes.getNodeAs<IntegerLiteral>("arg");
  }

  // Extract the source with this expression
  argOneSource = getExprSourceString(arg, sm, langOpts);

  const std::string ckdFunc = getCkdFunction(MatchedExpr->getOpcodeStr());

  if (ckdFunc == "") {
    llvm::errs() << "Error converting operation:" << MatchedExpr->getOpcodeStr() << "\n";
    return;
  }

  auto replacement = "({ " + resultType + " tmp;\n";
  replacement += "if(" + ckdFunc + "(&tmp" + ", " + argOneSource + ", " + argOneSource + ")) {\n";
  replacement += HandleCode + "\n};"; 
  replacement += "tmp;})";

  DiagnosticBuilder Diag =
      diag(MatchedExpr->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedExpr->getSourceRange(), replacement);

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
  const auto *MatchedExpr =
      Result.Nodes.getNodeAs<UnaryOperator>("UnaryOp");

  const auto resultType = MatchedExpr->getType().getAsString();

  const SourceManager &sm = *Result.SourceManager;
  const LangOptions &langOpts = Result.Context->getLangOpts();
  std::string argOneSource;

  const clang::Expr* arg = Result.Nodes.getNodeAs<Expr>("arg");

  // Extract the source with this expression
  argOneSource = getExprSourceString(arg, sm, langOpts);

  const std::string ckdFunc = getCkdFunction(MatchedExpr->getOpcodeStr(MatchedExpr->getOpcode()));

  if (ckdFunc == "") {
    llvm::errs() << "Error converting operation.\n";
    return;
  }

  std::string replacement;

  switch (MatchedExpr->getOpcode()) {
    case clang::UO_PreInc:
    case clang::UO_PreDec:
        // handle fix for prefix 
        replacement = "({ " + resultType + " tmp;\n";
        replacement += "if(" + ckdFunc + "(&tmp" + ", " + argOneSource + ", " + argOneSource + ")) {\n";
        replacement += HandleCode + "\n};"; 
        replacement += "tmp;})";

        break;

    case clang::UO_PostInc:
    case clang::UO_PostDec:
        // handle fix for postfix
        replacement = "({ " + resultType + " tmpOld = " + argOneSource + ";\n";
        replacement = resultType + " tmp;\n";
        replacement += "if(" + ckdFunc + "(&tmp" + ", " + argOneSource + ", " + argOneSource + ")) {\n";
        replacement += HandleCode + "\n};"; 
        replacement += "tmpOld;})";
        break;

    default:
        llvm::errs() << "Unknown unary operator: " << MatchedExpr->getOpcodeStr(MatchedExpr->getOpcode()) << "\n";
        return;
  }


  DiagnosticBuilder Diag =
      diag(MatchedExpr->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedExpr->getSourceRange(), replacement);

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
  const auto *MatchedExpr = Result.Nodes.getNodeAs<BinaryOperator>("Non-AssignmentOp");
  const auto resultType = MatchedExpr->getType().getAsString();

  const SourceManager &sm = *Result.SourceManager;
  const LangOptions &langOpts = Result.Context->getLangOpts();
  std::string argOneSource, argTwoSource;

  const clang::Expr* argOne;
  const clang::Expr* argTwo;

  // If it's an expression
  if(Result.Nodes.getNodeAs<Expr>("argOne")) {
      argOne = Result.Nodes.getNodeAs<Expr>("argOne");
  } else { // if it's an integer literal
      argOne = Result.Nodes.getNodeAs<IntegerLiteral>("argOne");
  }

  // Same logic for argTwo
  if(Result.Nodes.getNodeAs<Expr>("argTwo")) {
      argTwo = Result.Nodes.getNodeAs<Expr>("argTwo");
  } else {
      argTwo = Result.Nodes.getNodeAs<IntegerLiteral>("argTwo");
  }

  // Extract the source with this expression
  argOneSource = getExprSourceString(argOne, sm, langOpts);

  argTwoSource = getExprSourceString(argTwo, sm, langOpts);

  const std::string ckdFunc = getCkdFunction(MatchedExpr->getOpcodeStr());

  if (ckdFunc == "") {
    llvm::errs() << "Error converting operation:" << MatchedExpr->getOpcodeStr() << "\n";
    return;
  }

  auto replacement = "({ " + resultType + " tmp;\n";
  replacement += "if(" + ckdFunc + "(&tmp" + ", " + argOneSource + ", " + argTwoSource + ")) {\n";
  replacement += HandleCode + "\n};"; 
  replacement += "tmp;})";

  DiagnosticBuilder Diag =
      diag(MatchedExpr->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedExpr->getSourceRange(), replacement);

  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      "<stdckdint.h>");
  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      HandleImport);
  return;

}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {

  if(HandleImport == "__unset") {
      HandleImport = "<assert.h>";
  }

  if(HandleCode == "__unset") {
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
