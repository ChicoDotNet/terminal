#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

struct terminal_app_ffi_fzf_pattern;

namespace fzf::matcher
{
    struct TextRun
    {
        size_t Start;
        size_t End;
    };

    struct MatchResult
    {
        int32_t Score = 0;
        std::vector<TextRun> Runs;
    };

    struct Pattern
    {
        std::shared_ptr<terminal_app_ffi_fzf_pattern> RustPattern;
    };

    Pattern ParsePattern(std::wstring_view patternStr);
    std::optional<MatchResult> Match(std::wstring_view text, const Pattern& pattern);
    bool IsEmpty(const Pattern& pattern);
}
