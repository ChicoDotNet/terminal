#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_device_attributes_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_PRIMARY = 1,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_SECONDARY = 2,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_TERTIARY = 3,
} terminal_parser_ffi_output_csi_device_attributes_kind;

typedef struct terminal_parser_ffi_output_csi_device_attributes_result
{
    uint32_t kind;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
} terminal_parser_ffi_output_csi_device_attributes_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_device_attributes_plan(
    uint64_t identifier,
    int32_t parameter0,
    terminal_parser_ffi_output_csi_device_attributes_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_device_attributes_result) == 16);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_attributes_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_attributes_result, reserved0) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_attributes_result, reserved1) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_attributes_result, reserved2) == 12);
#endif
