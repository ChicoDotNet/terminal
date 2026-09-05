use std::ptr;

use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{Parameters, StateMachineEngine, VtId};

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputCsiCursorRestoreKind {
    None = 0,
    Restore = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCsiCursorRestorePlan {
    pub kind: u32,
    pub reserved0: u32,
    pub reserved1: u32,
    pub reserved2: u32,
}

impl Default for OutputCsiCursorRestorePlan {
    fn default() -> Self {
        Self {
            kind: OutputCsiCursorRestoreKind::None as u32,
            reserved0: 0,
            reserved1: 0,
            reserved2: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputCsiCursorRestorePlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::CursorRestoreState => OutputCsiCursorRestorePlan {
                kind: OutputCsiCursorRestoreKind::Restore as u32,
                reserved0: 0,
                reserved1: 0,
                reserved2: 0,
            },
            _ => OutputCsiCursorRestorePlan::default(),
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

/// Replays CSI cursor-restore dispatch through the existing Rust output engine.
/// Unrelated CSI actions return `None`, preserving native C++ ownership until
/// this slice is independently verified and promoted.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_csi_cursor_restore_plan(
    identifier: u64,
    out_plan: *mut OutputCsiCursorRestorePlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }
        let Some(id) = vt_id_from_value(identifier) else {
            return FfiStatus::InvalidArgument;
        };

        let parameters = Parameters::default();
        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_csi_dispatch(id, &parameters);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the ABI requires
        // one writable `OutputCsiCursorRestorePlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputCsiCursorRestoreKind, OutputCsiCursorRestorePlan,
        terminal_parser_ffi_output_csi_cursor_restore_plan,
    };
    use crate::FfiStatus;
    use terminal_parser::state_machine::VtId;

    fn expect(id: &str, kind: OutputCsiCursorRestoreKind) {
        let mut result = OutputCsiCursorRestorePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_cursor_restore_plan(
                VtId::from_ascii(id).value(),
                &mut result,
            ),
            FfiStatus::Ok
        );
        assert_eq!(result.kind, kind as u32, "id={id:?}");
    }

    #[test]
    fn csi_cursor_restore_ffi_replays_microsoft_contract() {
        expect("u", OutputCsiCursorRestoreKind::Restore);
        expect("m", OutputCsiCursorRestoreKind::None);
    }

    #[test]
    fn csi_cursor_restore_ffi_validates_pointer_and_identifier() {
        assert_eq!(
            terminal_parser_ffi_output_csi_cursor_restore_plan(0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
        let mut result = OutputCsiCursorRestorePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_csi_cursor_restore_plan(
                0xff00_0000_0000_0000,
                &mut result,
            ),
            FfiStatus::InvalidArgument
        );
    }
}
