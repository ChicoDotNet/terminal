use std::{ptr, slice};

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{MAX_PARAMETER_COUNT, Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiModeKind {
    None = 0,
    Mode = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiModePlan {
    pub kind: u32,
    pub private_mode: u32,
    pub enabled: u32,
    pub mode: i32,
}

impl Default for OutputCsiModePlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiModeKind::None as u32,
            private_mode: 0,
            enabled: 0,
            mode: 0,
        }
    }
}

fn mode_plan(action: OutputAction) -> Option<OutputCsiModePlan> {
    match action {
        OutputAction::SetMode {
            private,
            enabled,
            mode,
        } => Some(OutputCsiModePlan {
            kind: OutputCsiModeKind::Mode as u32,
            private_mode: u32::from(private),
            enabled: u32::from(enabled),
            mode,
        }),
        _ => None,
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiModePlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = mode_plan(action).unwrap_or_default();
    }
}

#[derive(Default)]
struct BatchPlanDispatch {
    plans: Vec<OutputCsiModePlan>,
}

impl TermDispatch for BatchPlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        if let Some(plan) = mode_plan(action) {
            self.plans.push(plan);
        }
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

/// Replays one ANSI/DEC CSI set/reset-mode parameter through the Rust output
/// engine. Retained as the smallest single-parameter ABI witness while product
/// ownership moves to the batch contract below.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_mode_plan(
    identifier: u64,
    mode: i32,
    out_plan: *mut OutputCsiModePlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let parameters = Parameters::from_values(vec![Some(mode)]);
        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_csi_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the ABI requires one
        // writable `OutputCsiModePlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

/// Replays the complete ANSI/DEC CSI set/reset-mode parameter list through the
/// Rust output engine. Inputs and outputs remain caller-owned; Rust allocates no
/// memory whose ownership crosses the ABI boundary.
///
/// Call with `out_plans = null` and `output_capacity = 0` to query the required
/// number of plans. An empty parameter list is significant: Windows Terminal's
/// VT contract treats it as one default mode parameter with value zero.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_mode_plans(
    identifier: u64,
    modes: *const i32,
    mode_count: usize,
    out_plans: *mut OutputCsiModePlan,
    output_capacity: usize,
    out_count: *mut usize,
) -> FfiStatus {
    ffi_guard(|| {
        if out_count.is_null()
            || mode_count > MAX_PARAMETER_COUNT
            || (modes.is_null() && mode_count != 0)
            || (out_plans.is_null() && output_capacity != 0)
        {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let modes = if mode_count == 0 {
            &[]
        } else {
            // SAFETY: The ABI contract requires `modes` to reference
            // `mode_count` readable i32 values; null with non-zero count was
            // rejected above.
            unsafe { slice::from_raw_parts(modes, mode_count) }
        };
        let parameters = Parameters::from_values(modes.iter().copied().map(Some).collect());
        let mut engine = OutputStateMachineEngine::new(BatchPlanDispatch::default());
        let _ = engine.action_csi_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();
        let required = dispatch.plans.len();

        // SAFETY: `out_count` was checked non-null above and the ABI requires
        // one writable usize for the duration of this call.
        unsafe { ptr::write(out_count, required) };

        if output_capacity < required {
            return FfiStatus::BufferTooSmall;
        }
        if required != 0 {
            if out_plans.is_null() {
                return FfiStatus::InvalidArgument;
            }
            // SAFETY: `output_capacity >= required`; caller guarantees a
            // writable array of `OutputCsiModePlan` values and it cannot
            // overlap Rust-owned `dispatch.plans` storage.
            unsafe { ptr::copy_nonoverlapping(dispatch.plans.as_ptr(), out_plans, required) };
        }

        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiModeKind, OutputCsiModePlan, terminal_parser_ffi_output_csi_mode_plan,
        terminal_parser_ffi_output_csi_mode_plans,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::{MAX_PARAMETER_COUNT, VtId};

    fn expect(id: &str, mode: i32, private_mode: u32, enabled: u32) {
        let mut result = OutputCsiModePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plan(VtId::from_ascii(id).value(), mode, &mut result),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, OutputCsiModeKind::Mode as u32, "id={id:?}");
        assert_eq!(result.private_mode, private_mode, "id={id:?}");
        assert_eq!(result.enabled, enabled, "id={id:?}");
        assert_eq!(result.mode, mode, "id={id:?}");
    }

    fn batch(id: &str, modes: &[i32]) -> Vec<OutputCsiModePlan> {
        let mut required = 0usize;
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                VtId::from_ascii(id).value(),
                modes.as_ptr(),
                modes.len(),
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::BufferTooSmall
        );
        let mut plans = vec![OutputCsiModePlan::default(); required];
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                VtId::from_ascii(id).value(),
                modes.as_ptr(),
                modes.len(),
                plans.as_mut_ptr(),
                plans.len(),
                &mut required,
            ),
            FfiStatus::Ok
        );
        plans
    }

    #[test]
    fn csi_mode_ffi_replays_set_and_reset_contracts() {
        expect("h", 4, 0, 1);
        expect("?h", 25, 1, 1);
        expect("l", 4, 0, 0);
        expect("?l", 25, 1, 0);

        let mut unrelated = OutputCsiModePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plan(
                VtId::from_ascii("m").value(),
                3,
                &mut unrelated,
            ),
            FfiStatus::Ok
        );
        assert_eq!(unrelated.kind, OutputCsiModeKind::None as u32);
    }

    #[test]
    fn csi_mode_batch_replays_every_parameter_in_order() {
        for (id, private_mode, enabled) in [
            ("h", 0, 1),
            ("?h", 1, 1),
            ("l", 0, 0),
            ("?l", 1, 0),
        ] {
            let plans = batch(id, &[4, 20, 25]);
            assert_eq!(plans.len(), 3, "id={id:?}");
            assert_eq!(
                plans.iter().map(|plan| plan.mode).collect::<Vec<_>>(),
                [4, 20, 25],
                "id={id:?}"
            );
            assert!(plans.iter().all(|plan| {
                plan.kind == OutputCsiModeKind::Mode as u32
                    && plan.private_mode == private_mode
                    && plan.enabled == enabled
            }));
        }
    }

    #[test]
    fn csi_mode_batch_preserves_empty_parameter_default_and_unrelated_csi() {
        let plans = batch("h", &[]);
        assert_eq!(plans.len(), 1);
        assert_eq!(plans[0].mode, 0);
        assert_eq!(plans[0].enabled, 1);

        let mut count = usize::MAX;
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                VtId::from_ascii("m").value(),
                std::ptr::null(),
                0,
                std::ptr::null_mut(),
                0,
                &mut count,
            ),
            FfiStatus::Ok
        );
        assert_eq!(count, 0);
    }

    #[test]
    fn csi_mode_batch_reports_capacity_and_validates_arguments() {
        let modes = [4, 20];
        let mut required = 0usize;
        let mut one = OutputCsiModePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                VtId::from_ascii("h").value(),
                modes.as_ptr(),
                modes.len(),
                &mut one,
                1,
                &mut required,
            ),
            FfiStatus::BufferTooSmall
        );
        assert_eq!(required, 2);

        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                VtId::from_ascii("h").value(),
                std::ptr::null(),
                1,
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                VtId::from_ascii("h").value(),
                modes.as_ptr(),
                MAX_PARAMETER_COUNT + 1,
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                VtId::from_ascii("h").value(),
                modes.as_ptr(),
                modes.len(),
                std::ptr::null_mut(),
                1,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plans(
                0xff00_0000_0000_0000,
                modes.as_ptr(),
                modes.len(),
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
    }

    #[test]
    fn csi_mode_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plan(0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiModePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_mode_plan(
                0xff00_0000_0000_0000,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
