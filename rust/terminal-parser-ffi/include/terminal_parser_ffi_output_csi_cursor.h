#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_cursor_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_UP = 1,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_DOWN = 2,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_FORWARD = 3,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_BACKWARD = 4,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NEXT_LINE = 5,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_PREVIOUS_LINE = 6,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_HORIZONTAL_ABSOLUTE = 7,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_VERTICAL_ABSOLUTE = 8,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_HORIZONTAL_RELATIVE = 9,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_VERTICAL_RELATIVE = 10,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_POSITION = 11,
} terminal_parser_ffi_output_csi_cursor_kind;

typedef struct terminal_parser_ffi_output_csi_cursor_result
{
    uint32_t kind;
    int32_t argument1;
    int32_t argument2;
    uint32_t reserved;
} terminal_parser_ffi_output_csi_cursor_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_cursor_plan(
    uint64_t identifier,
    int32_t parameter0,
    int32_t parameter1,
    terminal_parser_ffi_output_csi_cursor_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_cursor_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_csi_cursor_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_cursor_result, argument1) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_cursor_result, argument2) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_cursor_result, reserved) == 12);
#endif
