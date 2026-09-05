use crate::{FfiStatus, ffi_guard};
use terminal_parser::input_mouse::plan_sgr_mouse;

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct FfiSgrMousePlan {
    pub valid: u32,
    pub button_id: u32,
    pub button_state: u32,
    pub persistent_button_state: u32,
    pub event_flags: u32,
    pub track_click: u32,
}

#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_sgr_mouse_plan(
    previous_button_state: u32,
    encoding: u32,
    button_down: u32,
    out_plan: *mut FfiSgrMousePlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() {
            return FfiStatus::InvalidArgument;
        }

        let plan = plan_sgr_mouse(previous_button_state, encoding, button_down != 0);
        let ffi_plan = match plan {
            Some(plan) => FfiSgrMousePlan {
                valid: 1,
                button_id: plan.button_id,
                button_state: plan.button_state,
                persistent_button_state: plan.persistent_button_state,
                event_flags: plan.event_flags,
                track_click: u32::from(plan.track_click),
            },
            None => FfiSgrMousePlan::default(),
        };

        // SAFETY: `out_plan` was checked non-null above and the ABI contract
        // requires it to reference one writable plan for the duration of the call.
        unsafe { std::ptr::write(out_plan, ffi_plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::{offset_of, size_of};
    use terminal_parser::input_mouse::{
        FROM_LEFT_1ST_BUTTON_PRESSED, MOUSE_WHEELED, SCROLL_DELTA_FORWARD,
    };

    #[test]
    fn sgr_mouse_plan_layout_matches_c_header() {
        assert_eq!(size_of::<FfiSgrMousePlan>(), 24);
        assert_eq!(offset_of!(FfiSgrMousePlan, valid), 0);
        assert_eq!(offset_of!(FfiSgrMousePlan, button_id), 4);
        assert_eq!(offset_of!(FfiSgrMousePlan, button_state), 8);
        assert_eq!(offset_of!(FfiSgrMousePlan, persistent_button_state), 12);
        assert_eq!(offset_of!(FfiSgrMousePlan, event_flags), 16);
        assert_eq!(offset_of!(FfiSgrMousePlan, track_click), 20);
    }

    #[test]
    fn sgr_mouse_ffi_replays_press_and_wheel_contracts() {
        let mut plan = FfiSgrMousePlan::default();
        assert_eq!(
            terminal_parser_ffi_input_sgr_mouse_plan(0, 0, 1, &mut plan),
            FfiStatus::Ok
        );
        assert_eq!(plan.valid, 1);
        assert_eq!(plan.button_id, 0);
        assert_eq!(plan.button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert_eq!(plan.persistent_button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert_eq!(plan.event_flags, 0);
        assert_eq!(plan.track_click, 1);

        assert_eq!(
            terminal_parser_ffi_input_sgr_mouse_plan(
                plan.persistent_button_state,
                64,
                1,
                &mut plan,
            ),
            FfiStatus::Ok
        );
        assert_eq!(plan.valid, 1);
        assert_eq!(
            plan.button_state,
            FROM_LEFT_1ST_BUTTON_PRESSED | SCROLL_DELTA_FORWARD
        );
        assert_eq!(plan.persistent_button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert_eq!(plan.event_flags, MOUSE_WHEELED);
        assert_eq!(plan.track_click, 0);
    }

    #[test]
    fn sgr_mouse_ffi_preserves_invalid_encoding_without_overloading_status() {
        let mut plan = FfiSgrMousePlan {
            valid: 1,
            ..FfiSgrMousePlan::default()
        };
        assert_eq!(
            terminal_parser_ffi_input_sgr_mouse_plan(0, 128, 1, &mut plan),
            FfiStatus::Ok
        );
        assert_eq!(plan, FfiSgrMousePlan::default());
        assert_eq!(
            terminal_parser_ffi_input_sgr_mouse_plan(0, 0, 1, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
    }
}
