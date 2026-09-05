#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_tab_control_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_CLEAR = 1,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_SET = 2,
} terminal_parser_ffi_output_csi_tab_control_kind;

typedef struct terminal_parser_ffi_output_csi_tab_control_result
{
    uint32_t kind;
    int32_t value;
} terminal_parser_ffi_output_csi_tab_control_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_tab_control_plans(
    uint64_t identifier,
    const int32_t* values,
    size_t value_count,
    terminal_parser_ffi_output_csi_tab_control_result* out_plans,
    size_t output_capacity,
    size_t* out_count);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_tab_control_result) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_tab_control_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_tab_control_result, value) == 4);
#endif
