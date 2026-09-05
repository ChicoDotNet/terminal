use std::ptr;

use terminal_parser::output_engine::{
    DeviceAttributesKind, OutputAction, OutputStateMachineEngine, TermDispatch,
};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputVt52Kind {
    None = 0,
    CursorUp = 1,
    CursorDown = 2,
    CursorForward = 3,
    CursorBackward = 4,
    Designate94Charset = 5,
    CursorPosition = 6,
    ReverseLineFeed = 7,
    EraseInDisplay = 8,
    EraseInLine = 9,
    DeviceAttributesVt52 = 10,
    SetKeypadMode = 11,
    SetAnsiMode = 12,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputVt52Plan {
    pub kind: u32,
    pub argument1: i32,
    pub argument2: i32,
    pub reserved: u32,
    pub payload: u64,
}

impl Default for OutputVt52Plan {
    fn default() -> Self {
        Self {
            kind: OutputVt52Kind::None as u32,
            argument1: 0,
            argument2: 0,
            reserved: 0,
            payload: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputVt52Plan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::CursorUp(count) => plan(OutputVt52Kind::CursorUp, count, 0, 0),
            OutputAction::CursorDown(count) => plan(OutputVt52Kind::CursorDown, count, 0, 0),
            OutputAction::CursorForward(count) => {
                plan(OutputVt52Kind::CursorForward, count, 0, 0)
            }
            OutputAction::CursorBackward(count) => {
                plan(OutputVt52Kind::CursorBackward, count, 0, 0)
            }
            OutputAction::Designate94Charset { slot, charset } => plan(
                OutputVt52Kind::Designate94Charset,
                i32::from(slot),
                0,
                charset,
            ),
            OutputAction::CursorPosition { line, column } => {
                plan(OutputVt52Kind::CursorPosition, line, column, 0)
            }
            OutputAction::ReverseLineFeed => plan(OutputVt52Kind::ReverseLineFeed, 0, 0, 0),
            OutputAction::EraseInDisplay(kind) => {
                plan(OutputVt52Kind::EraseInDisplay, kind, 0, 0)
            }
            OutputAction::EraseInLine(kind) => plan(OutputVt52Kind::EraseInLine, kind, 0, 0),
            OutputAction::DeviceAttributes(DeviceAttributesKind::Vt52) => {
                plan(OutputVt52Kind::DeviceAttributesVt52, 0, 0, 0)
            }
            OutputAction::SetKeypadMode(enabled) => plan(
                OutputVt52Kind::SetKeypadMode,
                i32::from(u8::from(enabled)),
                0,
                0,
            ),
            OutputAction::SetMode {
                private: true,
                enabled: true,
                mode: 2,
            } => plan(OutputVt52Kind::SetAnsiMode, 0, 0, 0),
            _ => OutputVt52Plan::default(),
        };
    }
}

const fn plan(
    kind: OutputVt52Kind,
    argument1: i32,
    argument2: i32,
    payload: u64,
) -> OutputVt52Plan {
    OutputVt52Plan {
        kind: kind as u32,
        argument1,
        argument2,
        reserved: 0,
        payload,
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

/// Maps one packed Windows Terminal VT52 `VTID` through the existing Rust
/// output engine and returns one compact semantic dispatch plan. The two raw
/// VT52 parameter bytes are preserved as signed integers until the Rust owner
/// applies protocol semantics such as direct cursor-address normalization.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_vt52_plan(
    identifier: u64,
    parameter0: i32,
    parameter1: i32,
    out_plan: *mut OutputVt52Plan,
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
        let _ = engine.action_vt52_esc_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the C ABI requires
        // it to reference one writable `OutputVt52Plan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{OutputVt52Kind, OutputVt52Plan, terminal_parser_ffi_output_vt52_plan};
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(
        id: &str,
        parameter0: i32,
        parameter1: i32,
        kind: OutputVt52Kind,
        argument1: i32,
        argument2: i32,
        payload: u64,
    ) {
        let mut result = OutputVt52Plan::default();
        assert_eq!(
            terminal_parser_ffi_output_vt52_plan(
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
        assert_eq!(result.payload, payload, "id={id:?}");
    }

    #[test]
    fn output_vt52_ffi_replays_complete_microsoft_dispatch_table() {
        expect("A", 0, 0, OutputVt52Kind::CursorUp, 1, 0, 0);
        expect("B", 0, 0, OutputVt52Kind::CursorDown, 1, 0, 0);
        expect("C", 0, 0, OutputVt52Kind::CursorForward, 1, 0, 0);
        expect("D", 0, 0, OutputVt52Kind::CursorBackward, 1, 0, 0);
        expect(
            "F",
            0,
            0,
            OutputVt52Kind::Designate94Charset,
            0,
            0,
            u64::from(b'0'),
        );
        expect(
            "G",
            0,
            0,
            OutputVt52Kind::Designate94Charset,
            0,
            0,
            u64::from(b'B'),
        );
        expect("H", 0, 0, OutputVt52Kind::CursorPosition, 1, 1, 0);
        expect("I", 0, 0, OutputVt52Kind::ReverseLineFeed, 0, 0, 0);
        expect("J", 0, 0, OutputVt52Kind::EraseInDisplay, 0, 0, 0);
        expect("K", 0, 0, OutputVt52Kind::EraseInLine, 0, 0, 0);
        expect(
            "Y",
            i32::from(b' ') + 4,
            i32::from(b' ') + 9,
            OutputVt52Kind::CursorPosition,
            5,
            10,
            0,
        );
        expect(
            "Z",
            0,
            0,
            OutputVt52Kind::DeviceAttributesVt52,
            0,
            0,
            0,
        );
        expect("=", 0, 0, OutputVt52Kind::SetKeypadMode, 1, 0, 0);
        expect(">", 0, 0, OutputVt52Kind::SetKeypadMode, 0, 0, 0);
        expect("<", 0, 0, OutputVt52Kind::SetAnsiMode, 0, 0, 0);
        expect("?", 0, 0, OutputVt52Kind::None, 0, 0, 0);
    }

    #[test]
    fn output_vt52_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_vt52_plan(
                VtId::from_ascii("A").value(),
                0,
                0,
                std::ptr::null_mut(),
            ),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputVt52Plan::default();
        assert_eq!(
            terminal_parser_ffi_output_vt52_plan(
                0x0100_0000_0000_0000,
                0,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }

    #[test]
    fn output_vt52_plan_layout_is_stable() {
        assert_eq!(std::mem::size_of::<OutputVt52Plan>(), 24);
        assert_eq!(std::mem::align_of::<OutputVt52Plan>(), 8);
    }
}
