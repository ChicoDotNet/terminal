//! Narrow compatibility boundary between the existing C++ code and Rust.
//!
//! R08 turns the previously empty ABI placeholder into the common FFI
//! foundation used by product-integration facades. Product semantics stay in
//! safe Rust crates; raw-pointer handling belongs only in explicit FFI modules.

#![deny(unsafe_op_in_unsafe_fn)]

mod input_control;
mod input_keymap;
mod input_mouse;
mod input_win32;
mod output_csi_cursor;
mod output_csi_cursor_restore;
mod output_csi_cursor_style;
mod output_csi_device_attributes;
mod output_csi_device_status_report;
mod output_csi_displayed_extent;
mod output_csi_edit;
mod output_csi_erase;
mod output_csi_erase_characters;
mod output_csi_line_edit;
mod output_csi_margins;
mod output_csi_mode;
mod output_csi_page;
mod output_csi_page_position;
mod output_csi_request_mode;
mod output_csi_scroll;
mod output_csi_soft_reset;
mod output_csi_tab;
mod output_csi_tab_control;
mod output_csi_terminal_parameters;
mod output_csi_window_manipulation;
mod output_esc;
mod output_execute;
mod output_vt52;

use std::{
    panic::{AssertUnwindSafe, catch_unwind},
    ptr, slice,
};

use terminal_parser::base64::{DecodeError, decode_utf16};

/// Stable status values returned across the C ABI.
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FfiStatus {
    /// Operation completed successfully.
    Ok = 0,
    /// The caller supplied an invalid ABI argument.
    InvalidArgument = 1,
    /// The parser rejected the supplied Base64 input.
    InvalidBase64 = 2,
    /// Base64 decoded successfully but the payload was not UTF-8.
    InvalidUtf8 = 3,
    /// The caller-provided output buffer is too small. The required UTF-16
    /// length is still written to `out_len`.
    BufferTooSmall = 4,
    /// Rust panicked while servicing the call.
    Panic = 255,
}

impl From<DecodeError> for FfiStatus {
    fn from(error: DecodeError) -> Self {
        match error {
            DecodeError::InvalidBase64 => Self::InvalidBase64,
            DecodeError::InvalidUtf8 => Self::InvalidUtf8,
        }
    }
}

/// Current ABI contract version.
///
/// Increment this only for an intentional breaking change to the exported C
/// surface. Additive functions do not require a bump.
pub const ABI_VERSION: u32 = 1;

/// Returns the ABI contract version without allocating or crossing ownership
/// boundaries.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_abi_version() -> u32 {
    ABI_VERSION
}

/// Executes an FFI operation while preventing Rust panics from unwinding into
/// C++.
fn ffi_guard(operation: impl FnOnce() -> FfiStatus) -> FfiStatus {
    catch_unwind(AssertUnwindSafe(operation)).unwrap_or(FfiStatus::Panic)
}

/// Exercises the status-returning ABI path without pointer or ownership
/// semantics. This gives C/C++ consumers a stable handshake that also proves
/// the panic-containment path is part of the production boundary.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_status_probe() -> FfiStatus {
    ffi_guard(|| FfiStatus::Ok)
}

/// Decodes Windows Terminal-compatible Base64 from UTF-16 into UTF-16.
///
/// The ABI uses caller-owned buffers so neither language frees memory allocated
/// by the other. Call once with `output = null` and `output_capacity = 0` to
/// query the required UTF-16 code-unit count, then call again with a buffer of
/// at least that size. `out_len` is populated on both successful sizing calls
/// and `BufferTooSmall` results.
///
/// A null input pointer is valid only when `input_len == 0`. A null output
/// pointer is valid only when `output_capacity == 0`. `out_len` must never be
/// null.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_base64_decode_utf16(
    input: *const u16,
    input_len: usize,
    output: *mut u16,
    output_capacity: usize,
    out_len: *mut usize,
) -> FfiStatus {
    ffi_guard(|| {
        if out_len.is_null()
            || (input.is_null() && input_len != 0)
            || (output.is_null() && output_capacity != 0)
        {
            return FfiStatus::InvalidArgument;
        }

        let input = if input_len == 0 {
            &[]
        } else {
            // SAFETY: The C ABI contract requires `input` to reference
            // `input_len` readable UTF-16 code units for the duration of this
            // call. Null with a non-zero length was rejected above.
            unsafe { slice::from_raw_parts(input, input_len) }
        };

        let decoded = match decode_utf16(input) {
            Ok(decoded) => decoded,
            Err(error) => return error.into(),
        };
        let decoded_utf16 = decoded.encode_utf16().collect::<Vec<_>>();
        let required = decoded_utf16.len();

        // SAFETY: `out_len` was checked non-null above and the ABI requires it
        // to reference one writable `usize` for the duration of the call.
        unsafe { ptr::write(out_len, required) };

        if output_capacity < required {
            return FfiStatus::BufferTooSmall;
        }

        if required != 0 {
            if output.is_null() {
                return FfiStatus::InvalidArgument;
            }
            // SAFETY: `output_capacity >= required`; the ABI contract requires
            // `output` to reference that many writable UTF-16 code units and
            // the source vector cannot overlap caller-owned output memory.
            unsafe { ptr::copy_nonoverlapping(decoded_utf16.as_ptr(), output, required) };
        }

        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        ABI_VERSION, FfiStatus, ffi_guard, terminal_parser_ffi_abi_version,
        terminal_parser_ffi_base64_decode_utf16, terminal_parser_ffi_status_probe,
    };
    use terminal_parser::base64::DecodeError;

    #[test]
    fn abi_version_and_status_probe_are_stable_and_exported() {
        assert_eq!(terminal_parser_ffi_abi_version(), ABI_VERSION);
        assert_eq!(terminal_parser_ffi_status_probe(), FfiStatus::Ok);
        assert_eq!(ABI_VERSION, 1);
    }

    #[test]
    fn decode_errors_have_stable_status_mapping() {
        assert_eq!(
            FfiStatus::from(DecodeError::InvalidBase64),
            FfiStatus::InvalidBase64
        );
        assert_eq!(
            FfiStatus::from(DecodeError::InvalidUtf8),
            FfiStatus::InvalidUtf8
        );
    }

    #[test]
    fn ffi_guard_returns_status_without_translation() {
        assert_eq!(ffi_guard(|| FfiStatus::Ok), FfiStatus::Ok);
        assert_eq!(
            ffi_guard(|| FfiStatus::InvalidArgument),
            FfiStatus::InvalidArgument
        );
    }

    #[test]
    fn ffi_guard_contains_panics() {
        assert_eq!(
            ffi_guard(|| panic!("panic must not cross the C ABI")),
            FfiStatus::Panic
        );
    }

    #[test]
    fn base64_ffi_uses_caller_owned_utf16_buffers() {
        let encoded = "44Gr44G744KT44GU5rGJ6K+t7ZWc6rWt"
            .encode_utf16()
            .collect::<Vec<_>>();
        let expected = "にほんご汉语한국";
        let mut required = 0usize;

        assert_eq!(
            terminal_parser_ffi_base64_decode_utf16(
                encoded.as_ptr(),
                encoded.len(),
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::BufferTooSmall
        );
        assert_eq!(required, expected.encode_utf16().count());

        let mut output = vec![0u16; required];
        assert_eq!(
            terminal_parser_ffi_base64_decode_utf16(
                encoded.as_ptr(),
                encoded.len(),
                output.as_mut_ptr(),
                output.len(),
                &mut required,
            ),
            FfiStatus::Ok
        );
        assert_eq!(String::from_utf16(&output).unwrap(), expected);
    }

    #[test]
    fn base64_ffi_preserves_error_classification_and_validates_pointers() {
        let invalid = "A".encode_utf16().collect::<Vec<_>>();
        let mut required = usize::MAX;

        assert_eq!(
            terminal_parser_ffi_base64_decode_utf16(
                invalid.as_ptr(),
                invalid.len(),
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidBase64
        );
        assert_eq!(
            terminal_parser_ffi_base64_decode_utf16(
                std::ptr::null(),
                1,
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_base64_decode_utf16(
                std::ptr::null(),
                0,
                std::ptr::null_mut(),
                0,
                std::ptr::null_mut(),
            ),
            FfiStatus::InvalidArgument
        );
    }

    #[test]
    fn base64_ffi_handles_empty_output_without_requiring_a_buffer() {
        let mut required = usize::MAX;
        assert_eq!(
            terminal_parser_ffi_base64_decode_utf16(
                std::ptr::null(),
                0,
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::Ok
        );
        assert_eq!(required, 0);
    }
}
