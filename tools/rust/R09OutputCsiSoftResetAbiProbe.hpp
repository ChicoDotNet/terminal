#pragma once

#include "terminal_parser_ffi_output_csi_soft_reset.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_soft_reset_id(const char finalCharacter)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>('!')) |
               (static_cast<uint64_t>(static_cast<unsigned char>(finalCharacter)) << 8);
    }

    inline bool expect_output_csi_soft_reset_plan(
        const char finalCharacter,
        const uint32_t expectedKind)
    {
        terminal_parser_ffi_output_csi_soft_reset_result plan{};
        const auto status = terminal_parser_ffi_output_csi_soft_reset_plan(
            packed_csi_soft_reset_id(finalCharacter), &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(
                stderr,
                "output CSI soft reset status %u for CSI !%c\n",
                static_cast<unsigned>(status),
                finalCharacter);
            return false;
        }

        if (plan.kind != expectedKind)
        {
            std::fprintf(
                stderr,
                "output CSI soft reset mismatch for CSI !%c: kind=%u\n",
                finalCharacter,
                plan.kind);
            return false;
        }

        return true;
    }

    inline bool output_csi_soft_reset_replay()
    {
        terminal_parser_ffi_output_csi_soft_reset_result unrelated{};
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_soft_reset_plan(
            static_cast<uint64_t>(static_cast<unsigned char>('m')),
            &unrelated);

        return
            expect_output_csi_soft_reset_plan('p', TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_SOFT_RESET) &&
            unrelatedStatus == TERMINAL_PARSER_FFI_OK &&
            unrelated.kind == TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_NONE;
    }
}
