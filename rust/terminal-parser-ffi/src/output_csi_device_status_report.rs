use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiDeviceStatusReportKind {
    None = 0,
    Report = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiDeviceStatusReportPlan {
    pub kind: u32,
    pub private_mode: u32,
    pub status: i32,
    pub has_id: u32,
    pub id: i32,
}

impl Default for OutputCsiDeviceStatusReportPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiDeviceStatusReportKind::None as u32,
            private_mode: 0,
            status: 0,
            has_id: 0,
            id: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiDeviceStatusReportPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::DeviceStatusReport {
                private,
                status,
                id,
            } => OutputCsiDeviceStatusReportPlan {
                kind: OutputCsiDeviceStatusReportKind::Report as u32,
                private_mode: u32::from(private),
                status,
                has_id: u32::from(id.is_some()),
                id: id.unwrap_or_default(),
            },
            _ => OutputCsiDeviceStatusReportPlan::default(),
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

/// Replays ANSI/DEC CSI device-status-report classification through the Rust
/// output engine. Unrelated CSI actions return `None`, preserving native
/// ownership until this slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_device_status_report_plan(
    identifier: u64,
    parameter0: i32,
    has_parameter1: u32,
    parameter1: i32,
    out_plan: *mut OutputCsiDeviceStatusReportPlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() || has_parameter1 > 1 {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let parameters = Parameters::from_values(vec![
            Some(parameter0),
            if has_parameter1 != 0 { Some(parameter1) } else { None },
        ]);
        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_csi_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the ABI requires one
        // writable `OutputCsiDeviceStatusReportPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiDeviceStatusReportKind, OutputCsiDeviceStatusReportPlan,
        terminal_parser_ffi_output_csi_device_status_report_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(
        id: &str,
        parameter0: i32,
        parameter1: Option<i32>,
        kind: OutputCsiDeviceStatusReportKind,
        private_mode: u32,
        status: i32,
        expected_id: Option<i32>,
    ) {
        let mut result = OutputCsiDeviceStatusReportPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_device_status_report_plan(
                VtId::from_ascii(id).value(),
                parameter0,
                u32::from(parameter1.is_some()),
                parameter1.unwrap_or_default(),
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
        assert_eq!(result.private_mode, private_mode, "id={id:?}");
        assert_eq!(result.status, status, "id={id:?}");
        assert_eq!(result.has_id, u32::from(expected_id.is_some()), "id={id:?}");
        assert_eq!(result.id, expected_id.unwrap_or_default(), "id={id:?}");
    }

    #[test]
    fn csi_device_status_report_ffi_replays_microsoft_contract() {
        expect("n", 5, None, OutputCsiDeviceStatusReportKind::Report, 0, 5, None);
        expect("?n", 6, Some(42), OutputCsiDeviceStatusReportKind::Report, 1, 6, Some(42));
        expect("m", 3, None, OutputCsiDeviceStatusReportKind::None, 0, 0, None);
    }

    #[test]
    fn csi_device_status_report_ffi_validates_pointer_identifier_and_presence_flag() {
        assert_eq!(
            terminal_parser_ffi_output_csi_device_status_report_plan(0, 0, 0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiDeviceStatusReportPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_device_status_report_plan(
                0xff00_0000_0000_0000,
                0,
                0,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_output_csi_device_status_report_plan(
                VtId::from_ascii("n").value(),
                5,
                2,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
