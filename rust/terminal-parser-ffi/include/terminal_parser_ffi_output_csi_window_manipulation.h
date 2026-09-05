#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_window_manipulation_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_WINDOW_MANIPULATION_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_WINDOW_MANIPULATION = 1,
} terminal_parser_ffi_output_csi_window_manipulation_kind;

typedef struct terminal_parser_ffi_output_csi_window_manipulation_result
{
    uint32_t kind;
    int32_t function;
    int32_t parameter1;
    int32_t parameter2;
} terminal_parser_ffi_output_csi_window_manipulation_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_window_manipulation_plan(
    uint64_t identifier,
    int32_t parameter0,
    int32_t parameter1,
    int32_t parameter2,
    terminal_parser_ffi_output_csi_window_manipulation_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_window_manipulation_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_csi_window_manipulation_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_window_manipulation_result, function) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_window_manipulation_result, parameter1) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_window_manipulation_result, parameter2) == 12);
#endif
