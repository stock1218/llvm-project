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
  // FIXME: Add matchers.
  Finder->addMatcher(binaryOperation(hasAnyOperatorName("+", "-", "*")).bind("operation"), this);
}

void UseCheckedArithmeticDebugCheck::check(const MatchFinder::MatchResult &Result) {
  // FIXME: Add callback implementation.
  const auto *MatchedOperation = Result.Nodes.getNodeAs<BinaryOperator>("operation");
  diag(MatchedOperation->getBeginLoc(), "Potential checked operation");
}

} // namespace clang::tidy::modernize
