use std::ptr;

use terminal_parser::output_engine::{LineFeedType, OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::StateMachineEngine;

use super::{FfiStatus, ffi_guard};

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OutputExecuteKind {
    None = 0,
    EnquireAnswerback = 1,
    WarningBell = 2,
    CursorBackward = 3,
    ForwardTab = 4,
    CarriageReturn = 5,
    LineFeedDependsOnMode = 6,
    LockingShift = 7,
    Print = 8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputExecutePlan {
    pub kind: u32,
    pub argument: u32,
}

impl Default for OutputExecutePlan {
    fn default() -> Self {
        Self {
            kind: OutputExecuteKind::None as u32,
            argument: 0,
        }
    }
}

#[derive(Default)]
struct PlanDispatch {
    plan: OutputExecutePlan,
}

impl TermDispatch for PlanDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.plan = match action {
            OutputAction::EnquireAnswerback => plan(OutputExecuteKind::EnquireAnswerback, 0),
            OutputAction::WarningBell => plan(OutputExecuteKind::WarningBell, 0),
            OutputAction::CursorBackward(count) => {
                plan(OutputExecuteKind::CursorBackward, u32::try_from(count).unwrap_or_default())
            }
            OutputAction::ForwardTab(count) => {
                plan(OutputExecuteKind::ForwardTab, u32::try_from(count).unwrap_or_default())
            }
            OutputAction::CarriageReturn => plan(OutputExecuteKind::CarriageReturn, 0),
            OutputAction::LineFeed(LineFeedType::DependsOnMode) => {
                plan(OutputExecuteKind::LineFeedDependsOnMode, 0)
            }
            OutputAction::LockingShift(slot) => plan(OutputExecuteKind::LockingShift, u32::from(slot)),
            OutputAction::Print(code_unit) => plan(OutputExecuteKind::Print, u32::from(code_unit)),
            _ => OutputExecutePlan::default(),
        };
    }
}

const fn plan(kind: OutputExecuteKind, argument: u32) -> OutputExecutePlan {
    OutputExecutePlan {
        kind: kind as u32,
        argument,
    }
}

/// Maps one C0/DEL code unit through the existing Rust output engine and returns
/// a compact semantic dispatch plan. No Windows or C++ types cross this ABI.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_output_execute_plan(
    code_unit: u16,
    out_plan: *mut OutputExecutePlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }

        let mut engine = OutputStateMachineEngine::new(PlanDispatch::default());
        let _ = engine.action_execute(code_unit);
        let dispatch = engine.into_dispatch();

        // SAFETY: `out_plan` was checked non-null above and the C ABI requires
        // it to reference one writable `OutputExecutePlan` for this call.
        unsafe { ptr::write(out_plan, dispatch.plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::{
        OutputExecuteKind, OutputExecutePlan, terminal_parser_ffi_output_execute_plan,
    };
    use crate::FfiStatus;

    fn expect(code_unit: u16, kind: OutputExecuteKind, argument: u32) {
        let mut plan = OutputExecutePlan::default();
        assert_eq!(
            terminal_parser_ffi_output_execute_plan(code_unit, &mut plan),
            FfiStatus::Ok
        );
        assert_eq!(plan.kind, kind as u32);
        assert_eq!(plan.argument, argument);
    }

    #[test]
    fn output_execute_ffi_replays_the_existing_rust_engine_contract() {
        expect(0x05, OutputExecuteKind::EnquireAnswerback, 0);
        expect(0x07, OutputExecuteKind::WarningBell, 0);
        expect(0x08, OutputExecuteKind::CursorBackward, 1);
        expect(0x09, OutputExecuteKind::ForwardTab, 1);
        expect(0x0d, OutputExecuteKind::CarriageReturn, 0);
        for code_unit in [0x0a, 0x0b, 0x0c] {
            expect(code_unit, OutputExecuteKind::LineFeedDependsOnMode, 0);
        }
        expect(0x0f, OutputExecuteKind::LockingShift, 0);
        expect(0x0e, OutputExecuteKind::LockingShift, 1);
        expect(0x1a, OutputExecuteKind::Print, 0x2426);
        expect(0x7f, OutputExecuteKind::Print, 0x7f);
        expect(0x01, OutputExecuteKind::None, 0);
    }

    #[test]
    fn output_execute_ffi_validates_the_output_pointer() {
        assert_eq!(
            terminal_parser_ffi_output_execute_plan(0x07, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
    }

    #[test]
    fn output_execute_plan_layout_is_stable() {
        assert_eq!(std::mem::size_of::<OutputExecutePlan>(), 8);
        assert_eq!(std::mem::align_of::<OutputExecutePlan>(), 4);
    }
}
