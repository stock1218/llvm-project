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
                        hasLHS(ignoringImpCasts(hasType(isInteger()))),
                        hasRHS(ignoringImpCasts(declRefExpr().bind("argTwo"))),
                        hasRHS(ignoringImpCasts(hasType(isInteger()))))
                        .bind("operator")))
      .bind("NonDeclOperation");
}

DeclarationMatcher makeDeclMatcher() {
  return varDecl(hasInitializer(ignoringImplicit(binaryOperator(
                         hasAnyOperatorName("+", "-", "*"),
                         hasLHS(ignoringImpCasts(declRefExpr().bind("argOne"))),
                         hasLHS(ignoringImpCasts(hasType(isInteger()))),
                         hasRHS(ignoringImpCasts(declRefExpr().bind("argTwo"))),
                         hasRHS(ignoringImpCasts(hasType(isInteger()))))
                         .bind("operator"))))
      .bind("DeclOperation");
}

DeclarationMatcher makeMultiDeclMatcher() {
  return varDecl(
             hasInitializer(ignoringImplicit(binaryOperator(
                     hasAnyOperatorName("+", "-", "*"),
                     hasLHS(ignoringImplicit(binaryOperator(hasAnyOperatorName("+", "-", "*"),
                                           hasLHS(ignoringImpCasts(
                                               declRefExpr().bind("argOne"))),
                                           hasRHS(ignoringImpCasts(
                                               declRefExpr().bind("argTwo"))))
                                .bind("opOne"))),
                     hasRHS(ignoringImpCasts(declRefExpr().bind("argThree"))))
                     .bind("opTwo"))))
      .bind("MultiDeclOperation");
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

  /*
  Finder->addMatcher(traverse(TK_AsIs, makeNonDeclMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeDeclMatcher()), this);
  Finder->addMatcher(traverse(TK_AsIs, makeMultiDeclMatcher()), this);
  */
  Finder->addMatcher(traverse(TK_AsIs, makeStmtMatcher()), this);
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
  llvm::outs() << "Arg one source: " << argOneSource;

  argTwoSource = getExprSourceString(argTwo, sm, langOpts);
  llvm::outs() << " -> Arg two source: " << argTwoSource << " | ";

  /*
  const auto argOneName = Result.Nodes.getNodeAs<DeclRefExpr>("argOne")->getNameInfo().getAsString();
  const auto argTwoName = Result.Nodes.getNodeAs<DeclRefExpr>("argTwo")->getNameInfo().getAsString();
  */

  const std::string ckdFunc = getCkdFunction(MatchedExpr->getOpcodeStr());

  if (ckdFunc == "") {
    llvm::outs() << "Error converting operation.\n";
    return;
  }

  llvm::outs() << " Expression type: " << resultType << "\n";

  auto replacement = "({ " + resultType + " tmp;\n";
  replacement += "if(" + ckdFunc + "(&tmp" + ", " + argOneSource + ", " + argTwoSource + ")) {\n";
  replacement += "assert(0);\n};"; 
  replacement += "tmp;})";

  DiagnosticBuilder Diag =
      diag(MatchedExpr->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateReplacement(MatchedExpr->getSourceRange(), replacement);

  Diag << IncludeInserter.createIncludeInsertion(
      Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()),
      "<stdckdint.h>");
  return;

}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {
  // TODO this will be used to do the rewrite

   /*
  if (Result.Nodes.getNodeAs<Expr>("NonDeclOperation")) {
    fixNonDeclOperation(Result);
  } else if (Result.Nodes.getNodeAs<VarDecl>("DeclOperation")) {
    fixDeclOperation(Result);
  } else if (Result.Nodes.getNodeAs<VarDecl>("MultiDeclOperation")) {
    fixMultiDeclOperation(Result);
  } else if (Result.Nodes.getNodeAs<Expr>("Statement")) {
  }
  */

  /*
  const auto *MatchedOperation =
      Result.Nodes.getNodeAs<BinaryOperator>("Statement");
  diag(MatchedOperation->getBeginLoc(), "Potential checked operation");
  */

  fixStmt(Result);

  return;
}
} // namespace clang::tidy::modernize
