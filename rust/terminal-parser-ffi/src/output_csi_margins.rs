use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiMarginsKind {
    None = 0,
    TopBottom = 1,
    LeftRight = 2,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiMarginsPlan {
    pub kind: u32,
    pub first: i32,
    pub second: i32,
    pub reserved: u32,
}

impl Default for OutputCsiMarginsPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiMarginsKind::None as u32,
            first: 0,
            second: 0,
            reserved: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiMarginsPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::SetTopBottomScrollingMargins { top, bottom } => {
                plan(OutputCsiMarginsKind::TopBottom, top, bottom)
            }
            OutputAction::SetLeftRightScrollingMargins { left, right } => {
                plan(OutputCsiMarginsKind::LeftRight, left, right)
            }
            _ => OutputCsiMarginsPlan::default(),
        };
    }
}

const fn plan(kind: OutputCsiMarginsKind, first: i32, second: i32) -> OutputCsiMarginsPlan {
    OutputCsiMarginsPlan {
        kind: kind as u32,
        first,
        second,
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

/// Replays the compact CSI scrolling-margin slice through the existing Rust
/// output engine. Unrelated CSI actions return `None`, so C++ keeps ownership
/// until each remaining slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_margins_plan(
    identifier: u64,
    parameter0: i32,
    parameter1: i32,
    out_plan: *mut OutputCsiMarginsPlan,
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
        // it to reference one writable `OutputCsiMarginsPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiMarginsKind, OutputCsiMarginsPlan, terminal_parser_ffi_output_csi_margins_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(
        id: &str,
        parameter0: i32,
        parameter1: i32,
        kind: OutputCsiMarginsKind,
        first: i32,
        second: i32,
    ) {
        let mut result = OutputCsiMarginsPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_margins_plan(
                VtId::from_ascii(id).value(),
                parameter0,
                parameter1,
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
        assert_eq!(result.first, first, "id={id:?}");
        assert_eq!(result.second, second, "id={id:?}");
    }

    #[test]
    fn csi_margins_ffi_replays_microsoft_margin_table() {
        expect("r", 0, 0, OutputCsiMarginsKind::TopBottom, 0, 0);
        expect("r", 3, 40, OutputCsiMarginsKind::TopBottom, 3, 40);
        expect("s", 0, 0, OutputCsiMarginsKind::LeftRight, 0, 0);
        expect("s", 5, 70, OutputCsiMarginsKind::LeftRight, 5, 70);
        expect("m", 1, 2, OutputCsiMarginsKind::None, 0, 0);
    }

    #[test]
    fn csi_margins_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_margins_plan(0, 0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiMarginsPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_margins_plan(
                0xff00_0000_0000_0000,
                0,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
