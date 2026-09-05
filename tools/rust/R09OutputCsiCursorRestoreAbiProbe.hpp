#pragma once

#include "terminal_parser_ffi_output_csi_cursor_restore.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline bool expect_output_csi_cursor_restore_plan(
        const char finalCharacter,
        const uint32_t expectedKind)
    {
        terminal_parser_ffi_output_csi_cursor_restore_result plan{};
        const auto status = terminal_parser_ffi_output_csi_cursor_restore_plan(
            static_cast<uint64_t>(static_cast<unsigned char>(finalCharacter)), &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(
                stderr,
                "output CSI cursor restore status %u for CSI %c\n",
                static_cast<unsigned>(status),
                finalCharacter);
            return false;
        }

        if (plan.kind != expectedKind)
        {
            std::fprintf(
                stderr,
                "output CSI cursor restore mismatch for CSI %c: kind=%u\n",
                finalCharacter,
                plan.kind);
            return false;
        }

        return true;
    }

    inline bool output_csi_cursor_restore_replay()
    {
        return
            expect_output_csi_cursor_restore_plan('u', TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_RESTORE) &&
            expect_output_csi_cursor_restore_plan('m', TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_NONE);
    }
}
