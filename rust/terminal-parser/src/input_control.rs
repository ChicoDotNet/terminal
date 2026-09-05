//! Portable classification for input-side control characters.
//!
//! This module owns the deterministic portion of Microsoft's `_DoControlCharacter`
//! contract. Keyboard-layout lookup and `INPUT_RECORD` synthesis stay native.

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ControlCharacterKind {
    Print = 0,
    CtrlC = 1,
    MappedC0 = 2,
    DeleteAsBackspace = 3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ControlCharacterPlan {
    pub kind: ControlCharacterKind,
    pub character: u16,
    pub forced_virtual_key: u16,
    pub write_ctrl: bool,
    pub clear_layout_modifiers: bool,
}

const VK_C: u16 = 0x43;

/// Classifies one UTF-16 code unit using Microsoft's input-engine control rules.
///
/// `write_alt` matters for Ctrl+C because ESC-prefixed ETX is treated as the
/// ordinary Ctrl+Alt+C path instead of the host-special Ctrl+C dispatch.
#[must_use]
pub const fn classify_control_character(code_unit: u16, write_alt: bool) -> ControlCharacterPlan {
    if code_unit == 0x03 && !write_alt {
        return ControlCharacterPlan {
            kind: ControlCharacterKind::CtrlC,
            character: 0x03,
            forced_virtual_key: VK_C,
            write_ctrl: true,
            clear_layout_modifiers: true,
        };
    }

    if code_unit < 0x20 {
        return match code_unit {
            0x08 => ControlCharacterPlan {
                kind: ControlCharacterKind::MappedC0,
                character: 0x7f,
                forced_virtual_key: 0,
                write_ctrl: true,
                clear_layout_modifiers: true,
            },
            0x0d => ControlCharacterPlan {
                kind: ControlCharacterKind::MappedC0,
                character: 0x0d,
                forced_virtual_key: 0,
                write_ctrl: false,
                clear_layout_modifiers: true,
            },
            0x1b => ControlCharacterPlan {
                kind: ControlCharacterKind::MappedC0,
                character: 0x1b,
                forced_virtual_key: 0x1b,
                write_ctrl: false,
                clear_layout_modifiers: true,
            },
            0x09 => ControlCharacterPlan {
                kind: ControlCharacterKind::MappedC0,
                character: 0x09,
                forced_virtual_key: 0,
                write_ctrl: false,
                clear_layout_modifiers: false,
            },
            _ => ControlCharacterPlan {
                kind: ControlCharacterKind::MappedC0,
                character: code_unit,
                forced_virtual_key: 0,
                write_ctrl: true,
                clear_layout_modifiers: false,
            },
        };
    }

    if code_unit == 0x7f {
        return ControlCharacterPlan {
            kind: ControlCharacterKind::DeleteAsBackspace,
            character: 0x08,
            forced_virtual_key: 0x08,
            write_ctrl: false,
            clear_layout_modifiers: true,
        };
    }

    ControlCharacterPlan {
        kind: ControlCharacterKind::Print,
        character: code_unit,
        forced_virtual_key: 0,
        write_ctrl: false,
        clear_layout_modifiers: false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ctrl_c_is_host_special_only_without_alt() {
        let plain = classify_control_character(0x03, false);
        assert_eq!(plain.kind, ControlCharacterKind::CtrlC);
        assert_eq!(plain.character, 0x03);
        assert_eq!(plain.forced_virtual_key, u16::from(b'C'));
        assert!(plain.write_ctrl);
        assert!(plain.clear_layout_modifiers);

        let alt = classify_control_character(0x03, true);
        assert_eq!(alt.kind, ControlCharacterKind::MappedC0);
        assert_eq!(alt.character, 0x03);
        assert_eq!(alt.forced_virtual_key, 0);
        assert!(alt.write_ctrl);
        assert!(!alt.clear_layout_modifiers);
    }

    #[test]
    fn replays_microsoft_c0_execution_contract() {
        for code_unit in 0u16..0x20 {
            let plan = classify_control_character(code_unit, true);
            assert_eq!(plan.kind, ControlCharacterKind::MappedC0);

            let expected_character = if code_unit == 0x08 { 0x7f } else { code_unit };
            let expected_virtual_key = if code_unit == 0x1b { 0x1b } else { 0 };
            let expected_write_ctrl = !matches!(code_unit, 0x09 | 0x0d | 0x1b);
            let expected_clear_layout_modifiers = matches!(code_unit, 0x08 | 0x0d | 0x1b);

            assert_eq!(plan.character, expected_character, "character for C0 {code_unit:#04x}");
            assert_eq!(
                plan.forced_virtual_key, expected_virtual_key,
                "forced virtual key for C0 {code_unit:#04x}"
            );
            assert_eq!(plan.write_ctrl, expected_write_ctrl, "Ctrl state for C0 {code_unit:#04x}");
            assert_eq!(
                plan.clear_layout_modifiers, expected_clear_layout_modifiers,
                "layout modifiers for C0 {code_unit:#04x}"
            );
        }
    }

    #[test]
    fn delete_and_printable_paths_remain_distinct() {
        let delete = classify_control_character(0x7f, true);
        assert_eq!(delete.kind, ControlCharacterKind::DeleteAsBackspace);
        assert_eq!(delete.character, 0x08);
        assert_eq!(delete.forced_virtual_key, 0x08);
        assert!(!delete.write_ctrl);
        assert!(delete.clear_layout_modifiers);

        let printable = classify_control_character(u16::from(b'A'), false);
        assert_eq!(printable.kind, ControlCharacterKind::Print);
        assert_eq!(printable.character, u16::from(b'A'));
        assert_eq!(printable.forced_virtual_key, 0);
        assert!(!printable.write_ctrl);
        assert!(!printable.clear_layout_modifiers);
    }
}
