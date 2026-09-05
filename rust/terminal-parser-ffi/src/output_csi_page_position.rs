use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiPagePositionKind {
    None = 0,
    Absolute = 1,
    Relative = 2,
    Back = 3,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiPagePositionPlan {
    pub kind: u32,
    pub count: i32,
    pub reserved0: u32,
    pub reserved1: u32,
}

impl Default for OutputCsiPagePositionPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiPagePositionKind::None as u32,
            count: 0,
            reserved0: 0,
            reserved1: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiPagePositionPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::PagePositionAbsolute(count) => {
                plan(OutputCsiPagePositionKind::Absolute, count)
            }
            OutputAction::PagePositionRelative(count) => {
                plan(OutputCsiPagePositionKind::Relative, count)
            }
            OutputAction::PagePositionBack(count) => plan(OutputCsiPagePositionKind::Back, count),
            _ => OutputCsiPagePositionPlan::default(),
        };
    }
}

const fn plan(kind: OutputCsiPagePositionKind, count: i32) -> OutputCsiPagePositionPlan {
    OutputCsiPagePositionPlan {
        kind: kind as u32,
        count,
        reserved0: 0,
        reserved1: 0,
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

/// Replays CSI page-positioning actions through the existing Rust output engine.
/// Unrelated CSI actions return `None`, preserving native C++ ownership until
/// this slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_page_position_plan(
    identifier: u64,
    parameter0: i32,
    out_plan: *mut OutputCsiPagePositionPlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let parameters = Parameters::from_values(vec![Some(parameter0)]);
        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_csi_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the ABI requires
        // one writable `OutputCsiPagePositionPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiPagePositionKind, OutputCsiPagePositionPlan,
        terminal_parser_ffi_output_csi_page_position_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(id: &str, parameter0: i32, kind: OutputCsiPagePositionKind, count: i32) {
        let mut result = OutputCsiPagePositionPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_page_position_plan(
                VtId::from_ascii(id).value(),
                parameter0,
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
        assert_eq!(result.count, count, "id={id:?}");
    }

    #[test]
    fn csi_page_position_ffi_replays_microsoft_page_position_contract() {
        expect(" P", 0, OutputCsiPagePositionKind::Absolute, 1);
        expect(" P", 4, OutputCsiPagePositionKind::Absolute, 4);
        expect(" Q", 0, OutputCsiPagePositionKind::Relative, 1);
        expect(" Q", 5, OutputCsiPagePositionKind::Relative, 5);
        expect(" R", 0, OutputCsiPagePositionKind::Back, 1);
        expect(" R", 6, OutputCsiPagePositionKind::Back, 6);
        expect("m", 3, OutputCsiPagePositionKind::None, 0);
    }

    #[test]
    fn csi_page_position_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_page_position_plan(0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiPagePositionPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_page_position_plan(
                0xff00_0000_0000_0000,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
