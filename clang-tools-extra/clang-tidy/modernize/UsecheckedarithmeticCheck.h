//===--- UseCheckedArithmeticCheck.h - clang-tidy ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USECHECKEDARITHMETICCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USECHECKEDARITHMETICCHECK_H

#include "../ClangTidyCheck.h"
#include "../utils/TransformerClangTidyCheck.h"
#include "clang/Tooling/Transformer/Stencil.h"

using namespace clang::tidy::utils;
using namespace clang::transformer;

namespace clang::tidy::modernize {

class UseCheckedArithmeticCheck : public TransformerClangTidyCheck {
public:
  UseCheckedArithmeticCheck(StringRef Name, ClangTidyContext *Context)
      : TransformerClangTidyCheck(Name, Context) {
    setRule(createCheckedStatementRule());
  }
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return C23;
  }

private:
  static transformer::RewriteRuleWith<std::string> createCheckedStatementRule();
};

} // namespace clang::tidy::modernize

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USECHECKEDARITHMETICCHECK_H
