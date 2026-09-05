#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_scroll_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_UP = 1,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_DOWN = 2,
} terminal_parser_ffi_output_csi_scroll_kind;

typedef struct terminal_parser_ffi_output_csi_scroll_result
{
    uint32_t kind;
    int32_t count;
    uint32_t reserved0;
    uint32_t reserved1;
} terminal_parser_ffi_output_csi_scroll_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_scroll_plan(
    uint64_t identifier,
    int32_t parameter0,
    terminal_parser_ffi_output_csi_scroll_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_scroll_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_csi_scroll_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_scroll_result, count) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_scroll_result, reserved0) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_scroll_result, reserved1) == 12);
#endif
