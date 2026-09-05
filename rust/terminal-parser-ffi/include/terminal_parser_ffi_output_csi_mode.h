#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_mode_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_MODE_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_MODE_MODE = 1,
} terminal_parser_ffi_output_csi_mode_kind;

typedef struct terminal_parser_ffi_output_csi_mode_result
{
    uint32_t kind;
    uint32_t private_mode;
    uint32_t enabled;
    int32_t mode;
} terminal_parser_ffi_output_csi_mode_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_mode_plan(
    uint64_t identifier,
    int32_t mode,
    terminal_parser_ffi_output_csi_mode_result* out_plan);

terminal_parser_ffi_status terminal_parser_ffi_output_csi_mode_plans(
    uint64_t identifier,
    const int32_t* modes,
    size_t mode_count,
    terminal_parser_ffi_output_csi_mode_result* out_plans,
    size_t output_capacity,
    size_t* out_count);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_mode_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_csi_mode_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_mode_result, private_mode) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_mode_result, enabled) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_mode_result, mode) == 12);
#endif
