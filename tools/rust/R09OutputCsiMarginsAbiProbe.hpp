#pragma once

#include "terminal_parser_ffi_output_csi_margins.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_margins_id(const char character)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(character));
    }

    inline bool expect_output_csi_margins_plan(
        const char id,
        const int32_t parameter0,
        const int32_t parameter1,
        const uint32_t expectedKind,
        const int32_t expectedFirst,
        const int32_t expectedSecond)
    {
        terminal_parser_ffi_output_csi_margins_result plan{};
        const auto status = terminal_parser_ffi_output_csi_margins_plan(
            packed_csi_margins_id(id), parameter0, parameter1, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(stderr, "output CSI margins status %u for CSI %c\n", static_cast<unsigned>(status), id);
            return false;
        }

        if (plan.kind != expectedKind || plan.first != expectedFirst || plan.second != expectedSecond)
        {
            std::fprintf(
                stderr,
                "output CSI margins mismatch for CSI %c: kind=%u first=%d second=%d\n",
                id,
                plan.kind,
                plan.first,
                plan.second);
            return false;
        }

        return true;
    }

    inline bool output_csi_margins_replay()
    {
        return
            expect_output_csi_margins_plan('r', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_TOP_BOTTOM, 0, 0) &&
            expect_output_csi_margins_plan('r', 3, 40, TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_TOP_BOTTOM, 3, 40) &&
            expect_output_csi_margins_plan('s', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_LEFT_RIGHT, 0, 0) &&
            expect_output_csi_margins_plan('s', 5, 70, TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_LEFT_RIGHT, 5, 70) &&
            expect_output_csi_margins_plan('m', 1, 2, TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_NONE, 0, 0);
    }
}
