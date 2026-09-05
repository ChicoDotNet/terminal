use std::ptr;

use terminal_parser::input_engine::{InputAction, InputDispatch, InputStateMachineEngine, KeyEvent};
use terminal_parser::state_machine::Parameters;

use super::{FfiStatus, ffi_guard};

const PARAMETER_MASK: u32 = 0x3f;

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct TerminalParserFfiKeyEvent {
    pub key_down: u32,
    pub repeat_count: u16,
    pub virtual_key: u16,
    pub scan_code: u16,
    pub unicode_char: u16,
    pub control_key_state: u32,
}

struct NoopDispatch;
impl InputDispatch for NoopDispatch {
    fn dispatch(&mut self, _action: InputAction) {}
}

fn parameter(mask: u32, index: u32, value: i32) -> Option<i32> {
    (mask & (1u32 << index) != 0).then_some(value)
}

fn fields(key: KeyEvent) -> TerminalParserFfiKeyEvent {
    TerminalParserFfiKeyEvent {
        key_down: if key.key_down { 1 } else { 0 },
        repeat_count: key.repeat_count,
        virtual_key: key.virtual_key,
        scan_code: key.scan_code,
        unicode_char: key.unicode_char,
        control_key_state: key.control_key_state,
    }
}

/// Converts serialized Win32-input VT parameters to scalar key-event fields.
/// `present_mask` bits 0..5 mark parameters present in the source sequence.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_win32_key_fields(
    present_mask: u32,
    virtual_key: i32,
    scan_code: i32,
    unicode_char: i32,
    key_down: i32,
    control_key_state: i32,
    repeat_count: i32,
    out_key: *mut TerminalParserFfiKeyEvent,
) -> FfiStatus {
    ffi_guard(|| {
        if out_key.is_null() || present_mask & !PARAMETER_MASK != 0 {
            return FfiStatus::InvalidArgument;
        }
        let parameters = Parameters::from_values(vec![
            parameter(present_mask, 0, virtual_key),
            parameter(present_mask, 1, scan_code),
            parameter(present_mask, 2, unicode_char),
            parameter(present_mask, 3, key_down),
            parameter(present_mask, 4, control_key_state),
            parameter(present_mask, 5, repeat_count),
        ]);
        let key = InputStateMachineEngine::<NoopDispatch>::generate_win32_key(&parameters);
        unsafe { ptr::write(out_key, fields(key)) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn replays_serialized_key_fields_and_defaults() {
        let mut key = TerminalParserFfiKeyEvent::default();
        assert_eq!(
            terminal_parser_ffi_input_win32_key_fields(0x3f, 1, 2, 65, 1, 5, 6, &mut key),
            FfiStatus::Ok
        );
        assert_eq!(
            key,
            TerminalParserFfiKeyEvent {
                key_down: 1,
                repeat_count: 6,
                virtual_key: 1,
                scan_code: 2,
                unicode_char: 65,
                control_key_state: 5,
            }
        );

        assert_eq!(
            terminal_parser_ffi_input_win32_key_fields(0, 9, 9, 9, 9, 9, 9, &mut key),
            FfiStatus::Ok
        );
        assert_eq!(
            key,
            TerminalParserFfiKeyEvent {
                repeat_count: 1,
                ..TerminalParserFfiKeyEvent::default()
            }
        );
    }

    #[test]
    fn rejects_invalid_abi_arguments() {
        let mut key = TerminalParserFfiKeyEvent::default();
        assert_eq!(
            terminal_parser_ffi_input_win32_key_fields(0x40, 0, 0, 0, 0, 0, 0, &mut key),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_input_win32_key_fields(
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                std::ptr::null_mut(),
            ),
            FfiStatus::InvalidArgument
        );
    }
}
