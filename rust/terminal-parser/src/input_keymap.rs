//! Deterministic VT input key mappings shared by the Rust parser and product FFI.
//!
//! These tables correspond to the portable portions of the C++
//! `InputStateMachineEngine` CSI/Generic/SS3 key maps. Windows-specific scan
//! code generation and `INPUT_RECORD` synthesis remain native.

const VK_PRIOR: u16 = 0x21;
const VK_NEXT: u16 = 0x22;
const VK_END: u16 = 0x23;
const VK_HOME: u16 = 0x24;
const VK_LEFT: u16 = 0x25;
const VK_UP: u16 = 0x26;
const VK_RIGHT: u16 = 0x27;
const VK_DOWN: u16 = 0x28;
const VK_INSERT: u16 = 0x2d;
const VK_DELETE: u16 = 0x2e;
const VK_F1: u16 = 0x70;
const VK_F2: u16 = 0x71;
const VK_F3: u16 = 0x72;
const VK_F4: u16 = 0x73;
const VK_F5: u16 = 0x74;
const VK_F6: u16 = 0x75;
const VK_F7: u16 = 0x76;
const VK_F8: u16 = 0x77;
const VK_F9: u16 = 0x78;
const VK_F10: u16 = 0x79;
const VK_F11: u16 = 0x7a;
const VK_F12: u16 = 0x7b;

const SHIFT_PRESSED: u32 = 0x0010;
const LEFT_ALT_PRESSED: u32 = 0x0002;
const LEFT_CTRL_PRESSED: u32 = 0x0008;
const ENHANCED_KEY: u32 = 0x0100;
const VT_SHIFT: u32 = 1;
const VT_ALT: u32 = 2;
const VT_CTRL: u32 = 4;
const SGR_SHIFT: u32 = 4;
const SGR_META: u32 = 8;
const SGR_CTRL: u32 = 16;

/// Maps a single-byte CSI final character to the Windows virtual-key value.
#[must_use]
pub const fn cursor_virtual_key(final_character: u16) -> Option<u16> {
    match final_character {
        0x41 => Some(VK_UP),
        0x42 => Some(VK_DOWN),
        0x43 => Some(VK_RIGHT),
        0x44 => Some(VK_LEFT),
        0x48 => Some(VK_HOME),
        0x46 => Some(VK_END),
        0x50 => Some(VK_F1),
        0x51 => Some(VK_F2),
        0x52 => Some(VK_F3),
        0x53 => Some(VK_F4),
        _ => None,
    }
}

/// Maps a CSI `~` generic-key identifier to the Windows virtual-key value.
#[must_use]
pub const fn generic_virtual_key(identifier: i32) -> Option<u16> {
    match identifier {
        1 => Some(VK_HOME),
        2 => Some(VK_INSERT),
        3 => Some(VK_DELETE),
        4 => Some(VK_END),
        5 => Some(VK_PRIOR),
        6 => Some(VK_NEXT),
        15 => Some(VK_F5),
        17 => Some(VK_F6),
        18 => Some(VK_F7),
        19 => Some(VK_F8),
        20 => Some(VK_F9),
        21 => Some(VK_F10),
        23 => Some(VK_F11),
        24 => Some(VK_F12),
        _ => None,
    }
}

/// Maps a single-byte SS3 final character to the Windows virtual-key value.
#[must_use]
pub const fn ss3_virtual_key(final_character: u16) -> Option<u16> {
    cursor_virtual_key(final_character)
}

/// Converts a VT encoded modifier parameter to Windows input modifier flags.
///
/// VT encodes modifiers as `1 + bitflags`; missing/default parameters are
/// represented by `1` at the ABI boundary.
#[must_use]
pub const fn vt_modifier_state(modifier_parameter: u32) -> u32 {
    let encoded = modifier_parameter.saturating_sub(1);
    let mut modifiers = 0;
    if encoded & VT_SHIFT != 0 {
        modifiers |= SHIFT_PRESSED;
    }
    if encoded & VT_ALT != 0 {
        modifiers |= LEFT_ALT_PRESSED;
    }
    if encoded & VT_CTRL != 0 {
        modifiers |= LEFT_CTRL_PRESSED;
    }
    modifiers
}

/// Composes VT modifier translation with the enhanced-key bit required by
/// mapped CSI cursor/navigation keys. CSI F1-F4 are not enhanced keys.
#[must_use]
pub const fn cursor_modifier_state(final_character: u16, modifier_parameter: u32) -> u32 {
    let mut modifiers = vt_modifier_state(modifier_parameter);
    if let Some(virtual_key) = cursor_virtual_key(final_character) {
        if virtual_key != VK_F1
            && virtual_key != VK_F2
            && virtual_key != VK_F3
            && virtual_key != VK_F4
        {
            modifiers |= ENHANCED_KEY;
        }
    }
    modifiers
}

/// Composes VT modifier translation with the enhanced-key bit required by
/// generic navigation keys (Home/Insert/Delete/End/PageUp/PageDown).
#[must_use]
pub const fn generic_modifier_state(identifier: i32, modifier_parameter: u32) -> u32 {
    let mut modifiers = vt_modifier_state(modifier_parameter);
    if identifier >= 1 && identifier <= 6 && generic_virtual_key(identifier).is_some() {
        modifiers |= ENHANCED_KEY;
    }
    modifiers
}

/// Normalizes the parser's signed/optional VT parameter representation before
/// applying the shared modifier mapping.
#[must_use]
pub fn vt_modifier_state_from_parameter(modifier_parameter: Option<i32>) -> u32 {
    let normalized = modifier_parameter.unwrap_or(1).max(1);
    let normalized = u32::try_from(normalized).unwrap_or(1);
    vt_modifier_state(normalized)
}

/// Extracts Shift/Alt/Ctrl state from the SGR mouse encoding bitfield.
#[must_use]
pub const fn sgr_mouse_modifier_state(encoding: u32) -> u32 {
    let mut modifiers = 0;
    if encoding & SGR_SHIFT != 0 {
        modifiers |= SHIFT_PRESSED;
    }
    if encoding & SGR_META != 0 {
        modifiers |= LEFT_ALT_PRESSED;
    }
    if encoding & SGR_CTRL != 0 {
        modifiers |= LEFT_CTRL_PRESSED;
    }
    modifiers
}

/// Preserves the parser's signed SGR encoding bit pattern before applying the
/// shared modifier mapping.
#[must_use]
pub const fn sgr_mouse_modifier_state_from_encoding(encoding: i32) -> u32 {
    sgr_mouse_modifier_state(u32::from_ne_bytes(encoding.to_ne_bytes()))
}

#[cfg(test)]
mod tests {
    use super::{
        ENHANCED_KEY, LEFT_ALT_PRESSED, LEFT_CTRL_PRESSED, SHIFT_PRESSED, cursor_modifier_state,
        cursor_virtual_key, generic_modifier_state, generic_virtual_key, sgr_mouse_modifier_state,
        sgr_mouse_modifier_state_from_encoding, ss3_virtual_key, vt_modifier_state,
        vt_modifier_state_from_parameter,
    };

    const CURSOR_AND_SS3_CASES: [(u8, u16); 10] = [
        (b'A', 0x26),
        (b'B', 0x28),
        (b'C', 0x27),
        (b'D', 0x25),
        (b'H', 0x24),
        (b'F', 0x23),
        (b'P', 0x70),
        (b'Q', 0x71),
        (b'R', 0x72),
        (b'S', 0x73),
    ];

    const GENERIC_CASES: [(i32, u16); 14] = [
        (1, 0x24),
        (2, 0x2d),
        (3, 0x2e),
        (4, 0x23),
        (5, 0x21),
        (6, 0x22),
        (15, 0x74),
        (17, 0x75),
        (18, 0x76),
        (19, 0x77),
        (20, 0x78),
        (21, 0x79),
        (23, 0x7a),
        (24, 0x7b),
    ];

    #[test]
    fn cursor_map_matches_microsoft_input_engine_table() {
        for (final_character, vkey) in CURSOR_AND_SS3_CASES {
            assert_eq!(cursor_virtual_key(u16::from(final_character)), Some(vkey));
        }
        assert_eq!(cursor_virtual_key(u16::from(b'X')), None);
    }

    #[test]
    fn generic_map_matches_microsoft_input_engine_table() {
        for (identifier, vkey) in GENERIC_CASES {
            assert_eq!(generic_virtual_key(identifier), Some(vkey));
        }
        for unmapped in [0, 7, 14, 16, 22, 25, i32::MAX] {
            assert_eq!(generic_virtual_key(unmapped), None);
        }
    }

    #[test]
    fn ss3_map_matches_microsoft_input_engine_table() {
        for (final_character, vkey) in CURSOR_AND_SS3_CASES {
            assert_eq!(ss3_virtual_key(u16::from(final_character)), Some(vkey));
        }
        assert_eq!(ss3_virtual_key(u16::from(b'X')), None);
    }

    #[test]
    fn vt_modifier_contract_replays_microsoft_bit_translation() {
        let cases = [
            (1, 0),
            (2, SHIFT_PRESSED),
            (3, LEFT_ALT_PRESSED),
            (4, SHIFT_PRESSED | LEFT_ALT_PRESSED),
            (5, LEFT_CTRL_PRESSED),
            (6, SHIFT_PRESSED | LEFT_CTRL_PRESSED),
            (7, LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED),
            (8, SHIFT_PRESSED | LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED),
        ];
        for (parameter, expected) in cases {
            assert_eq!(vt_modifier_state(parameter), expected);
        }
        assert_eq!(vt_modifier_state(0), 0);
    }

    #[test]
    fn key_modifier_contract_replays_microsoft_enhanced_key_rules() {
        assert_eq!(cursor_modifier_state(u16::from(b'A'), 1), ENHANCED_KEY);
        assert_eq!(
            cursor_modifier_state(u16::from(b'H'), 8),
            ENHANCED_KEY | 0x001a
        );
        for final_character in [b'P', b'Q', b'R', b'S'] {
            assert_eq!(cursor_modifier_state(u16::from(final_character), 1), 0);
        }
        assert_eq!(cursor_modifier_state(u16::from(b'X'), 8), 0x001a);

        for identifier in 1..=6 {
            assert_eq!(generic_modifier_state(identifier, 1), ENHANCED_KEY);
        }
        assert_eq!(generic_modifier_state(6, 8), ENHANCED_KEY | 0x001a);
        assert_eq!(generic_modifier_state(15, 1), 0);
        assert_eq!(generic_modifier_state(0, 8), 0x001a);
    }

    #[test]
    fn parser_vt_modifier_adapter_preserves_default_and_lower_bound_semantics() {
        assert_eq!(vt_modifier_state_from_parameter(None), 0);
        assert_eq!(vt_modifier_state_from_parameter(Some(-1)), 0);
        assert_eq!(vt_modifier_state_from_parameter(Some(0)), 0);
        assert_eq!(vt_modifier_state_from_parameter(Some(1)), 0);
        assert_eq!(
            vt_modifier_state_from_parameter(Some(8)),
            SHIFT_PRESSED | LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED
        );
    }

    #[test]
    fn sgr_mouse_modifier_contract_replays_microsoft_bit_translation() {
        let cases = [
            (0, 0),
            (4, SHIFT_PRESSED),
            (8, LEFT_ALT_PRESSED),
            (16, LEFT_CTRL_PRESSED),
            (28, SHIFT_PRESSED | LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED),
            (32 | 28, SHIFT_PRESSED | LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED),
            (0xc0 | 28, SHIFT_PRESSED | LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED),
        ];
        for (encoding, expected) in cases {
            assert_eq!(sgr_mouse_modifier_state(encoding), expected);
        }
    }

    #[test]
    fn parser_sgr_modifier_adapter_preserves_signed_bit_pattern() {
        for encoding in [0, 4, 8, 16, 28, 60, 220, -1] {
            assert_eq!(
                sgr_mouse_modifier_state_from_encoding(encoding),
                sgr_mouse_modifier_state(u32::from_ne_bytes(encoding.to_ne_bytes()))
            );
        }
    }
}
