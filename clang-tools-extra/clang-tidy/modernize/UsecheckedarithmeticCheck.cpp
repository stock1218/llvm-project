//===--- UseCheckedArithmeticCheck.cpp - clang-tidy -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UseCheckedArithmeticCheck.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Transformer/Stencil.h"

using namespace clang::ast_matchers;
using namespace clang::transformer;

namespace clang::tidy::modernize {

auto createAddCheckedStatementRule() {
  auto rule = makeRule(traverse(clang::TK_IgnoreUnlessSpelledInSource,
                                binaryOperation(hasOperatorName("+"))),
                       changeTo(cat("bruh")), cat("MODIFIED"));
  return rule;
}

transformer::RewriteRuleWith<std::string>
UseCheckedArithmeticCheck::createCheckedStatementRule() {
  return applyFirst({createAddCheckedStatementRule()});
}

} // namespace clang::tidy::modernize
