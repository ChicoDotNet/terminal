#include "pch.h"
#include "fzf.h"
#include "terminal_app_ffi.h"

using namespace fzf::matcher;

Pattern fzf::matcher::ParsePattern(const std::wstring_view patternStr)
{
    static_assert(sizeof(wchar_t) == sizeof(uint16_t));

    terminal_app_ffi_fzf_pattern* rawPattern = nullptr;
    const auto status = terminal_app_ffi_fzf_pattern_create_utf16(
        reinterpret_cast<const uint16_t*>(patternStr.data()),
        patternStr.size(),
        &rawPattern);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_APP_FFI_OK || rawPattern == nullptr);

    return Pattern{
        std::shared_ptr<terminal_app_ffi_fzf_pattern>{ rawPattern, [](terminal_app_ffi_fzf_pattern* value) noexcept {
                                                         if (value)
                                                         {
                                                             terminal_app_ffi_fzf_pattern_destroy(value);
                                                         }
                                                     } }
    };
}

std::optional<MatchResult> fzf::matcher::Match(std::wstring_view text, const Pattern& pattern)
{
    static_assert(sizeof(wchar_t) == sizeof(uint16_t));
    THROW_HR_IF(E_UNEXPECTED, !pattern.RustPattern);

    int32_t score = 0;
    uint8_t matched = 0;
    size_t requiredRuns = 0;
    auto status = terminal_app_ffi_fzf_match_utf16(
        pattern.RustPattern.get(),
        reinterpret_cast<const uint16_t*>(text.data()),
        text.size(),
        &score,
        &matched,
        nullptr,
        0,
        &requiredRuns);

    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_APP_FFI_OK && status != TERMINAL_APP_FFI_BUFFER_TOO_SMALL);
    if (!matched)
    {
        return std::nullopt;
    }

    std::vector<terminal_app_ffi_fzf_run> ffiRuns(requiredRuns);
    if (requiredRuns != 0)
    {
        size_t writtenRuns = 0;
        status = terminal_app_ffi_fzf_match_utf16(
            pattern.RustPattern.get(),
            reinterpret_cast<const uint16_t*>(text.data()),
            text.size(),
            &score,
            &matched,
            ffiRuns.data(),
            ffiRuns.size(),
            &writtenRuns);
        THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_APP_FFI_OK || !matched || writtenRuns != ffiRuns.size());
    }

    MatchResult result;
    result.Score = score;
    result.Runs.reserve(ffiRuns.size());
    for (const auto& run : ffiRuns)
    {
        result.Runs.push_back({ run.start, run.end });
    }
    return result;
}

bool fzf::matcher::IsEmpty(const Pattern& pattern)
{
    THROW_HR_IF(E_UNEXPECTED, !pattern.RustPattern);

    uint8_t isEmpty = 0;
    const auto status = terminal_app_ffi_fzf_pattern_is_empty(pattern.RustPattern.get(), &isEmpty);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_APP_FFI_OK);
    return isEmpty != 0;
}
