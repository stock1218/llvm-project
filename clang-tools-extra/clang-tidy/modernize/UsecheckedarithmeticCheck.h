#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USE_CHECKED_ARITHMETIC_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USE_CHECKED_ARITHMETIC_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::modernize {

/// This check will warn on arithemtic expressions that can overflow and suggest using the C23 ckd_* macro
///
class UseCheckedArithmeticCheck : public ClangTidyCheck {
public:
  UseCheckedArithmeticCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.CPlusPlus17;
  }
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace clang::tidy::modernize

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USE_CHECKED_ARITHMETIC_H
