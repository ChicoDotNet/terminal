#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_margins_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_TOP_BOTTOM = 1,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_LEFT_RIGHT = 2,
} terminal_parser_ffi_output_csi_margins_kind;

typedef struct terminal_parser_ffi_output_csi_margins_result
{
    uint32_t kind;
    int32_t first;
    int32_t second;
    uint32_t reserved;
} terminal_parser_ffi_output_csi_margins_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_margins_plan(
    uint64_t identifier,
    int32_t parameter0,
    int32_t parameter1,
    terminal_parser_ffi_output_csi_margins_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_margins_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_csi_margins_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_margins_result, first) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_margins_result, second) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_margins_result, reserved) == 12);
#endif
