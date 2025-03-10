#include "UseCheckedArithmeticCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void UseCheckedArithmeticCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(traverse(TK_AsIs, binaryOperator().bind("test")), this);

}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {
  // TODO this will be used to do the rewrite
  const auto *test = Result.Nodes.getNodeAs<Expr>("test");
  SourceLocation BeginLoc = test->getBeginLoc();
  SourceLocation EndLoc = test->getEndLoc();
  auto Diag = diag(BeginLoc, "matched here");
  return;
}
} // namespace clang::tidy::modernize
