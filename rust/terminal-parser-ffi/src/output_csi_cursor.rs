use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiCursorKind {
    None = 0,
    CursorUp = 1,
    CursorDown = 2,
    CursorForward = 3,
    CursorBackward = 4,
    CursorNextLine = 5,
    CursorPreviousLine = 6,
    CursorHorizontalAbsolute = 7,
    VerticalLineAbsolute = 8,
    HorizontalRelative = 9,
    VerticalRelative = 10,
    CursorPosition = 11,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiCursorPlan {
    pub kind: u32,
    pub argument1: i32,
    pub argument2: i32,
    pub reserved: u32,
}

impl Default for OutputCsiCursorPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiCursorKind::None as u32,
            argument1: 0,
            argument2: 0,
            reserved: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiCursorPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::CursorUp(count) => plan(OutputCsiCursorKind::CursorUp, count, 0),
            OutputAction::CursorDown(count) => plan(OutputCsiCursorKind::CursorDown, count, 0),
            OutputAction::CursorForward(count) => {
                plan(OutputCsiCursorKind::CursorForward, count, 0)
            }
            OutputAction::CursorBackward(count) => {
                plan(OutputCsiCursorKind::CursorBackward, count, 0)
            }
            OutputAction::CursorNextLine(count) => {
                plan(OutputCsiCursorKind::CursorNextLine, count, 0)
            }
            OutputAction::CursorPreviousLine(count) => {
                plan(OutputCsiCursorKind::CursorPreviousLine, count, 0)
            }
            OutputAction::CursorHorizontalPositionAbsolute(column) => {
                plan(OutputCsiCursorKind::CursorHorizontalAbsolute, column, 0)
            }
            OutputAction::VerticalLinePositionAbsolute(line) => {
                plan(OutputCsiCursorKind::VerticalLineAbsolute, line, 0)
            }
            OutputAction::HorizontalPositionRelative(count) => {
                plan(OutputCsiCursorKind::HorizontalRelative, count, 0)
            }
            OutputAction::VerticalPositionRelative(count) => {
                plan(OutputCsiCursorKind::VerticalRelative, count, 0)
            }
            OutputAction::CursorPosition { line, column } => {
                plan(OutputCsiCursorKind::CursorPosition, line, column)
            }
            _ => OutputCsiCursorPlan::default(),
        };
    }
}

const fn plan(kind: OutputCsiCursorKind, argument1: i32, argument2: i32) -> OutputCsiCursorPlan {
    OutputCsiCursorPlan {
        kind: kind as u32,
        argument1,
        argument2,
        reserved: 0,
    }
}

fn vt_id_from_value(identifier: u64) -> Option<VtId> {
    if identifier & 0xff00_0000_0000_0000 != 0 {
        return None;
    }

    let bytes = identifier.to_le_bytes();
    let length = bytes[..7]
        .iter()
        .position(|byte| *byte == 0)
        .unwrap_or(7);
    if bytes[length..7].iter().any(|byte| *byte != 0) || !bytes[..length].is_ascii() {
        return None;
    }
    let text = std::str::from_utf8(&bytes[..length]).ok()?;
    Some(VtId::from_ascii(text))
}

/// Replays one compact CSI cursor/navigation sequence through the existing
/// Rust output engine. Non-cursor CSI actions return `None`, allowing the C++
/// product seam to retain unrelated CSI dispatch until their own slices move.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_cursor_plan(
    identifier: u64,
    parameter0: i32,
    parameter1: i32,
    out_plan: *mut OutputCsiCursorPlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let parameters = Parameters::from_values(vec![Some(parameter0), Some(parameter1)]);
        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_csi_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the ABI requires
        // it to reference one writable `OutputCsiCursorPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiCursorKind, OutputCsiCursorPlan, terminal_parser_ffi_output_csi_cursor_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(
        id: &str,
        parameter0: i32,
        parameter1: i32,
        kind: OutputCsiCursorKind,
        argument1: i32,
        argument2: i32,
    ) {
        let mut result = OutputCsiCursorPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_cursor_plan(
                VtId::from_ascii(id).value(),
                parameter0,
                parameter1,
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
        assert_eq!(result.argument1, argument1, "id={id:?}");
        assert_eq!(result.argument2, argument2, "id={id:?}");
    }

    #[test]
    fn csi_cursor_ffi_replays_microsoft_navigation_table() {
        expect("A", 4, 0, OutputCsiCursorKind::CursorUp, 4, 0);
        expect("B", 0, 0, OutputCsiCursorKind::CursorDown, 1, 0);
        expect("C", 2, 0, OutputCsiCursorKind::CursorForward, 2, 0);
        expect("D", 3, 0, OutputCsiCursorKind::CursorBackward, 3, 0);
        expect("E", 5, 0, OutputCsiCursorKind::CursorNextLine, 5, 0);
        expect("F", 6, 0, OutputCsiCursorKind::CursorPreviousLine, 6, 0);
        expect("G", 7, 0, OutputCsiCursorKind::CursorHorizontalAbsolute, 7, 0);
        expect("`", 8, 0, OutputCsiCursorKind::CursorHorizontalAbsolute, 8, 0);
        expect("d", 9, 0, OutputCsiCursorKind::VerticalLineAbsolute, 9, 0);
        expect("a", 10, 0, OutputCsiCursorKind::HorizontalRelative, 10, 0);
        expect("e", 11, 0, OutputCsiCursorKind::VerticalRelative, 11, 0);
        expect("H", 12, 13, OutputCsiCursorKind::CursorPosition, 12, 13);
        expect("f", 14, 15, OutputCsiCursorKind::CursorPosition, 14, 15);
        expect("m", 1, 0, OutputCsiCursorKind::None, 0, 0);
    }

    #[test]
    fn csi_cursor_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_cursor_plan(0, 0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiCursorPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_cursor_plan(
                0xff00_0000_0000_0000,
                0,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
