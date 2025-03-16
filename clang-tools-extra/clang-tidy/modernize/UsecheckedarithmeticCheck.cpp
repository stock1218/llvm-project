#include "UseCheckedArithmeticCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"
#include "../utils/IncludeInserter.h"

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
	  ).bind("operation")),
	  this);

}

void UseCheckedArithmeticCheck::registerPPCallbacks(
    const SourceManager &SM, Preprocessor *PP, Preprocessor *ModuleExpanderPP) {
  IncludeInserter.registerPreprocessor(PP);
}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {
  // TODO this will be used to do the rewrite

  const auto *FullExpr = Result.Nodes.getNodeAs<Expr>("operation");

  const auto *dest = Result.Nodes.getNodeAs<DeclRefExpr>("dest");
  if(dest) {
	  SourceLocation destBeginLoc = dest->getBeginLoc();
	  SourceLocation destEndLoc = dest->getEndLoc();
	  auto Diag = diag(destBeginLoc, "dest");
  } else {
	  llvm::outs() << "Error: dest";
  }

  const auto *opOne = Result.Nodes.getNodeAs<DeclRefExpr>("opOne");
  if(opOne) {
	  SourceLocation opOneBeginLoc = opOne->getBeginLoc();
	  SourceLocation opOneEndLoc = opOne->getEndLoc();
	  auto opOneDiag = diag(opOneBeginLoc, "op one");
  } else {
	  llvm::outs() << "Error: op one";
  }

  const auto *opTwo = Result.Nodes.getNodeAs<DeclRefExpr>("opTwo");
  if(opTwo) {
	  SourceLocation opTwoBeginLoc = opTwo->getBeginLoc();
	  SourceLocation opTwoEndLoc = opTwo->getEndLoc();
	  auto opTwoDiag = diag(opTwoBeginLoc, "op two");
  } else {
	  llvm::outs() << "Error: op one";
  }

  /*
  llvm::outs() << dest->getNameInfo();
  */

  auto replacement = "if(!ckd_add(&" + 
	  dest->getNameInfo().getAsString() + ", " +
	  opOne->getNameInfo().getAsString() + ", " +
	  opTwo->getNameInfo().getAsString() + ")) {\n" +
	  "assert(false);\n" + "}\n";

  /*
  llvm::outs() << "START";
  llvm::outs() << replacement;
  llvm::outs() << "DONE";
  */

  DiagnosticBuilder Diag = diag(dest->getBeginLoc(), "use checked arithmetic");
  Diag << FixItHint::CreateInsertion(dest->getLocation(), replacement);
  llvm::outs() << "Adding inc";
  Diag << IncludeInserter.createIncludeInsertion(Result.Context->getSourceManager().getFileID(FullExpr->getBeginLoc()), "<stdckdint.h>");

  return;
}
} // namespace clang::tidy::modernize
