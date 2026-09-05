#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_esc_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_ESC_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_BACK_INDEX = 1,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_CURSOR_SAVE_STATE = 2,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_CURSOR_RESTORE_STATE = 3,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_FORWARD_INDEX = 4,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_KEYPAD_MODE = 5,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_LINE_FEED_WITH_RETURN = 6,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_LINE_FEED_WITHOUT_RETURN = 7,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_REVERSE_LINE_FEED = 8,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_HORIZONTAL_TAB_SET = 9,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_DEVICE_ATTRIBUTES_PRIMARY = 10,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_HARD_RESET = 11,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_SINGLE_SHIFT = 12,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT = 13,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT_RIGHT = 14,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_ACCEPT_C1_CONTROLS = 15,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_SEND_C1_CONTROLS = 16,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_ANNOUNCE_CODE_STRUCTURE = 17,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_LINE_RENDITION = 18,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_SCREEN_ALIGNMENT_PATTERN = 19,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_CODING_SYSTEM = 20,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_94_CHARSET = 21,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_96_CHARSET = 22,
} terminal_parser_ffi_output_esc_kind;

typedef enum terminal_parser_ffi_output_esc_line_rendition
{
    TERMINAL_PARSER_FFI_OUTPUT_ESC_SINGLE_WIDTH = 0,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_WIDTH = 1,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_HEIGHT_TOP = 2,
    TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_HEIGHT_BOTTOM = 3,
} terminal_parser_ffi_output_esc_line_rendition;

typedef struct terminal_parser_ffi_output_esc_result
{
    uint32_t kind;
    uint32_t argument;
    uint64_t payload;
} terminal_parser_ffi_output_esc_result;

terminal_parser_ffi_status terminal_parser_ffi_output_esc_plan(
    uint64_t identifier,
    terminal_parser_ffi_output_esc_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_esc_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_esc_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_esc_result, argument) == 4);
static_assert(offsetof(terminal_parser_ffi_output_esc_result, payload) == 8);
#endif
