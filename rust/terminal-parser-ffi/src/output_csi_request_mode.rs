use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiRequestModeKind {
    None = 0,
    RequestMode = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiRequestModePlan {
    pub kind: u32,
    pub private_mode: u32,
    pub mode: i32,
    pub reserved: u32,
}

impl Default for OutputCsiRequestModePlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiRequestModeKind::None as u32,
            private_mode: 0,
            mode: 0,
            reserved: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiRequestModePlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::RequestMode { private, mode } => OutputCsiRequestModePlan {
                kind: OutputCsiRequestModeKind::RequestMode as u32,
                private_mode: u32::from(private),
                mode,
                reserved: 0,
            },
            _ => OutputCsiRequestModePlan::default(),
        };
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

/// Replays ANSI/DEC CSI mode-request classification through the Rust output engine.
/// Unrelated CSI actions return `None`, preserving native ownership until this
/// slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_request_mode_plan(
    identifier: u64,
    parameter0: i32,
    out_plan: *mut OutputCsiRequestModePlan,
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

        // SAFETY: `out_plan` was checked non-null above and the ABI requires one
        // writable `OutputCsiRequestModePlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiRequestModeKind, OutputCsiRequestModePlan,
        terminal_parser_ffi_output_csi_request_mode_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(id: &str, parameter0: i32, kind: OutputCsiRequestModeKind, private_mode: u32, mode: i32) {
        let mut result = OutputCsiRequestModePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_request_mode_plan(
                VtId::from_ascii(id).value(),
                parameter0,
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
        assert_eq!(result.private_mode, private_mode, "id={id:?}");
        assert_eq!(result.mode, mode, "id={id:?}");
    }

    #[test]
    fn csi_request_mode_ffi_replays_microsoft_contract() {
        expect("$p", 4, OutputCsiRequestModeKind::RequestMode, 0, 4);
        expect("?$p", 25, OutputCsiRequestModeKind::RequestMode, 1, 25);
        expect("m", 3, OutputCsiRequestModeKind::None, 0, 0);
    }

    #[test]
    fn csi_request_mode_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_request_mode_plan(0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiRequestModePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_request_mode_plan(
                0xff00_0000_0000_0000,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
