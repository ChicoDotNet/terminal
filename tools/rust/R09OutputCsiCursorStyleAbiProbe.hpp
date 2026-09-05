#pragma once

#include "terminal_parser_ffi_output_csi_cursor_style.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_cursor_style_id()
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(' ')) |
               (static_cast<uint64_t>(static_cast<unsigned char>('q')) << 8);
    }

    inline bool output_csi_cursor_style_replay()
    {
        terminal_parser_ffi_output_csi_cursor_style_result defaultStyle{};
        const auto defaultStatus = terminal_parser_ffi_output_csi_cursor_style_plan(
            packed_csi_cursor_style_id(), 0, &defaultStyle);
        if (defaultStatus != TERMINAL_PARSER_FFI_OK ||
            defaultStyle.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_SET_CURSOR_STYLE ||
            defaultStyle.style != 0)
        {
            std::fprintf(stderr, "output CSI cursor style default mismatch: status=%u kind=%u style=%d\n", static_cast<unsigned>(defaultStatus), defaultStyle.kind, defaultStyle.style);
            return false;
        }

        terminal_parser_ffi_output_csi_cursor_style_result explicitStyle{};
        const auto explicitStatus = terminal_parser_ffi_output_csi_cursor_style_plan(
            packed_csi_cursor_style_id(), 6, &explicitStyle);
        if (explicitStatus != TERMINAL_PARSER_FFI_OK ||
            explicitStyle.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_SET_CURSOR_STYLE ||
            explicitStyle.style != 6)
        {
            std::fprintf(stderr, "output CSI cursor style explicit mismatch: status=%u kind=%u style=%d\n", static_cast<unsigned>(explicitStatus), explicitStyle.kind, explicitStyle.style);
            return false;
        }

        terminal_parser_ffi_output_csi_cursor_style_result unrelated{};
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_cursor_style_plan(
            static_cast<uint64_t>(static_cast<unsigned char>('m')), 3, &unrelated);
        if (unrelatedStatus != TERMINAL_PARSER_FFI_OK ||
            unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_NONE)
        {
            std::fprintf(stderr, "output CSI cursor style unrelated mismatch: status=%u kind=%u\n", static_cast<unsigned>(unrelatedStatus), unrelated.kind);
            return false;
        }

        return true;
    }
}
