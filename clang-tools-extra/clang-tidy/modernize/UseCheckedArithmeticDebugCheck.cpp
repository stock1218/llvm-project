//===--- UsecheckedarithmeticdebugCheck.cpp - clang-tidy ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UseCheckedArithmeticDebugCheck.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

void UseCheckedArithmeticDebugCheck::registerMatchers(MatchFinder *Finder) {
  // Matcher for binops
  Finder->addMatcher(
      binaryOperation(
          hasAnyOperatorName("+", "-", "*"))
          //hasLHS(ignoringImpCasts(hasType(isInteger()))),
          //hasRHS(ignoringImpCasts(hasType(isInteger()))))
          .bind("operation"),
      this);

  // Matcher for unary ops
  Finder->addMatcher(
      unaryOperator(
          hasAnyOperatorName("++", "--"))
          .bind("operation"),
      this);

  // Matcher for compound assignment
  Finder->addMatcher(
      binaryOperation(
          isAssignmentOperator(),
          hasAnyOperatorName("+=", "-=", "*="))
          .bind("operation"),
      this);
}

void UseCheckedArithmeticDebugCheck::check(
    const MatchFinder::MatchResult &Result) {
  // FIXME: Add callback implementation.
  const auto *MatchedOperation =
      Result.Nodes.getNodeAs<Expr>("operation");
  diag(MatchedOperation->getBeginLoc(), "Potential checked operation");
}

} // namespace clang::tidy::modernize
