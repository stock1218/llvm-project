#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USE_CHECKED_ARITHMETIC_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USE_CHECKED_ARITHMETIC_H

#include "../ClangTidyCheck.h"
#include "../utils/IncludeInserter.h"

namespace clang::tidy::modernize {

/// This check will warn on arithemtic expressions that can overflow and suggest
/// using the C23 ckd_* macro
///
class UseCheckedArithmeticCheck : public ClangTidyCheck {
public:
  UseCheckedArithmeticCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context),
        HandleImport(Options.get("handleImport", "__unset")),
        HandleCode(Options.get("handleCode", "__unset")),
        IncludeInserter(Options.getLocalOrGlobal("IncludeStyle",
                                                 utils::IncludeSorter::IS_LLVM),
                        areDiagsSelfContained()) {}
  /* TODO maybe specify later
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    llvm::outs() << "Called lang supported";
    return LangOpts.C23;
  }
  */

  void storeOptions(ClangTidyOptions::OptionMap &Opts) override {
      Options.store(Opts, "handleImport", HandleImport);
      Options.store(Opts, "handleCode", HandleCode);
  }

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  utils::IncludeInserter IncludeInserter;
  std::string HandleImport;
  std::string HandleCode;
  void fixNonAssignmentOp(const ast_matchers::MatchFinder::MatchResult &Result);
  void fixAssignmentOp(const ast_matchers::MatchFinder::MatchResult &Result);
  void fixUnaryOp(const ast_matchers::MatchFinder::MatchResult &Result);
};

} // namespace clang::tidy::modernize

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MODERNIZE_USE_CHECKED_ARITHMETIC_H
