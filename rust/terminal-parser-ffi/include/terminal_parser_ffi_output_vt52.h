#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_vt52_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_VT52_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_UP = 1,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_DOWN = 2,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_FORWARD = 3,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_BACKWARD = 4,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_DESIGNATE_94_CHARSET = 5,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_POSITION = 6,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_REVERSE_LINE_FEED = 7,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_ERASE_IN_DISPLAY = 8,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_ERASE_IN_LINE = 9,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_DEVICE_ATTRIBUTES = 10,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_SET_KEYPAD_MODE = 11,
    TERMINAL_PARSER_FFI_OUTPUT_VT52_SET_ANSI_MODE = 12,
} terminal_parser_ffi_output_vt52_kind;

typedef struct terminal_parser_ffi_output_vt52_result
{
    uint32_t kind;
    int32_t argument1;
    int32_t argument2;
    uint32_t reserved;
    uint64_t payload;
} terminal_parser_ffi_output_vt52_result;

terminal_parser_ffi_status terminal_parser_ffi_output_vt52_plan(
    uint64_t identifier,
    int32_t parameter0,
    int32_t parameter1,
    terminal_parser_ffi_output_vt52_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_vt52_result) == 24);
static_assert(offsetof(terminal_parser_ffi_output_vt52_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_vt52_result, argument1) == 4);
static_assert(offsetof(terminal_parser_ffi_output_vt52_result, argument2) == 8);
static_assert(offsetof(terminal_parser_ffi_output_vt52_result, reserved) == 12);
static_assert(offsetof(terminal_parser_ffi_output_vt52_result, payload) == 16);
#endif
