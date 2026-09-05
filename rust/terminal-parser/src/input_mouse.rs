//! Portable SGR mouse decoding shared by the parser and the native product seam.

pub const FROM_LEFT_1ST_BUTTON_PRESSED: u32 = 0x0001;
pub const RIGHTMOST_BUTTON_PRESSED: u32 = 0x0002;
pub const FROM_LEFT_2ND_BUTTON_PRESSED: u32 = 0x0004;

pub const MOUSE_MOVED: u32 = 0x0001;
pub const MOUSE_WHEELED: u32 = 0x0004;
pub const MOUSE_HWHEELED: u32 = 0x0008;

pub const SCROLL_DELTA_BACKWARD: u32 = 0xff80_0000;
pub const SCROLL_DELTA_FORWARD: u32 = 0x0080_0000;

const SGR_DRAG: u32 = 32;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SgrMousePlan {
    pub button_id: u32,
    pub button_state: u32,
    pub persistent_button_state: u32,
    pub event_flags: u32,
    pub track_click: bool,
}

#[must_use]
pub fn plan_sgr_mouse(previous_button_state: u32, encoding: u32, button_down: bool) -> Option<SgrMousePlan> {
    let button_id = (encoding & 0x3) | ((encoding & 0xc0) >> 4);
    let mut button_state = previous_button_state & 0xffff;
    let mut event_flags = 0u32;

    let button_flag = match button_id {
        0 => FROM_LEFT_1ST_BUTTON_PRESSED,
        1 => FROM_LEFT_2ND_BUTTON_PRESSED,
        2 => RIGHTMOST_BUTTON_PRESSED,
        3 => 0,
        4 => {
            button_state |= SCROLL_DELTA_FORWARD;
            event_flags |= MOUSE_WHEELED;
            0
        }
        5 => {
            button_state |= SCROLL_DELTA_BACKWARD;
            event_flags |= MOUSE_WHEELED;
            0
        }
        6 => {
            button_state |= SCROLL_DELTA_BACKWARD;
            event_flags |= MOUSE_HWHEELED;
            0
        }
        7 => {
            button_state |= SCROLL_DELTA_FORWARD;
            event_flags |= MOUSE_HWHEELED;
            0
        }
        _ => return None,
    };

    if button_down {
        button_state |= button_flag;
    } else {
        button_state &= !button_flag;
    }

    if encoding & SGR_DRAG != 0 {
        event_flags |= MOUSE_MOVED;
    }

    Some(SgrMousePlan {
        button_id,
        button_state,
        persistent_button_state: button_state & 0xffff,
        event_flags,
        track_click: button_down && matches!(button_id, 0..=2),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn press_release_and_drag_preserve_low_word_button_state() {
        let left = plan_sgr_mouse(0, 0, true).unwrap();
        assert_eq!(left.button_id, 0);
        assert_eq!(left.button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert_eq!(left.persistent_button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert!(left.track_click);

        let drag = plan_sgr_mouse(left.persistent_button_state, 32, true).unwrap();
        assert_eq!(drag.button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert_eq!(drag.event_flags, MOUSE_MOVED);

        let released = plan_sgr_mouse(drag.persistent_button_state, 0, false).unwrap();
        assert_eq!(released.button_state, 0);
        assert_eq!(released.persistent_button_state, 0);
        assert!(!released.track_click);
    }

    #[test]
    fn primary_button_ids_match_windows_terminal_mapping() {
        assert_eq!(plan_sgr_mouse(0, 0, true).unwrap().button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert_eq!(plan_sgr_mouse(0, 1, true).unwrap().button_state, FROM_LEFT_2ND_BUTTON_PRESSED);
        assert_eq!(plan_sgr_mouse(0, 2, true).unwrap().button_state, RIGHTMOST_BUTTON_PRESSED);
        assert_eq!(plan_sgr_mouse(7, 3, true).unwrap().button_state, 7);
    }

    #[test]
    fn releasing_one_primary_button_preserves_other_pressed_buttons() {
        let all_primary = FROM_LEFT_1ST_BUTTON_PRESSED
            | FROM_LEFT_2ND_BUTTON_PRESSED
            | RIGHTMOST_BUTTON_PRESSED;

        let release_middle = plan_sgr_mouse(all_primary, 1, false).unwrap();
        assert_eq!(
            release_middle.persistent_button_state,
            FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED
        );
        assert_eq!(release_middle.event_flags, 0);
        assert!(!release_middle.track_click);

        let release_right = plan_sgr_mouse(release_middle.persistent_button_state, 2, false).unwrap();
        assert_eq!(release_right.persistent_button_state, FROM_LEFT_1ST_BUTTON_PRESSED);

        let release_left = plan_sgr_mouse(release_right.persistent_button_state, 0, false).unwrap();
        assert_eq!(release_left.persistent_button_state, 0);
    }

    #[test]
    fn wheel_events_use_transient_high_word_and_do_not_persist_it() {
        let previous = FROM_LEFT_1ST_BUTTON_PRESSED;
        let cases = [
            (64, SCROLL_DELTA_FORWARD, MOUSE_WHEELED),
            (65, SCROLL_DELTA_BACKWARD, MOUSE_WHEELED),
            (66, SCROLL_DELTA_BACKWARD, MOUSE_HWHEELED),
            (67, SCROLL_DELTA_FORWARD, MOUSE_HWHEELED),
        ];

        for (encoding, delta, flags) in cases {
            let plan = plan_sgr_mouse(previous, encoding, true).unwrap();
            assert_eq!(plan.button_state, previous | delta);
            assert_eq!(plan.persistent_button_state, previous);
            assert_eq!(plan.event_flags, flags);
            assert!(!plan.track_click);
        }
    }

    #[test]
    fn unsupported_button_id_is_rejected() {
        assert!(plan_sgr_mouse(0, 128, true).is_none());
    }
}
