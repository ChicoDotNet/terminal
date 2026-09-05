#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct terminal_app_ffi_fzf_pattern terminal_app_ffi_fzf_pattern;

typedef struct terminal_app_ffi_fzf_run
{
    size_t start;
    size_t end;
} terminal_app_ffi_fzf_run;

enum terminal_app_ffi_status
{
    TERMINAL_APP_FFI_OK = 0,
    TERMINAL_APP_FFI_INVALID_ARGUMENT = 1,
    TERMINAL_APP_FFI_INVALID_UTF16 = 2,
    TERMINAL_APP_FFI_BUFFER_TOO_SMALL = 3,
    TERMINAL_APP_FFI_PANIC = 255,
};

uint32_t terminal_app_ffi_abi_version(void);

int32_t terminal_app_ffi_fzf_pattern_create_utf16(
    const uint16_t* input,
    size_t input_len,
    terminal_app_ffi_fzf_pattern** out_pattern);

int32_t terminal_app_ffi_fzf_pattern_is_empty(
    const terminal_app_ffi_fzf_pattern* pattern,
    uint8_t* out_is_empty);

int32_t terminal_app_ffi_fzf_pattern_destroy(terminal_app_ffi_fzf_pattern* pattern);

int32_t terminal_app_ffi_fzf_match_utf16(
    const terminal_app_ffi_fzf_pattern* pattern,
    const uint16_t* text,
    size_t text_len,
    int32_t* out_score,
    uint8_t* out_matched,
    terminal_app_ffi_fzf_run* out_runs,
    size_t runs_capacity,
    size_t* out_runs_len);

#ifdef __cplusplus
}
#endif
