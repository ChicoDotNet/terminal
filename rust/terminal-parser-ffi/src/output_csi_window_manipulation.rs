use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiWindowManipulationKind {
    None = 0,
    WindowManipulation = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiWindowManipulationPlan {
    pub kind: u32,
    pub function: i32,
    pub parameter1: i32,
    pub parameter2: i32,
}

impl Default for OutputCsiWindowManipulationPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiWindowManipulationKind::None as u32,
            function: 0,
            parameter1: 0,
            parameter2: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiWindowManipulationPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::WindowManipulation {
                function,
                parameter1,
                parameter2,
            } => OutputCsiWindowManipulationPlan {
                kind: OutputCsiWindowManipulationKind::WindowManipulation as u32,
                function,
                parameter1,
                parameter2,
            },
            _ => OutputCsiWindowManipulationPlan::default(),
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

/// Replays CSI window-manipulation classification and numeric parameter
/// normalization through the Rust output engine. C++ remains the native
/// dispatch owner until this plan is independently replayed and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_window_manipulation_plan(
    identifier: u64,
    parameter0: i32,
    parameter1: i32,
    parameter2: i32,
    out_plan: *mut OutputCsiWindowManipulationPlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let parameters = Parameters::from_values(vec![
            Some(parameter0),
            Some(parameter1),
            Some(parameter2),
        ]);
        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_csi_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the ABI requires one
        // writable `OutputCsiWindowManipulationPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiWindowManipulationKind, OutputCsiWindowManipulationPlan,
        terminal_parser_ffi_output_csi_window_manipulation_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn replay(id: &str, parameters: [i32; 3]) -> OutputCsiWindowManipulationPlan {
        let mut plan = OutputCsiWindowManipulationPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_window_manipulation_plan(
                VtId::from_ascii(id).value(),
                parameters[0],
                parameters[1],
                parameters[2],
                &mut plan,
            ),
            FfiStatus::Ok
        );
        plan
    }

    #[test]
    fn csi_window_manipulation_replays_microsoft_contract() {
        let plan = replay("t", [8, 24, 80]);
        assert_eq!(
            plan.kind,
            OutputCsiWindowManipulationKind::WindowManipulation as u32
        );
        assert_eq!((plan.function, plan.parameter1, plan.parameter2), (8, 24, 80));
    }

    #[test]
    fn csi_window_manipulation_preserves_numeric_defaulting_and_rejects_unrelated_csi() {
        let plan = replay("t", [0, 0, 0]);
        assert_eq!(
            plan.kind,
            OutputCsiWindowManipulationKind::WindowManipulation as u32
        );
        assert_eq!((plan.function, plan.parameter1, plan.parameter2), (1, 1, 1));

        let unrelated = replay("m", [8, 24, 80]);
        assert_eq!(unrelated, OutputCsiWindowManipulationPlan::default());
    }

    #[test]
    fn csi_window_manipulation_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_window_manipulation_plan(
                VtId::from_ascii("t").value(),
                8,
                24,
                80,
                std::ptr::null_mut(),
            ),
            FfiStatus::InvalidArgument
        );

        let mut plan = OutputCsiWindowManipulationPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_window_manipulation_plan(
                0xff00_0000_0000_0000,
                8,
                24,
                80,
                &mut plan,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
