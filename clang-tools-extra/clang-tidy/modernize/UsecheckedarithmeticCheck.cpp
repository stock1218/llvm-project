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

StatementMatcher makeNonDeclMatcher() {
  return binaryOperator(
             hasOperatorName("="), hasLHS(declRefExpr().bind("dest")),
             hasRHS(binaryOperator(
                        hasAnyOperatorName("+", "-", "*"),
                        hasLHS(ignoringImpCasts(declRefExpr().bind("argOne"))),
                        hasRHS(ignoringImpCasts(declRefExpr().bind("argTwo"))))
                        .bind("operator")))
      .bind("NonDeclOperation");
}

DeclarationMatcher makeDeclMatcher() {
  return varDecl(has(binaryOperator(
                         hasAnyOperatorName("+", "-", "*"),
                         hasLHS(ignoringImpCasts(declRefExpr().bind("argOne"))),
                         hasRHS(ignoringImpCasts(declRefExpr().bind("argTwo"))))
                         .bind("operator")))
      .bind("DeclOperation");
}

DeclarationMatcher makeMultiDeclMatcher() {
  return varDecl(
             has(binaryOperator(
                     hasAnyOperatorName("+", "-", "*"),
                     hasLHS(binaryOperator(hasAnyOperatorName("+", "-", "*"),
                                           hasLHS(ignoringImpCasts(
                                               declRefExpr().bind("argOne"))),
                                           hasRHS(ignoringImpCasts(
                                               declRefExpr().bind("argTwo"))))
                                .bind("opOne")),
                     hasRHS(ignoringImpCasts(declRefExpr().bind("argThree"))))
                     .bind("opTwo")))
      .bind("MultiDeclOperation");
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

  Finder->addMatcher(traverse(TK_AsIs, makeNonDeclMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeDeclMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeMultiDeclMatcher()), this);
}

void UseCheckedArithmeticCheck::fixNonDeclOperation(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedExpr = Result.Nodes.getNodeAs<Expr>("NonDeclOperation");

  const auto *dest = Result.Nodes.getNodeAs<DeclRefExpr>("dest");
  const auto *argOne = Result.Nodes.getNodeAs<DeclRefExpr>("argOne");
  const auto *argTwo = Result.Nodes.getNodeAs<DeclRefExpr>("argTwo");

  const auto *op = Result.Nodes.getNodeAs<BinaryOperator>("operator");
  const std::string ckdFunc = getCkdFunction(op->getOpcodeStr());

  if (ckdFunc == "") {
    llvm::outs() << "Error converting operation.\n";
    return;
  }

  auto replacement =
      "if(" + ckdFunc + "(&" + dest->getNameInfo().getAsString() + ", " +
      argOne->getNameInfo().getAsString() + ", " +
      argTwo->getNameInfo().getAsString() + ")) {\n" + "assert(0);\n" + "}\n";

  DiagnosticBuilder Diag = diag(dest->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateRemoval(MatchedExpr->getSourceRange());
  Diag << FixItHint::CreateInsertion(dest->getLocation(), replacement);
  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      "<stdckdint.h>");
  return;
}

void UseCheckedArithmeticCheck::fixDeclOperation(
    const MatchFinder::MatchResult &Result) {
  const auto *MatchedDecl = Result.Nodes.getNodeAs<VarDecl>("DeclOperation");

  const auto *argOne = Result.Nodes.getNodeAs<DeclRefExpr>("argOne");
  const auto *argTwo = Result.Nodes.getNodeAs<DeclRefExpr>("argTwo");

  const auto *op = Result.Nodes.getNodeAs<BinaryOperator>("operator");
  const std::string ckdFunc = getCkdFunction(op->getOpcodeStr());

  if (ckdFunc == "") {
    llvm::outs() << "Error converting operation.\n";
    return;
  }

  const auto destType = MatchedDecl->getInit()->getType().getAsString();
  const auto destName = MatchedDecl->getNameAsString();

  auto replacement =
      destType + " " + destName + ";\n" + "if(" + ckdFunc + "(&" + destName +
      ", " + argOne->getNameInfo().getAsString() + ", " +
      argTwo->getNameInfo().getAsString() + ")) {\n" + "assert(0);\n" + "}\n";

  DiagnosticBuilder Diag =
      diag(MatchedDecl->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedDecl->getSourceRange(),
                                       replacement);
  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedDecl->getBeginLoc()),
      "<stdckdint.h>");
  return;
}

void UseCheckedArithmeticCheck::fixMultiDeclOperation(
    const MatchFinder::MatchResult &Result) {

  const auto *MatchedDecl = Result.Nodes.getNodeAs<VarDecl>("MultiDeclOperation");

  const auto *argOne = Result.Nodes.getNodeAs<DeclRefExpr>("argOne");
  const auto *argTwo = Result.Nodes.getNodeAs<DeclRefExpr>("argTwo");
  const auto *argThree = Result.Nodes.getNodeAs<DeclRefExpr>("argThree");
  const auto *opOne = Result.Nodes.getNodeAs<BinaryOperator>("opOne");
  const auto *opTwo = Result.Nodes.getNodeAs<BinaryOperator>("opTwo");

  const auto destType = MatchedDecl->getInit()->getType().getAsString();
  const auto destName = MatchedDecl->getNameAsString();
  const auto argOneName = argOne->getNameInfo().getAsString();
  const auto argTwoName = argTwo->getNameInfo().getAsString();
  const auto argThreeName = argThree->getNameInfo().getAsString();

  const auto opOneFunc = getCkdFunction(opOne->getOpcodeStr());
  const auto opTwoFunc = getCkdFunction(opTwo->getOpcodeStr());

  if (opOneFunc == "" || opTwoFunc == "") {
    llvm::outs() << "Error converting operation.\n";
    return;
  }

  auto replacement = destType + " " + destName + ";\n";
  replacement += "if(" + opOneFunc + "(&" + destName + ", " + argOneName + ", " + argTwoName + ")) {\n";
  replacement += "assert(0);\n};";

  replacement += "if(" + opTwoFunc + "(&" + destName + ", " + destName + ", " + argThreeName + ")) {\n";
  replacement += "assert(0);\n}";

  DiagnosticBuilder Diag =
      diag(MatchedDecl->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedDecl->getSourceRange(),
                                       replacement);
  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedDecl->getBeginLoc()),
      "<stdckdint.h>");
  return;
}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {
  // TODO this will be used to do the rewrite

  if (Result.Nodes.getNodeAs<Expr>("NonDeclOperation")) {
    fixNonDeclOperation(Result);
  } else if (Result.Nodes.getNodeAs<VarDecl>("DeclOperation")) {
    fixDeclOperation(Result);
  } else if (Result.Nodes.getNodeAs<VarDecl>("MultiDeclOperation")) {
    fixMultiDeclOperation(Result);
  }

  return;
}
} // namespace clang::tidy::modernize
