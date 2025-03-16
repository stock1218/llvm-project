#include "UseCheckedArithmeticCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"
#include "../utils/IncludeInserter.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void UseCheckedArithmeticCheck::registerPPCallbacks(
    const SourceManager &SM, Preprocessor *PP, Preprocessor *ModuleExpanderPP) {
  IncludeInserter.registerPreprocessor(PP);
}

StatementMatcher makeNonDeclMatcher() {
	  return binaryOperator(
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
		  ).bind("NonDeclOperation");
}

void UseCheckedArithmeticCheck::registerMatchers(MatchFinder *Finder) {

  Finder->addMatcher(traverse(TK_AsIs, makeNonDeclMatcher()), this);
  /*
  Finder->addMatcher(traverse(TK_AsIs,
	  varDecl(
		  hasDescendant(
			  binaryOperator(
				  hasOperatorName("+"),
				  hasLHS(ignoringImpCasts(declRefExpr().bind("opOne"))),
				  hasRHS(ignoringImpCasts(declRefExpr().bind("opTwo")))
			  )
		  )
	  ).bind("operation")),
	  this);
  */
}

void UseCheckedArithmeticCheck::fixNonDeclOperation(const MatchFinder::MatchResult &Result) {
	const auto *MatchedExpr = Result.Nodes.getNodeAs<Expr>("NonDeclOperation");

	const auto *dest = Result.Nodes.getNodeAs<DeclRefExpr>("dest");
	const auto *opOne = Result.Nodes.getNodeAs<DeclRefExpr>("opOne");
	const auto *opTwo = Result.Nodes.getNodeAs<DeclRefExpr>("opTwo");

	auto replacement = "if(!ckd_add(&" + 
		dest->getNameInfo().getAsString() + ", " +
		opOne->getNameInfo().getAsString() + ", " +
		opTwo->getNameInfo().getAsString() + ")) {\n" +
		"assert(false);\n" + "}\n";

	DiagnosticBuilder Diag = diag(dest->getBeginLoc(), "use checked arithmetic");
	Diag << FixItHint::CreateInsertion(dest->getLocation(), replacement);
	Diag << IncludeInserter.createIncludeInsertion(Result.Context->getSourceManager().getFileID(MatchedExpr->getBeginLoc()), "<stdckdint.h>");
	return;
}

void UseCheckedArithmeticCheck::check(const MatchFinder::MatchResult &Result) {
  // TODO this will be used to do the rewrite

  if(const auto *MatchedExpr = Result.Nodes.getNodeAs<Expr>("NonDeclOperation")) {
	  fixNonDeclOperation(Result);
  }

  return;
}
} // namespace clang::tidy::modernize
