#pragma once

#include "terminal_parser_ffi_output_csi_page_position.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_page_position_id(const char finalCharacter)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(' ')) |
               (static_cast<uint64_t>(static_cast<unsigned char>(finalCharacter)) << 8);
    }

    inline bool expect_output_csi_page_position_plan(
        const char finalCharacter,
        const int32_t parameter0,
        const uint32_t expectedKind,
        const int32_t expectedCount)
    {
        terminal_parser_ffi_output_csi_page_position_result plan{};
        const auto status = terminal_parser_ffi_output_csi_page_position_plan(
            packed_csi_page_position_id(finalCharacter), parameter0, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(
                stderr,
                "output CSI page position status %u for CSI SP %c\n",
                static_cast<unsigned>(status),
                finalCharacter);
            return false;
        }

        if (plan.kind != expectedKind || plan.count != expectedCount)
        {
            std::fprintf(
                stderr,
                "output CSI page position mismatch for CSI SP %c: kind=%u count=%d\n",
                finalCharacter,
                plan.kind,
                plan.count);
            return false;
        }

        return true;
    }

    inline bool output_csi_page_position_replay()
    {
        terminal_parser_ffi_output_csi_page_position_result unrelated{};
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_page_position_plan(
            static_cast<uint64_t>(static_cast<unsigned char>('m')),
            3,
            &unrelated);

        return
            expect_output_csi_page_position_plan('P', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_ABSOLUTE, 1) &&
            expect_output_csi_page_position_plan('P', 4, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_ABSOLUTE, 4) &&
            expect_output_csi_page_position_plan('Q', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_RELATIVE, 1) &&
            expect_output_csi_page_position_plan('Q', 5, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_RELATIVE, 5) &&
            expect_output_csi_page_position_plan('R', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_BACK, 1) &&
            expect_output_csi_page_position_plan('R', 6, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_BACK, 6) &&
            unrelatedStatus == TERMINAL_PARSER_FFI_OK &&
            unrelated.kind == TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_NONE &&
            unrelated.count == 0;
    }
}
