//===--- UseCheckedArithmeticTyppedDebugCheck.cpp - clang-tidy ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UseCheckedArithmeticTypedDebugCheck.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void UseCheckedArithmeticTypedDebugCheck::registerMatchers(MatchFinder *Finder) {
    Finder->addMatcher(
        binaryOperator(hasAnyOperatorName("+", "-", "*"),
                          hasParent(expr(hasType(isInteger()), unless(hasType(isConstQualified())))),
                          hasLHS(ignoringImpCasts(hasType(isInteger()))),
                          hasRHS(ignoringImpCasts(hasType(isInteger()))))
           .bind("Non-AssignmentOp"),
        this);


    Finder->addMatcher(
        binaryOperator(hasAnyOperatorName("+", "-", "*"),
                          hasLHS(ignoringImpCasts(hasType(isInteger()))),
                          hasRHS(ignoringImpCasts(hasType(isInteger()))))
           .bind("Non-AssignmentOp"),
        this);

    Finder->addMatcher(
        unaryOperator(hasAnyOperatorName("++", "--"),
                       hasUnaryOperand(ignoringImpCasts(hasType(isInteger()))))
      .bind("UnaryOp"), this);

    Finder->addMatcher(binaryOperation(
             hasAnyOperatorName("+=", "-=", "*="), isAssignmentOperator(),
             hasLHS(ignoringImpCasts(hasType(isInteger()))),
             hasRHS(ignoringImpCasts(hasType(isInteger()))))
      .bind("AssignmentOp"), this);

}

void UseCheckedArithmeticTypedDebugCheck::check(
    const MatchFinder::MatchResult &Result) {
  // FIXME: Add callback implementation.
  if(Result.Nodes.getNodeAs<Expr>("Non-AssignmentOp")) {
	  const auto *MatchedOperation = Result.Nodes.getNodeAs<Expr>("Non-AssignmentOp");
	  diag(MatchedOperation->getBeginLoc(), "Potential checked operation (non-assignment operation)");
  } else if(Result.Nodes.getNodeAs<Expr>("AssignmentOp")) {
	  const auto *MatchedOperation = Result.Nodes.getNodeAs<Expr>("AssignmentOp");
	  diag(MatchedOperation->getBeginLoc(), "Potential checked operation (assignment operation)");
  } else if(Result.Nodes.getNodeAs<Expr>("UnaryOp")) {
	  const auto *MatchedOperation = Result.Nodes.getNodeAs<Expr>("UnaryOp");
	  diag(MatchedOperation->getBeginLoc(), "Potential checked operation (unary operation)");
  }
}

} // namespace clang::tidy::modernize
