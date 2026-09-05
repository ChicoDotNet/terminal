#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_request_mode_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_REQUEST_MODE = 1,
} terminal_parser_ffi_output_csi_request_mode_kind;

typedef struct terminal_parser_ffi_output_csi_request_mode_result
{
    uint32_t kind;
    uint32_t private_mode;
    int32_t mode;
    uint32_t reserved;
} terminal_parser_ffi_output_csi_request_mode_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_request_mode_plan(
    uint64_t identifier,
    int32_t parameter0,
    terminal_parser_ffi_output_csi_request_mode_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_request_mode_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_csi_request_mode_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_request_mode_result, private_mode) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_request_mode_result, mode) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_request_mode_result, reserved) == 12);
#endif
