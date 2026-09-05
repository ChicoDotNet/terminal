#pragma once

#include "terminal_parser_ffi_output_csi_displayed_extent.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_displayed_extent_id()
    {
        return static_cast<uint64_t>(static_cast<unsigned char>('"')) |
               (static_cast<uint64_t>(static_cast<unsigned char>('v')) << 8);
    }

    inline bool output_csi_displayed_extent_replay()
    {
        terminal_parser_ffi_output_csi_displayed_extent_result request{};
        const auto requestStatus = terminal_parser_ffi_output_csi_displayed_extent_plan(
            packed_csi_displayed_extent_id(), &request);
        if (requestStatus != TERMINAL_PARSER_FFI_OK ||
            request.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_REQUEST)
        {
            std::fprintf(
                stderr,
                "output CSI displayed extent mismatch: status=%u kind=%u\n",
                static_cast<unsigned>(requestStatus),
                request.kind);
            return false;
        }

        terminal_parser_ffi_output_csi_displayed_extent_result unrelated{};
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_displayed_extent_plan(
            static_cast<uint64_t>(static_cast<unsigned char>('m')),
            &unrelated);
        if (unrelatedStatus != TERMINAL_PARSER_FFI_OK ||
            unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_NONE)
        {
            std::fprintf(
                stderr,
                "output CSI displayed extent unrelated mismatch: status=%u kind=%u\n",
                static_cast<unsigned>(unrelatedStatus),
                unrelated.kind);
            return false;
        }

        return true;
    }
}
