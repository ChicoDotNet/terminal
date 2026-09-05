#pragma once

#include "terminal_parser_ffi_output_csi_erase_characters.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_erase_characters_id(const char character)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(character));
    }

    inline bool expect_output_csi_erase_characters_plan(
        const char id,
        const int32_t parameter0,
        const uint32_t expectedKind,
        const int32_t expectedCount)
    {
        terminal_parser_ffi_output_csi_erase_characters_result plan{};
        const auto status = terminal_parser_ffi_output_csi_erase_characters_plan(
            packed_csi_erase_characters_id(id), parameter0, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(stderr, "output CSI erase characters status %u for CSI %c\n", static_cast<unsigned>(status), id);
            return false;
        }

        if (plan.kind != expectedKind || plan.count != expectedCount)
        {
            std::fprintf(
                stderr,
                "output CSI erase characters mismatch for CSI %c: kind=%u count=%d\n",
                id,
                plan.kind,
                plan.count);
            return false;
        }

        return true;
    }

    inline bool output_csi_erase_characters_replay()
    {
        return
            expect_output_csi_erase_characters_plan('X', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_CHARACTERS_ERASE_CHARACTERS, 1) &&
            expect_output_csi_erase_characters_plan('X', 5, TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_CHARACTERS_ERASE_CHARACTERS, 5) &&
            expect_output_csi_erase_characters_plan('m', 3, TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_CHARACTERS_NONE, 0);
    }
}