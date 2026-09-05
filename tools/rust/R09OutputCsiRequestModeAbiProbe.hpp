#pragma once

#include "terminal_parser_ffi_output_csi_request_mode.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_request_mode_id()
    {
        return static_cast<uint64_t>(static_cast<unsigned char>('$')) |
               (static_cast<uint64_t>(static_cast<unsigned char>('p')) << 8);
    }

    inline uint64_t packed_csi_private_request_mode_id()
    {
        return static_cast<uint64_t>(static_cast<unsigned char>('?')) |
               (static_cast<uint64_t>(static_cast<unsigned char>('$')) << 8) |
               (static_cast<uint64_t>(static_cast<unsigned char>('p')) << 16);
    }

    inline bool output_csi_request_mode_replay()
    {
        terminal_parser_ffi_output_csi_request_mode_result ansi{};
        const auto ansiStatus = terminal_parser_ffi_output_csi_request_mode_plan(
            packed_csi_request_mode_id(), 4, &ansi);
        if (ansiStatus != TERMINAL_PARSER_FFI_OK ||
            ansi.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_REQUEST_MODE ||
            ansi.private_mode != 0 ||
            ansi.mode != 4)
        {
            std::fprintf(stderr, "output CSI ANSI mode request mismatch: status=%u kind=%u private=%u mode=%d\n", static_cast<unsigned>(ansiStatus), ansi.kind, ansi.private_mode, ansi.mode);
            return false;
        }

        terminal_parser_ffi_output_csi_request_mode_result dec{};
        const auto decStatus = terminal_parser_ffi_output_csi_request_mode_plan(
            packed_csi_private_request_mode_id(), 25, &dec);
        if (decStatus != TERMINAL_PARSER_FFI_OK ||
            dec.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_REQUEST_MODE ||
            dec.private_mode != 1 ||
            dec.mode != 25)
        {
            std::fprintf(stderr, "output CSI DEC mode request mismatch: status=%u kind=%u private=%u mode=%d\n", static_cast<unsigned>(decStatus), dec.kind, dec.private_mode, dec.mode);
            return false;
        }

        terminal_parser_ffi_output_csi_request_mode_result unrelated{};
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_request_mode_plan(
            static_cast<uint64_t>(static_cast<unsigned char>('m')), 3, &unrelated);
        if (unrelatedStatus != TERMINAL_PARSER_FFI_OK ||
            unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_NONE)
        {
            std::fprintf(stderr, "output CSI mode request unrelated mismatch: status=%u kind=%u\n", static_cast<unsigned>(unrelatedStatus), unrelated.kind);
            return false;
        }

        return true;
    }
}
