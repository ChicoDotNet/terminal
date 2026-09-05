use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiTerminalParametersKind {
    None = 0,
    Request = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiTerminalParametersPlan {
    pub kind: u32,
    pub parameter: i32,
    pub reserved0: u32,
    pub reserved1: u32,
}

impl Default for OutputCsiTerminalParametersPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiTerminalParametersKind::None as u32,
            parameter: 0,
            reserved0: 0,
            reserved1: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiTerminalParametersPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::RequestTerminalParameters(parameter) => OutputCsiTerminalParametersPlan {
                kind: OutputCsiTerminalParametersKind::Request as u32,
                parameter,
                reserved0: 0,
                reserved1: 0,
            },
            _ => OutputCsiTerminalParametersPlan::default(),
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

/// Replays CSI request-terminal-parameters through the existing Rust output engine.
/// Unrelated CSI actions return `None`, preserving native C++ ownership until
/// this slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_terminal_parameters_plan(
    identifier: u64,
    parameter0: i32,
    out_plan: *mut OutputCsiTerminalParametersPlan,
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
        // one writable `OutputCsiTerminalParametersPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiTerminalParametersKind, OutputCsiTerminalParametersPlan,
        terminal_parser_ffi_output_csi_terminal_parameters_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(id: &str, parameter0: i32, kind: OutputCsiTerminalParametersKind, parameter: i32) {
        let mut result = OutputCsiTerminalParametersPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_terminal_parameters_plan(
                VtId::from_ascii(id).value(),
                parameter0,
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
        assert_eq!(result.parameter, parameter, "id={id:?}");
    }

    #[test]
    fn csi_terminal_parameters_ffi_replays_microsoft_contract() {
        expect("x", 0, OutputCsiTerminalParametersKind::Request, 0);
        expect("x", 1, OutputCsiTerminalParametersKind::Request, 1);
        expect("x", 9, OutputCsiTerminalParametersKind::Request, 9);
        expect("m", 3, OutputCsiTerminalParametersKind::None, 0);
    }

    #[test]
    fn csi_terminal_parameters_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_terminal_parameters_plan(0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiTerminalParametersPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_terminal_parameters_plan(
                0xff00_0000_0000_0000,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
