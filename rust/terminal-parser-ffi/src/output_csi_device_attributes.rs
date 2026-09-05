use std::ptr;

use terminal_parser::output_engine::{
    DeviceAttributesKind, OutputAction, OutputStateMachineEngine, TermDispatch,
};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiDeviceAttributesKind {
    None = 0,
    Primary = 1,
    Secondary = 2,
    Tertiary = 3,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiDeviceAttributesPlan {
    pub kind: u32,
    pub reserved0: u32,
    pub reserved1: u32,
    pub reserved2: u32,
}

impl Default for OutputCsiDeviceAttributesPlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiDeviceAttributesKind::None as u32,
            reserved0: 0,
            reserved1: 0,
            reserved2: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiDeviceAttributesPlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::DeviceAttributes(kind) => OutputCsiDeviceAttributesPlan {
                kind: match kind {
                    DeviceAttributesKind::Primary => OutputCsiDeviceAttributesKind::Primary as u32,
                    DeviceAttributesKind::Secondary => {
                        OutputCsiDeviceAttributesKind::Secondary as u32
                    }
                    DeviceAttributesKind::Tertiary => {
                        OutputCsiDeviceAttributesKind::Tertiary as u32
                    }
                    DeviceAttributesKind::Vt52 => OutputCsiDeviceAttributesKind::None as u32,
                },
                reserved0: 0,
                reserved1: 0,
                reserved2: 0,
            },
            _ => OutputCsiDeviceAttributesPlan::default(),
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

/// Replays CSI device-attributes dispatch through the existing Rust output engine.
/// Unrelated CSI actions return `None`, preserving native C++ ownership until
/// this slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_device_attributes_plan(
    identifier: u64,
    parameter0: i32,
    out_plan: *mut OutputCsiDeviceAttributesPlan,
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
        // one writable `OutputCsiDeviceAttributesPlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiDeviceAttributesKind, OutputCsiDeviceAttributesPlan,
        terminal_parser_ffi_output_csi_device_attributes_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(id: &str, parameter0: i32, kind: OutputCsiDeviceAttributesKind) {
        let mut result = OutputCsiDeviceAttributesPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_device_attributes_plan(
                VtId::from_ascii(id).value(),
                parameter0,
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}, parameter0={parameter0}");
    }

    #[test]
    fn csi_device_attributes_ffi_replays_microsoft_contract() {
        expect("c", 0, OutputCsiDeviceAttributesKind::Primary);
        expect(">c", 0, OutputCsiDeviceAttributesKind::Secondary);
        expect("=c", 0, OutputCsiDeviceAttributesKind::Tertiary);
        expect("c", 1, OutputCsiDeviceAttributesKind::None);
        expect(">c", 7, OutputCsiDeviceAttributesKind::None);
        expect("=c", 9, OutputCsiDeviceAttributesKind::None);
        expect("m", 0, OutputCsiDeviceAttributesKind::None);
    }

    #[test]
    fn csi_device_attributes_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_device_attributes_plan(0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiDeviceAttributesPlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_device_attributes_plan(
                0xff00_0000_0000_0000,
                0,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
