#pragma once

#include "terminal_parser_ffi_output_csi_page.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_page_id(const char character)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(character));
    }

    inline bool expect_output_csi_page_plan(
        const char id,
        const int32_t parameter0,
        const uint32_t expectedKind,
        const int32_t expectedCount)
    {
        terminal_parser_ffi_output_csi_page_result plan{};
        const auto status = terminal_parser_ffi_output_csi_page_plan(
            packed_csi_page_id(id), parameter0, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(stderr, "output CSI page status %u for CSI %c\n", static_cast<unsigned>(status), id);
            return false;
        }

        if (plan.kind != expectedKind || plan.count != expectedCount)
        {
            std::fprintf(
                stderr,
                "output CSI page mismatch for CSI %c: kind=%u count=%d\n",
                id,
                plan.kind,
                plan.count);
            return false;
        }

        return true;
    }

    inline bool output_csi_page_replay()
    {
        return
            expect_output_csi_page_plan('U', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_NEXT, 1) &&
            expect_output_csi_page_plan('U', 4, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_NEXT, 4) &&
            expect_output_csi_page_plan('V', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_PRECEDING, 1) &&
            expect_output_csi_page_plan('V', 7, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_PRECEDING, 7) &&
            expect_output_csi_page_plan('m', 3, TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_NONE, 0);
    }
}