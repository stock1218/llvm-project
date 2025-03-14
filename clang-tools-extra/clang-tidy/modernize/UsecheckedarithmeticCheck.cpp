#include "UseCheckedArithmeticCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void UseCheckedArithmeticCheck::registerMatchers(MatchFinder *Finder) {
	
  Finder->addMatcher(traverse(TK_AsIs,
	  binaryOperator(
		  hasOperatorName("="),
		  hasLHS(
			  declRefExpr().bind("dest")
		  ),
		  hasRHS(
			  binaryOperator(
				  hasOperatorName("+"),
				  hasLHS(ignoringImpCasts(declRefExpr().bind("opOne"))),
				  hasRHS(ignoringImpCasts(declRefExpr().bind("opTwo")))
			  )
		  )
	  )),
	  this);

}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {
  // TODO this will be used to do the rewrite

  const auto *dest = Result.Nodes.getNodeAs<Expr>("dest");
  if(dest) {
	  SourceLocation destBeginLoc = dest->getBeginLoc();
	  SourceLocation destEndLoc = dest->getEndLoc();
	  auto Diag = diag(destBeginLoc, "dest");
  } else {
	  llvm::outs() << "Error: dest";
  }

  const auto *opOne = Result.Nodes.getNodeAs<Expr>("opOne");
  if(opOne) {
	  SourceLocation opOneBeginLoc = opOne->getBeginLoc();
	  SourceLocation opOneEndLoc = opOne->getEndLoc();
	  auto opOneDiag = diag(opOneBeginLoc, "op one");
  } else {
	  llvm::outs() << "Error: op one";
  }

  const auto *opTwo = Result.Nodes.getNodeAs<Expr>("opTwo");
  if(opTwo) {
	  SourceLocation opTwoBeginLoc = opTwo->getBeginLoc();
	  SourceLocation opTwoEndLoc = opTwo->getEndLoc();
	  auto opTwoDiag = diag(opTwoBeginLoc, "op two");
  } else {
	  llvm::outs() << "Error: op one";
  }

  llvm::StringRef replacement = "test";

  DiagnosticBuilder Diag = diag(dest->getBeginLoc(), "use checked arith here: %0");
  Diag << FixItHint::CreateReplacement(dest->getSourceRange(), replacement);

  return;
}
} // namespace clang::tidy::modernize
