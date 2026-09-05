#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_status
{
    TERMINAL_PARSER_FFI_OK = 0,
    TERMINAL_PARSER_FFI_INVALID_ARGUMENT = 1,
    TERMINAL_PARSER_FFI_INVALID_BASE64 = 2,
    TERMINAL_PARSER_FFI_INVALID_UTF8 = 3,
    TERMINAL_PARSER_FFI_BUFFER_TOO_SMALL = 4,
    TERMINAL_PARSER_FFI_PANIC = 255,
} terminal_parser_ffi_status;

typedef enum terminal_parser_ffi_control_character_kind
{
    TERMINAL_PARSER_FFI_CONTROL_CHARACTER_PRINT = 0,
    TERMINAL_PARSER_FFI_CONTROL_CHARACTER_CTRL_C = 1,
    TERMINAL_PARSER_FFI_CONTROL_CHARACTER_MAPPED_C0 = 2,
    TERMINAL_PARSER_FFI_CONTROL_CHARACTER_DELETE_AS_BACKSPACE = 3,
} terminal_parser_ffi_control_character_kind;

typedef enum terminal_parser_ffi_output_execute_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_ENQUIRE_ANSWERBACK = 1,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_WARNING_BELL = 2,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_CURSOR_BACKWARD = 3,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_FORWARD_TAB = 4,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_CARRIAGE_RETURN = 5,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_LINE_FEED_DEPENDS_ON_MODE = 6,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_LOCKING_SHIFT = 7,
    TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_PRINT = 8,
} terminal_parser_ffi_output_execute_kind;

typedef struct terminal_parser_ffi_key_event
{
    uint32_t key_down;
    uint16_t repeat_count;
    uint16_t virtual_key;
    uint16_t scan_code;
    uint16_t unicode_char;
    uint32_t control_key_state;
} terminal_parser_ffi_key_event;

typedef struct terminal_parser_ffi_control_character_plan
{
    uint32_t kind;
    uint16_t character;
    uint16_t forced_virtual_key;
    uint32_t write_ctrl;
    uint32_t clear_layout_modifiers;
} terminal_parser_ffi_control_character_plan;

typedef struct terminal_parser_ffi_sgr_mouse_plan
{
    uint32_t valid;
    uint32_t button_id;
    uint32_t button_state;
    uint32_t persistent_button_state;
    uint32_t event_flags;
    uint32_t track_click;
} terminal_parser_ffi_sgr_mouse_plan;

typedef struct terminal_parser_ffi_output_execute_result
{
    uint32_t kind;
    uint32_t argument;
} terminal_parser_ffi_output_execute_result;

uint32_t terminal_parser_ffi_abi_version(void);
terminal_parser_ffi_status terminal_parser_ffi_status_probe(void);
terminal_parser_ffi_status terminal_parser_ffi_base64_decode_utf16(
    const uint16_t* input,
    size_t input_len,
    uint16_t* output,
    size_t output_capacity,
    size_t* out_len);

uint16_t terminal_parser_ffi_input_cursor_vkey(uint16_t final_character);
uint16_t terminal_parser_ffi_input_generic_vkey(int32_t identifier);
uint16_t terminal_parser_ffi_input_ss3_vkey(uint16_t final_character);
uint32_t terminal_parser_ffi_input_vt_modifier_state(uint32_t modifier_parameter);
uint32_t terminal_parser_ffi_input_cursor_modifier_state(uint16_t final_character, uint32_t modifier_parameter);
uint32_t terminal_parser_ffi_input_generic_modifier_state(int32_t identifier, uint32_t modifier_parameter);
uint32_t terminal_parser_ffi_input_sgr_mouse_modifier_state(uint32_t encoding);
terminal_parser_ffi_status terminal_parser_ffi_input_control_character_plan(
    uint16_t code_unit,
    uint32_t write_alt,
    terminal_parser_ffi_control_character_plan* out_plan);
terminal_parser_ffi_status terminal_parser_ffi_input_sgr_mouse_plan(
    uint32_t previous_button_state,
    uint32_t encoding,
    uint32_t button_down,
    terminal_parser_ffi_sgr_mouse_plan* out_plan);
terminal_parser_ffi_status terminal_parser_ffi_input_win32_key_fields(
    uint32_t present_mask,
    int32_t virtual_key,
    int32_t scan_code,
    int32_t unicode_char,
    int32_t key_down,
    int32_t control_key_state,
    int32_t repeat_count,
    terminal_parser_ffi_key_event* out_key);
terminal_parser_ffi_status terminal_parser_ffi_output_execute_plan(
    uint16_t code_unit,
    terminal_parser_ffi_output_execute_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_control_character_plan) == 16);
static_assert(offsetof(terminal_parser_ffi_control_character_plan, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_control_character_plan, character) == 4);
static_assert(offsetof(terminal_parser_ffi_control_character_plan, forced_virtual_key) == 6);
static_assert(offsetof(terminal_parser_ffi_control_character_plan, write_ctrl) == 8);
static_assert(offsetof(terminal_parser_ffi_control_character_plan, clear_layout_modifiers) == 12);

static_assert(sizeof(terminal_parser_ffi_sgr_mouse_plan) == 24);
static_assert(offsetof(terminal_parser_ffi_sgr_mouse_plan, valid) == 0);
static_assert(offsetof(terminal_parser_ffi_sgr_mouse_plan, button_id) == 4);
static_assert(offsetof(terminal_parser_ffi_sgr_mouse_plan, button_state) == 8);
static_assert(offsetof(terminal_parser_ffi_sgr_mouse_plan, persistent_button_state) == 12);
static_assert(offsetof(terminal_parser_ffi_sgr_mouse_plan, event_flags) == 16);
static_assert(offsetof(terminal_parser_ffi_sgr_mouse_plan, track_click) == 20);

static_assert(sizeof(terminal_parser_ffi_output_execute_result) == 8);
static_assert(offsetof(terminal_parser_ffi_output_execute_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_execute_result, argument) == 4);
#endif
