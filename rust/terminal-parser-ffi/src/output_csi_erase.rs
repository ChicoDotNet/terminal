use std::{ptr, slice};

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{MAX_PARAMETER_COUNT, Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiEraseKind {
    None = 0,
    EraseDisplay = 1,
    SelectiveEraseDisplay = 2,
    EraseLine = 3,
    SelectiveEraseLine = 4,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiErasePlan {
    pub kind: u32,
    pub value: i32,
}

impl Default for OutputCsiErasePlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiEraseKind::None as u32,
            value: 0,
        }
    }
}

fn erase_plan(action: OutputAction) -> Option<OutputCsiErasePlan> {
    let (kind, value) = match action {
        OutputAction::EraseInDisplay(value) => (OutputCsiEraseKind::EraseDisplay, value),
        OutputAction::SelectiveEraseInDisplay(value) => {
            (OutputCsiEraseKind::SelectiveEraseDisplay, value)
        }
        OutputAction::EraseInLine(value) => (OutputCsiEraseKind::EraseLine, value),
        OutputAction::SelectiveEraseInLine(value) => {
            (OutputCsiEraseKind::SelectiveEraseLine, value)
        }
        _ => return None,
    };
    Some(OutputCsiErasePlan {
        kind: kind as u32,
        value,
    })
}

#[derive(Default)]
struct BatchPlanDispatch {
    plans: Vec<OutputCsiErasePlan>,
}

impl TermDispatch for BatchPlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        if let Some(plan) = erase_plan(action) {
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

/// Replays the complete ANSI/selective CSI display/line erase parameter list
/// through the Rust output engine. Inputs and outputs remain caller-owned.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_erase_plans(
    identifier: u64,
    values: *const i32,
    value_count: usize,
    out_plans: *mut OutputCsiErasePlan,
    output_capacity: usize,
    out_count: *mut usize,
) -> FfiStatus {
    ffi_guard(|| {
        if out_count.is_null()
            || value_count > MAX_PARAMETER_COUNT
            || (values.is_null() && value_count != 0)
            || (out_plans.is_null() && output_capacity != 0)
        {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let values = if value_count == 0 {
            &[]
        } else {
            // SAFETY: The ABI contract requires `values` to reference
            // `value_count` readable i32 values; null with non-zero count was
            // rejected above.
            unsafe { slice::from_raw_parts(values, value_count) }
        };
        let parameters = Parameters::from_values(values.iter().copied().map(Some).collect());
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
            // writable array of plans and it cannot overlap Rust-owned storage.
            unsafe { ptr::copy_nonoverlapping(dispatch.plans.as_ptr(), out_plans, required) };
        }

        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{OutputCsiEraseKind, OutputCsiErasePlan, terminal_parser_ffi_output_csi_erase_plans};
    use crate::FfiStatus;
    use terminal_parser::state_machine::{MAX_PARAMETER_COUNT, VtId};

    fn batch(id: &str, values: &[i32]) -> Vec<OutputCsiErasePlan> {
        let mut required = 0usize;
        assert_eq!(
            terminal_parser_ffi_output_csi_erase_plans(
                VtId::from_ascii(id).value(),
                values.as_ptr(),
                values.len(),
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::BufferTooSmall
        );
        let mut plans = vec![OutputCsiErasePlan::default(); required];
        assert_eq!(
            terminal_parser_ffi_output_csi_erase_plans(
                VtId::from_ascii(id).value(),
                values.as_ptr(),
                values.len(),
                plans.as_mut_ptr(),
                plans.len(),
                &mut required,
            ),
            FfiStatus::Ok
        );
        plans
    }

    #[test]
    fn csi_erase_batch_replays_display_and_line_contracts() {
        for (id, expected_kind) in [
            ("J", OutputCsiEraseKind::EraseDisplay),
            ("?J", OutputCsiEraseKind::SelectiveEraseDisplay),
            ("K", OutputCsiEraseKind::EraseLine),
            ("?K", OutputCsiEraseKind::SelectiveEraseLine),
        ] {
            let plans = batch(id, &[0, 1, 2]);
            assert_eq!(plans.len(), 3, "id={id:?}");
            assert_eq!(plans.iter().map(|plan| plan.value).collect::<Vec<_>>(), [0, 1, 2]);
            assert!(plans.iter().all(|plan| plan.kind == expected_kind as u32));
        }
    }

    #[test]
    fn csi_erase_batch_preserves_empty_parameter_default_and_unrelated_csi() {
        let plans = batch("J", &[]);
        assert_eq!(plans.len(), 1);
        assert_eq!(plans[0].kind, OutputCsiEraseKind::EraseDisplay as u32);
        assert_eq!(plans[0].value, 0);

        let mut count = usize::MAX;
        assert_eq!(
            terminal_parser_ffi_output_csi_erase_plans(
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
    fn csi_erase_batch_reports_capacity_and_validates_arguments() {
        let values = [0, 1];
        let mut required = 0usize;
        let mut one = OutputCsiErasePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_erase_plans(
                VtId::from_ascii("J").value(),
                values.as_ptr(),
                values.len(),
                &mut one,
                1,
                &mut required,
            ),
            FfiStatus::BufferTooSmall
        );
        assert_eq!(required, 2);

        assert_eq!(
            terminal_parser_ffi_output_csi_erase_plans(
                VtId::from_ascii("J").value(),
                std::ptr::null(),
                1,
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_output_csi_erase_plans(
                VtId::from_ascii("J").value(),
                values.as_ptr(),
                MAX_PARAMETER_COUNT + 1,
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_output_csi_erase_plans(
                0xff00_0000_0000_0000,
                values.as_ptr(),
                values.len(),
                std::ptr::null_mut(),
                0,
                &mut required,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
