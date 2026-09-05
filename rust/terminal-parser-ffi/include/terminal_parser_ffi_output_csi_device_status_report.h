#pragma once

#include "terminal_parser_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum terminal_parser_ffi_output_csi_device_status_report_kind
{
    TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_NONE = 0,
    TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_REPORT = 1,
} terminal_parser_ffi_output_csi_device_status_report_kind;

typedef struct terminal_parser_ffi_output_csi_device_status_report_result
{
    uint32_t kind;
    uint32_t private_mode;
    int32_t status;
    uint32_t has_id;
    int32_t id;
} terminal_parser_ffi_output_csi_device_status_report_result;

terminal_parser_ffi_status terminal_parser_ffi_output_csi_device_status_report_plan(
    uint64_t identifier,
    int32_t parameter0,
    uint32_t has_parameter1,
    int32_t parameter1,
    terminal_parser_ffi_output_csi_device_status_report_result* out_plan);

#ifdef __cplusplus
}

static_assert(sizeof(terminal_parser_ffi_output_csi_device_status_report_result) == 20);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_status_report_result, kind) == 0);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_status_report_result, private_mode) == 4);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_status_report_result, status) == 8);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_status_report_result, has_id) == 12);
static_assert(offsetof(terminal_parser_ffi_output_csi_device_status_report_result, id) == 16);
#endif
