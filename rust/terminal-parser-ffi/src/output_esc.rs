use std::ptr;

use terminal_parser::output_engine::{
    DeviceAttributesKind, LineFeedType, LineRendition, OutputAction, OutputStateMachineEngine,
    TermDispatch,
};
use terminal_parser::state_machine::{StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputEscKind {
    None = 0,
    BackIndex = 1,
    CursorSaveState = 2,
    CursorRestoreState = 3,
    ForwardIndex = 4,
    SetKeypadMode = 5,
    LineFeedWithReturn = 6,
    LineFeedWithoutReturn = 7,
    ReverseLineFeed = 8,
    HorizontalTabSet = 9,
    DeviceAttributesPrimary = 10,
    HardReset = 11,
    SingleShift = 12,
    LockingShift = 13,
    LockingShiftRight = 14,
    AcceptC1Controls = 15,
    SendC1Controls = 16,
    AnnounceCodeStructure = 17,
    SetLineRendition = 18,
    ScreenAlignmentPattern = 19,
    DesignateCodingSystem = 20,
    Designate94Charset = 21,
    Designate96Charset = 22,
}

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputEscLineRendition {
    SingleWidth = 0,
    DoubleWidth = 1,
    DoubleHeightTop = 2,
    DoubleHeightBottom = 3,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputEscPlan {
    pub kind: u32,
    pub argument: u32,
    pub payload: u64,
}

impl Default for OutputEscPlan {
    fn default() -> Self {
        Self {
            kind: OutputEscKind::None as u32,
            argument: 0,
            payload: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputEscPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::BackIndex => plan(OutputEscKind::BackIndex, 0, 0),
            OutputAction::CursorSaveState => plan(OutputEscKind::CursorSaveState, 0, 0),
            OutputAction::CursorRestoreState => plan(OutputEscKind::CursorRestoreState, 0, 0),
            OutputAction::ForwardIndex => plan(OutputEscKind::ForwardIndex, 0, 0),
            OutputAction::SetKeypadMode(enabled) => {
                plan(OutputEscKind::SetKeypadMode, u32::from(u8::from(enabled)), 0)
            }
            OutputAction::LineFeed(LineFeedType::WithReturn) => {
                plan(OutputEscKind::LineFeedWithReturn, 0, 0)
            }
            OutputAction::LineFeed(LineFeedType::WithoutReturn) => {
                plan(OutputEscKind::LineFeedWithoutReturn, 0, 0)
            }
            OutputAction::ReverseLineFeed => plan(OutputEscKind::ReverseLineFeed, 0, 0),
            OutputAction::HorizontalTabSet => plan(OutputEscKind::HorizontalTabSet, 0, 0),
            OutputAction::DeviceAttributes(DeviceAttributesKind::Primary) => {
                plan(OutputEscKind::DeviceAttributesPrimary, 0, 0)
            }
            OutputAction::HardReset => plan(OutputEscKind::HardReset, 0, 0),
            OutputAction::SingleShift(slot) => {
                plan(OutputEscKind::SingleShift, u32::from(slot), 0)
            }
            OutputAction::LockingShift(slot) => {
                plan(OutputEscKind::LockingShift, u32::from(slot), 0)
            }
            OutputAction::LockingShiftRight(slot) => {
                plan(OutputEscKind::LockingShiftRight, u32::from(slot), 0)
            }
            OutputAction::AcceptC1Controls(enabled) => plan(
                OutputEscKind::AcceptC1Controls,
                u32::from(u8::from(enabled)),
                0,
            ),
            OutputAction::SendC1Controls(enabled) => plan(
                OutputEscKind::SendC1Controls,
                u32::from(u8::from(enabled)),
                0,
            ),
            OutputAction::AnnounceCodeStructure(level) => {
                plan(OutputEscKind::AnnounceCodeStructure, u32::from(level), 0)
            }
            OutputAction::SetLineRendition(rendition) => plan(
                OutputEscKind::SetLineRendition,
                line_rendition(rendition) as u32,
                0,
            ),
            OutputAction::ScreenAlignmentPattern => {
                plan(OutputEscKind::ScreenAlignmentPattern, 0, 0)
            }
            OutputAction::DesignateCodingSystem(charset) => {
                plan(OutputEscKind::DesignateCodingSystem, 0, charset)
            }
            OutputAction::Designate94Charset { slot, charset } => {
                plan(OutputEscKind::Designate94Charset, u32::from(slot), charset)
            }
            OutputAction::Designate96Charset { slot, charset } => {
                plan(OutputEscKind::Designate96Charset, u32::from(slot), charset)
            }
            _ => OutputEscPlan::default(),
        };
    }
}

const fn plan(kind: OutputEscKind, argument: u32, payload: u64) -> OutputEscPlan {
    OutputEscPlan {
        kind: kind as u32,
        argument,
        payload,
    }
}

const fn line_rendition(rendition: LineRendition) -> OutputEscLineRendition {
    match rendition {
        LineRendition::SingleWidth => OutputEscLineRendition::SingleWidth,
        LineRendition::DoubleWidth => OutputEscLineRendition::DoubleWidth,
        LineRendition::DoubleHeightTop => OutputEscLineRendition::DoubleHeightTop,
        LineRendition::DoubleHeightBottom => OutputEscLineRendition::DoubleHeightBottom,
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

/// Maps one packed Windows Terminal `VTID` through the existing Rust output
/// engine and returns one compact semantic ESC dispatch plan. No Windows or C++
/// types cross this ABI.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_esc_plan(
    identifier: u64,
    out_plan: *mut OutputEscPlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_esc_dispatch(id);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the C ABI requires
        // it to reference one writable `OutputEscPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputEscKind, OutputEscLineRendition, OutputEscPlan,
        terminal_parser_ffi_output_esc_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(id: &str, kind: OutputEscKind, argument: u32, payload: u64) {
        let mut result = OutputEscPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_esc_plan(VtId::from_ascii(id).value(), &mut result),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
        assert_eq!(result.argument, argument, "id={id:?}");
        assert_eq!(result.payload, payload, "id={id:?}");
    }

    #[test]
    fn output_esc_ffi_replays_representative_engine_actions() {
        expect("\\", OutputEscKind::None, 0, 0);
        expect("6", OutputEscKind::BackIndex, 0, 0);
        expect("=", OutputEscKind::SetKeypadMode, 1, 0);
        expect(">", OutputEscKind::SetKeypadMode, 0, 0);
        expect("E", OutputEscKind::LineFeedWithReturn, 0, 0);
        expect("D", OutputEscKind::LineFeedWithoutReturn, 0, 0);
        expect("N", OutputEscKind::SingleShift, 2, 0);
        expect("n", OutputEscKind::LockingShift, 2, 0);
        expect("~", OutputEscKind::LockingShiftRight, 1, 0);
        expect(" 7", OutputEscKind::AcceptC1Controls, 1, 0);
        expect(" F", OutputEscKind::SendC1Controls, 0, 0);
        expect(" L", OutputEscKind::AnnounceCodeStructure, 1, 0);
        expect(
            "#3",
            OutputEscKind::SetLineRendition,
            OutputEscLineRendition::DoubleHeightTop as u32,
            0,
        );
        expect("#8", OutputEscKind::ScreenAlignmentPattern, 0, 0);
        expect("%G", OutputEscKind::DesignateCodingSystem, 0, u64::from(b'G'));
        expect("(B", OutputEscKind::Designate94Charset, 0, u64::from(b'B'));
        expect("/A", OutputEscKind::Designate96Charset, 3, u64::from(b'A'));
    }

    #[test]
    fn output_esc_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_esc_plan(
                VtId::from_ascii("6").value(),
                std::ptr::null_mut()
            ),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputEscPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_esc_plan(0x0100_0000_0000_0000, &mut result),
            FfiStatus::InvalidArgument
        );
    }

    #[test]
    fn output_esc_plan_layout_is_stable() {
        assert_eq!(std::mem::size_of::<OutputEscPlan>(), 16);
        assert_eq!(std::mem::align_of::<OutputEscPlan>(), 8);
    }
}
