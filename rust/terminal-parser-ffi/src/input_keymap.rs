use terminal_parser::input_keymap::{
    cursor_modifier_state, cursor_virtual_key, generic_modifier_state, generic_virtual_key,
    sgr_mouse_modifier_state, ss3_virtual_key, vt_modifier_state,
};

/// Maps a CSI final character to a Windows virtual-key value.
/// Returns zero when the sequence is not one of the supported deterministic keys.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_cursor_vkey(final_character: u16) -> u16 {
    cursor_virtual_key(final_character).unwrap_or(0)
}

/// Maps a CSI `~` generic-key identifier to a Windows virtual-key value.
/// Returns zero when the identifier is not mapped.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_generic_vkey(identifier: i32) -> u16 {
    generic_virtual_key(identifier).unwrap_or(0)
}

/// Maps an SS3 final character to a Windows virtual-key value.
/// Returns zero when the sequence is not one of the supported deterministic keys.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_ss3_vkey(final_character: u16) -> u16 {
    ss3_virtual_key(final_character).unwrap_or(0)
}

/// Converts a VT encoded modifier parameter into Windows input modifier flags.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_vt_modifier_state(modifier_parameter: u32) -> u32 {
    vt_modifier_state(modifier_parameter)
}

/// Composes VT modifiers with the Windows enhanced-key flag for CSI cursor keys.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_cursor_modifier_state(
    final_character: u16,
    modifier_parameter: u32,
) -> u32 {
    cursor_modifier_state(final_character, modifier_parameter)
}

/// Composes VT modifiers with the Windows enhanced-key flag for generic CSI keys.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_generic_modifier_state(
    identifier: i32,
    modifier_parameter: u32,
) -> u32 {
    generic_modifier_state(identifier, modifier_parameter)
}

/// Extracts Windows input modifier flags from an SGR mouse encoding.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_input_sgr_mouse_modifier_state(encoding: u32) -> u32 {
    sgr_mouse_modifier_state(encoding)
}

#[cfg(test)]
mod tests {
    use super::{
        terminal_parser_ffi_input_cursor_modifier_state, terminal_parser_ffi_input_cursor_vkey,
        terminal_parser_ffi_input_generic_modifier_state, terminal_parser_ffi_input_generic_vkey,
        terminal_parser_ffi_input_sgr_mouse_modifier_state, terminal_parser_ffi_input_ss3_vkey,
        terminal_parser_ffi_input_vt_modifier_state,
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
    fn input_keymap_ffi_replays_every_cpp_mapping_and_preserves_zero_as_not_found() {
        for (final_character, vkey) in CURSOR_AND_SS3_CASES {
            assert_eq!(
                terminal_parser_ffi_input_cursor_vkey(u16::from(final_character)),
                vkey
            );
            assert_eq!(
                terminal_parser_ffi_input_ss3_vkey(u16::from(final_character)),
                vkey
            );
        }

        for (identifier, vkey) in GENERIC_CASES {
            assert_eq!(terminal_parser_ffi_input_generic_vkey(identifier), vkey);
        }

        assert_eq!(terminal_parser_ffi_input_cursor_vkey(u16::from(b'X')), 0);
        assert_eq!(terminal_parser_ffi_input_ss3_vkey(u16::from(b'X')), 0);
        for unmapped in [0, 7, 14, 16, 22, 25, i32::MAX] {
            assert_eq!(terminal_parser_ffi_input_generic_vkey(unmapped), 0);
        }
    }

    #[test]
    fn input_modifier_ffi_replays_cpp_bit_contracts() {
        let vt_cases = [
            (1, 0),
            (2, 0x0010),
            (3, 0x0002),
            (4, 0x0012),
            (5, 0x0008),
            (8, 0x001a),
        ];
        for (parameter, expected) in vt_cases {
            assert_eq!(terminal_parser_ffi_input_vt_modifier_state(parameter), expected);
        }

        let sgr_cases = [
            (0, 0),
            (4, 0x0010),
            (8, 0x0002),
            (16, 0x0008),
            (28, 0x001a),
            (60, 0x001a),
        ];
        for (encoding, expected) in sgr_cases {
            assert_eq!(
                terminal_parser_ffi_input_sgr_mouse_modifier_state(encoding),
                expected
            );
        }
    }

    #[test]
    fn input_key_modifier_ffi_replays_cpp_enhanced_key_contracts() {
        assert_eq!(
            terminal_parser_ffi_input_cursor_modifier_state(u16::from(b'A'), 1),
            0x0100
        );
        assert_eq!(
            terminal_parser_ffi_input_cursor_modifier_state(u16::from(b'H'), 8),
            0x011a
        );
        assert_eq!(
            terminal_parser_ffi_input_cursor_modifier_state(u16::from(b'P'), 1),
            0
        );
        assert_eq!(
            terminal_parser_ffi_input_cursor_modifier_state(u16::from(b'S'), 8),
            0x001a
        );

        assert_eq!(terminal_parser_ffi_input_generic_modifier_state(1, 1), 0x0100);
        assert_eq!(terminal_parser_ffi_input_generic_modifier_state(6, 8), 0x011a);
        assert_eq!(terminal_parser_ffi_input_generic_modifier_state(15, 1), 0);
    }
}
