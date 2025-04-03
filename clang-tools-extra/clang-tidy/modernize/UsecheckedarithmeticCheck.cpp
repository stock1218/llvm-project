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

StatementMatcher makeStmtMatcher() {
  return binaryOperation(
          hasAnyOperatorName("+", "-", "*"),
          hasLHS(ignoringImpCasts(hasType(isInteger()))),
          hasLHS(ignoringImpCasts(anyOf(expr().bind("argOne"), integerLiteral().bind("argOne")))),
          hasRHS(ignoringImpCasts(hasType(isInteger()))),
          hasRHS(ignoringImpCasts(anyOf(expr().bind("argTwo"), integerLiteral().bind("argTwo")))))
        .bind("Statement");
}


std::string getCkdFunction(llvm::StringRef opStr) {
  if (opStr == "+") {
    return "ckd_add";
  } else if (opStr == "-") {
    return "ckd_sub";
  } else if (opStr == "*") {
    return "ckd_mul";
  } else {
    llvm::outs() << "Unknown operation to convert\n";
    return "";
  }
}

void UseCheckedArithmeticCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(traverse(TK_AsIs, makeStmtMatcher()), this);
}

std::string getExprSourceString(const Expr* expr, const SourceManager& sm, const LangOptions& langOpts) {
    const auto range = expr->getSourceRange();

    const auto start = sm.getSpellingLoc(range.getBegin());
    const auto end = sm.getSpellingLoc(range.getEnd());

    return Lexer::getSourceText(CharSourceRange::getTokenRange(start, end), sm, langOpts).str();
}

void UseCheckedArithmeticCheck::fixStmt(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedExpr = Result.Nodes.getNodeAs<BinaryOperator>("Statement");
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
    llvm::outs() << "Error converting operation.\n";
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

  fixStmt(Result);

  return;
}
} // namespace clang::tidy::modernize
