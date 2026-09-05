use std::ptr;

use terminal_parser::input_control::classify_control_character;

use super::{FfiStatus, ffi_guard};

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct TerminalParserFfiControlCharacterPlan {
    pub kind: u32,
    pub character: u16,
    pub forced_virtual_key: u16,
    pub write_ctrl: u32,
    pub clear_layout_modifiers: u32,
}

/// Classifies one input code unit without crossing keyboard-layout or Win32 types.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_control_character_plan(
    code_unit: u16,
    write_alt: u32,
    out_plan: *mut TerminalParserFfiControlCharacterPlan,
) -> FfiStatus {
    ffi_guard(|| {
        if out_plan.is_null() || write_alt > 1 {
            return FfiStatus::InvalidArgument;
        }

        let plan = classify_control_character(code_unit, write_alt != 0);
        let ffi_plan = TerminalParserFfiControlCharacterPlan {
            kind: plan.kind as u32,
            character: plan.character,
            forced_virtual_key: plan.forced_virtual_key,
            write_ctrl: u32::from(plan.write_ctrl),
            clear_layout_modifiers: u32::from(plan.clear_layout_modifiers),
        };
        unsafe { ptr::write(out_plan, ffi_plan) };
        FfiStatus::Ok
    })
}

#[cfg(test)]
mod tests {
    use terminal_parser::input_control::ControlCharacterKind;

    use super::*;

    #[test]
    fn control_character_plan_layout_matches_c_header() {
        assert_eq!(std::mem::size_of::<TerminalParserFfiControlCharacterPlan>(), 16);
        assert_eq!(std::mem::offset_of!(TerminalParserFfiControlCharacterPlan, kind), 0);
        assert_eq!(
            std::mem::offset_of!(TerminalParserFfiControlCharacterPlan, character),
            4
        );
        assert_eq!(
            std::mem::offset_of!(TerminalParserFfiControlCharacterPlan, forced_virtual_key),
            6
        );
        assert_eq!(
            std::mem::offset_of!(TerminalParserFfiControlCharacterPlan, write_ctrl),
            8
        );
        assert_eq!(
            std::mem::offset_of!(TerminalParserFfiControlCharacterPlan, clear_layout_modifiers),
            12
        );
    }

    #[test]
    fn control_character_kind_values_match_c_header() {
        assert_eq!(ControlCharacterKind::Print as u32, 0);
        assert_eq!(ControlCharacterKind::CtrlC as u32, 1);
        assert_eq!(ControlCharacterKind::MappedC0 as u32, 2);
        assert_eq!(ControlCharacterKind::DeleteAsBackspace as u32, 3);
    }

    #[test]
    fn replays_ctrl_c_and_alt_ctrl_c_split() {
        let mut plan = TerminalParserFfiControlCharacterPlan::default();
        assert_eq!(
            terminal_parser_ffi_input_control_character_plan(0x03, 0, &mut plan),
            FfiStatus::Ok
        );
        assert_eq!(plan.kind, ControlCharacterKind::CtrlC as u32);
        assert_eq!(plan.character, 0x03);
        assert_eq!(plan.forced_virtual_key, u16::from(b'C'));
        assert_eq!(plan.write_ctrl, 1);
        assert_eq!(plan.clear_layout_modifiers, 1);

        assert_eq!(
            terminal_parser_ffi_input_control_character_plan(0x03, 1, &mut plan),
            FfiStatus::Ok
        );
        assert_eq!(plan.kind, ControlCharacterKind::MappedC0 as u32);
        assert_eq!(plan.character, 0x03);
        assert_eq!(plan.forced_virtual_key, 0);
        assert_eq!(plan.write_ctrl, 1);
        assert_eq!(plan.clear_layout_modifiers, 0);
    }

    #[test]
    fn replays_every_c0_execution_plan_across_ffi() {
        let mut plan = TerminalParserFfiControlCharacterPlan::default();

        for code_unit in 0u16..0x20 {
            assert_eq!(
                terminal_parser_ffi_input_control_character_plan(code_unit, 1, &mut plan),
                FfiStatus::Ok
            );
            assert_eq!(plan.kind, ControlCharacterKind::MappedC0 as u32);

            let expected_character = if code_unit == 0x08 { 0x7f } else { code_unit };
            let expected_virtual_key = if code_unit == 0x1b { 0x1b } else { 0 };
            let expected_write_ctrl = u32::from(!matches!(code_unit, 0x09 | 0x0d | 0x1b));
            let expected_clear_layout_modifiers = u32::from(matches!(code_unit, 0x08 | 0x0d | 0x1b));

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
    fn replays_delete_and_printable_paths() {
        let mut plan = TerminalParserFfiControlCharacterPlan::default();

        assert_eq!(
            terminal_parser_ffi_input_control_character_plan(0x7f, 1, &mut plan),
            FfiStatus::Ok
        );
        assert_eq!(plan.kind, ControlCharacterKind::DeleteAsBackspace as u32);
        assert_eq!(plan.character, 0x08);
        assert_eq!(plan.forced_virtual_key, 0x08);
        assert_eq!(plan.write_ctrl, 0);
        assert_eq!(plan.clear_layout_modifiers, 1);

        assert_eq!(
            terminal_parser_ffi_input_control_character_plan(u16::from(b'A'), 0, &mut plan),
            FfiStatus::Ok
        );
        assert_eq!(plan.kind, ControlCharacterKind::Print as u32);
        assert_eq!(plan.character, u16::from(b'A'));
        assert_eq!(plan.forced_virtual_key, 0);
        assert_eq!(plan.write_ctrl, 0);
        assert_eq!(plan.clear_layout_modifiers, 0);
    }

    #[test]
    fn rejects_invalid_abi_arguments() {
        let mut plan = TerminalParserFfiControlCharacterPlan::default();
        assert_eq!(
            terminal_parser_ffi_input_control_character_plan(0, 2, &mut plan),
            FfiStatus::InvalidArgument
        );
        assert_eq!(
            terminal_parser_ffi_input_control_character_plan(0, 0, std::ptr::null_mut()),
            FfiStatus::InvalidArgument
        );
    }
}
