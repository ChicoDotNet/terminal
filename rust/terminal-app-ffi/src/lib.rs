//! Narrow C ABI for portable TerminalApp behavior owned by Rust.
//!
//! The Windows/C++ layer keeps WinRT, XAML, and presentation ownership. This
//! crate exposes only deterministic operations that already have parity
//! evidence in `terminal-app`.

#![deny(unsafe_op_in_unsafe_fn)]

use std::{
    panic::{AssertUnwindSafe, catch_unwind},
    ptr, slice,
};

use terminal_app::{Pattern, match_text, parse_pattern};

/// Stable status values returned across the C ABI.
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FfiStatus {
    Ok = 0,
    InvalidArgument = 1,
    InvalidUtf16 = 2,
    BufferTooSmall = 3,
    Panic = 255,
}

/// UTF-16 highlight run returned to the native TerminalApp seam.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct FzfRun {
    pub start: usize,
    pub end: usize,
}

/// Opaque parse-once pattern handle. C++ never inspects this representation.
pub struct FzfPattern {
    pattern: Pattern,
    is_empty: bool,
}

pub const ABI_VERSION: u32 = 1;

fn ffi_guard(operation: impl FnOnce() -> FfiStatus) -> FfiStatus {
    catch_unwind(AssertUnwindSafe(operation)).unwrap_or(FfiStatus::Panic)
}

fn utf16_input<'a>(input: *const u16, input_len: usize) -> Result<&'a [u16], FfiStatus> {
    if input.is_null() {
        return if input_len == 0 {
            Ok(&[])
        } else {
            Err(FfiStatus::InvalidArgument)
        };
    }

    // SAFETY: The ABI requires `input` to reference `input_len` readable u16
    // code units for the duration of the call.
    Ok(unsafe { slice::from_raw_parts(input, input_len) })
}

#[unsafe(no_mangle)]
pub extern "C" fn terminal_app_ffi_abi_version() -> u32 {
    ABI_VERSION
}

/// Parses one command-palette search string and returns an opaque Rust handle.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_app_ffi_fzf_pattern_create_utf16(
    input: *const u16,
    input_len: usize,
    out_pattern: *mut *mut FzfPattern,
) -> FfiStatus {
    ffi_guard(|| {
        if out_pattern.is_null() {
            return FfiStatus::InvalidArgument;
        }

        // SAFETY: `out_pattern` was checked non-null and must point to one
        // writable pointer for the duration of the call.
        unsafe { ptr::write(out_pattern, ptr::null_mut()) };

        let input = match utf16_input(input, input_len) {
            Ok(input) => input,
            Err(status) => return status,
        };
        let input = match String::from_utf16(input) {
            Ok(input) => input,
            Err(_) => return FfiStatus::InvalidUtf16,
        };
        let pattern = parse_pattern(&input);
        let is_empty = match_text("", &pattern).is_some();
        let pattern = Box::new(FzfPattern { pattern, is_empty });

        // SAFETY: `out_pattern` is a valid writable pointer and Box::into_raw
        // transfers ownership to the caller until the matching destroy call.
        unsafe { ptr::write(out_pattern, Box::into_raw(pattern)) };
        FfiStatus::Ok
    })
}

/// Reports whether a parsed pattern has no effective FZF terms.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_app_ffi_fzf_pattern_is_empty(
    pattern: *const FzfPattern,
    out_is_empty: *mut u8,
) -> FfiStatus {
    ffi_guard(|| {
        if pattern.is_null() || out_is_empty.is_null() {
            return FfiStatus::InvalidArgument;
        }

        // SAFETY: Both pointers were checked non-null. The ABI requires the
        // pattern handle to remain alive and `out_is_empty` to be writable for
        // the duration of this call.
        let pattern = unsafe { &*pattern };
        unsafe { ptr::write(out_is_empty, u8::from(pattern.is_empty)) };
        FfiStatus::Ok
    })
}

/// Releases a pattern allocated by `terminal_app_ffi_fzf_pattern_create_utf16`.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_app_ffi_fzf_pattern_destroy(pattern: *mut FzfPattern) -> FfiStatus {
    ffi_guard(|| {
        if pattern.is_null() {
            return FfiStatus::Ok;
        }

        // SAFETY: The ABI requires this pointer to have been returned by the
        // create function and to be destroyed exactly once.
        unsafe { drop(Box::from_raw(pattern)) };
        FfiStatus::Ok
    })
}

/// Matches one UTF-16 item against a previously parsed pattern.
///
/// The caller owns the output buffer. A matched result writes exact Microsoft
/// FZF score and UTF-16 highlight offsets. Call with a null run buffer and zero
/// capacity to query the number of runs. No-match is represented as
/// `out_matched = 0`, score 0, and zero runs.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_app_ffi_fzf_match_utf16(
    pattern: *const FzfPattern,
    text: *const u16,
    text_len: usize,
    out_score: *mut i32,
    out_matched: *mut u8,
    out_runs: *mut FzfRun,
    runs_capacity: usize,
    out_runs_len: *mut usize,
) -> FfiStatus {
    ffi_guard(|| {
        if pattern.is_null()
            || out_score.is_null()
            || out_matched.is_null()
            || out_runs_len.is_null()
            || (out_runs.is_null() && runs_capacity != 0)
        {
            return FfiStatus::InvalidArgument;
        }

        let text = match utf16_input(text, text_len) {
            Ok(text) => text,
            Err(status) => return status,
        };
        let text = match String::from_utf16(text) {
            Ok(text) => text,
            Err(_) => return FfiStatus::InvalidUtf16,
        };

        // SAFETY: `pattern` was checked non-null. The ABI requires the handle
        // to remain alive for the duration of this call.
        let pattern = unsafe { &*pattern };
        let result = match_text(&text, &pattern.pattern);

        let (matched, score, runs) = match result {
            Some(result) => (1_u8, result.score, result.runs),
            None => (0_u8, 0, Vec::new()),
        };
        let required = runs.len();

        // SAFETY: All scalar output pointers were checked non-null above.
        unsafe {
            ptr::write(out_score, score);
            ptr::write(out_matched, matched);
            ptr::write(out_runs_len, required);
        }

        if runs_capacity < required {
            return FfiStatus::BufferTooSmall;
        }
        if required != 0 {
            if out_runs.is_null() {
                return FfiStatus::InvalidArgument;
            }
            for (index, run) in runs.into_iter().enumerate() {
                // SAFETY: `runs_capacity >= required`; every index written is
                // therefore within the caller-provided writable array.
                unsafe {
                    ptr::write(
                        out_runs.add(index),
                        FzfRun {
                            start: run.start,
                            end: run.end,
                        },
                    )
                };
            }
        }

        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        FfiStatus, FzfPattern, FzfRun, terminal_app_ffi_abi_version,
        terminal_app_ffi_fzf_match_utf16, terminal_app_ffi_fzf_pattern_create_utf16,
        terminal_app_ffi_fzf_pattern_destroy, terminal_app_ffi_fzf_pattern_is_empty,
    };

    fn utf16(value: &str) -> Vec<u16> {
        value.encode_utf16().collect()
    }

    fn create_pattern(value: &str) -> *mut FzfPattern {
        let value = utf16(value);
        let mut pattern = std::ptr::null_mut();
        assert_eq!(
            terminal_app_ffi_fzf_pattern_create_utf16(
                value.as_ptr(),
                value.len(),
                &mut pattern,
            ),
            FfiStatus::Ok
        );
        assert!(!pattern.is_null());
        pattern
    }

    fn pattern_is_empty(pattern: *const FzfPattern) -> bool {
        let mut is_empty = u8::MAX;
        assert_eq!(
            terminal_app_ffi_fzf_pattern_is_empty(pattern, &mut is_empty),
            FfiStatus::Ok
        );
        is_empty != 0
    }

    fn match_runs(pattern: *const FzfPattern, text: &str) -> (u8, i32, Vec<FzfRun>) {
        let text = utf16(text);
        let mut score = i32::MIN;
        let mut matched = u8::MAX;
        let mut required = usize::MAX;
        let first = terminal_app_ffi_fzf_match_utf16(
            pattern,
            text.as_ptr(),
            text.len(),
            &mut score,
            &mut matched,
            std::ptr::null_mut(),
            0,
            &mut required,
        );

        if required == 0 {
            assert_eq!(first, FfiStatus::Ok);
            return (matched, score, Vec::new());
        }
        assert_eq!(first, FfiStatus::BufferTooSmall);

        let mut runs = vec![FzfRun { start: 0, end: 0 }; required];
        assert_eq!(
            terminal_app_ffi_fzf_match_utf16(
                pattern,
                text.as_ptr(),
                text.len(),
                &mut score,
                &mut matched,
                runs.as_mut_ptr(),
                runs.len(),
                &mut required,
            ),
            FfiStatus::Ok
        );
        (matched, score, runs)
    }

    #[test]
    fn fzf_abi_version_is_stable() {
        assert_eq!(terminal_app_ffi_abi_version(), 1);
    }

    #[test]
    fn fzf_ffi_preserves_empty_pattern_as_zero_weight_match() {
        let pattern = create_pattern("");
        let (matched, score, runs) = match_runs(pattern, "anything");

        assert!(pattern_is_empty(pattern));
        assert_eq!((matched, score), (1, 0));
        assert!(runs.is_empty());
        assert_eq!(terminal_app_ffi_fzf_pattern_destroy(pattern), FfiStatus::Ok);
    }

    #[test]
    fn fzf_ffi_treats_separator_only_pattern_as_empty() {
        let pattern = create_pattern("   ");

        assert!(pattern_is_empty(pattern));
        assert_eq!(terminal_app_ffi_fzf_pattern_destroy(pattern), FfiStatus::Ok);
    }

    #[test]
    fn fzf_ffi_reports_nonempty_pattern_without_matching_text() {
        let pattern = create_pattern("foo");

        assert!(!pattern_is_empty(pattern));
        assert_eq!(terminal_app_ffi_fzf_pattern_destroy(pattern), FfiStatus::Ok);
    }

    #[test]
    fn fzf_ffi_preserves_parse_once_multi_term_score_and_runs() {
        let pattern = create_pattern("sp anta");
        let (matched, score, runs) = match_runs(
            pattern,
            "Split Pane, split: horizontal, profile: SSH: Antares",
        );

        assert_eq!(matched, 1);
        assert_eq!(score, 160);
        assert_eq!(
            runs,
            vec![FzfRun { start: 0, end: 1 }, FzfRun { start: 45, end: 48 }]
        );
        assert_eq!(terminal_app_ffi_fzf_pattern_destroy(pattern), FfiStatus::Ok);
    }

    #[test]
    fn fzf_ffi_preserves_utf16_offsets_for_surrogate_pairs() {
        let pattern = create_pattern("N😀ewer");
        let (matched, score, runs) = match_runs(pattern, "N😀ewer tab");

        assert_eq!(matched, 1);
        assert_eq!(score, 152);
        assert_eq!(runs, vec![FzfRun { start: 0, end: 6 }]);
        assert_eq!(terminal_app_ffi_fzf_pattern_destroy(pattern), FfiStatus::Ok);
    }

    #[test]
    fn fzf_ffi_preserves_no_match_as_zero_weight() {
        let pattern = create_pattern("fbb");
        let (matched, score, runs) = match_runs(pattern, "foo bar");

        assert_eq!((matched, score), (0, 0));
        assert!(runs.is_empty());
        assert_eq!(terminal_app_ffi_fzf_pattern_destroy(pattern), FfiStatus::Ok);
    }

    #[test]
    fn fzf_ffi_rejects_invalid_utf16_without_leaking_a_handle() {
        let invalid = [0xd800_u16];
        let mut pattern = 1_usize as *mut FzfPattern;
        assert_eq!(
            terminal_app_ffi_fzf_pattern_create_utf16(
                invalid.as_ptr(),
                invalid.len(),
                &mut pattern,
            ),
            FfiStatus::InvalidUtf16
        );
        assert!(pattern.is_null());
    }

    #[test]
    fn fzf_ffi_validates_required_pointers() {
        let value = utf16("foo");
        assert_eq!(
            terminal_app_ffi_fzf_pattern_create_utf16(
                value.as_ptr(),
                value.len(),
                std::ptr::null_mut(),
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_app_ffi_fzf_pattern_is_empty(std::ptr::null(), std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
    }
}
