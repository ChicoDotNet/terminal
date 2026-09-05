use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiScrollKind {
    None = 0,
    ScrollUp = 1,
    ScrollDown = 2,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiScrollPlan {
    pub kind: u32,
    pub count: i32,
    pub reserved0: u32,
    pub reserved1: u32,
}

impl Default for OutputCsiScrollPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiScrollKind::None as u32,
            count: 0,
            reserved0: 0,
            reserved1: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiScrollPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::ScrollUp(count) => plan(OutputCsiScrollKind::ScrollUp, count),
            OutputAction::ScrollDown(count) => plan(OutputCsiScrollKind::ScrollDown, count),
            _ => OutputCsiScrollPlan::default(),
        };
    }
}

const fn plan(kind: OutputCsiScrollKind, count: i32) -> OutputCsiScrollPlan {
    OutputCsiScrollPlan {
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

/// Replays CSI scrolling through the existing Rust output engine.
/// Unrelated CSI actions return `None`, preserving native C++ ownership until
/// this slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_scroll_plan(
    identifier: u64,
    parameter0: i32,
    out_plan: *mut OutputCsiScrollPlan,
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
        // one writable `OutputCsiScrollPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiScrollKind, OutputCsiScrollPlan, terminal_parser_ffi_output_csi_scroll_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(id: &str, parameter0: i32, kind: OutputCsiScrollKind, count: i32) {
        let mut result = OutputCsiScrollPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_scroll_plan(
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
    fn csi_scroll_ffi_replays_microsoft_scroll_contract() {
        expect("S", 0, OutputCsiScrollKind::ScrollUp, 1);
        expect("S", 4, OutputCsiScrollKind::ScrollUp, 4);
        expect("T", 0, OutputCsiScrollKind::ScrollDown, 1);
        expect("T", 7, OutputCsiScrollKind::ScrollDown, 7);
        expect("m", 3, OutputCsiScrollKind::None, 0);
    }

    #[test]
    fn csi_scroll_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_scroll_plan(0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiScrollPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_scroll_plan(0xff00_0000_0000_0000, 0, &mut result),
            FfiStatus::InvalidArgument
        );
    }
}
