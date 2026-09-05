#pragma once

#include "terminal_parser_ffi_output_csi_cursor.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_cursor_id(const char character)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(character));
    }

    inline bool expect_output_csi_cursor_plan(
        const char id,
        const int32_t parameter0,
        const int32_t parameter1,
        const uint32_t expectedKind,
        const int32_t expectedArgument1,
        const int32_t expectedArgument2)
    {
        terminal_parser_ffi_output_csi_cursor_result plan{};
        const auto status = terminal_parser_ffi_output_csi_cursor_plan(
            packed_csi_cursor_id(id), parameter0, parameter1, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(stderr, "output CSI cursor status %u for CSI %c\n", static_cast<unsigned>(status), id);
            return false;
        }

        if (plan.kind != expectedKind ||
            plan.argument1 != expectedArgument1 ||
            plan.argument2 != expectedArgument2)
        {
            std::fprintf(
                stderr,
                "output CSI cursor mismatch for CSI %c: kind=%u arg1=%d arg2=%d\n",
                id,
                plan.kind,
                plan.argument1,
                plan.argument2);
            return false;
        }

        return true;
    }

    inline bool output_csi_cursor_replay()
    {
        return
            expect_output_csi_cursor_plan('A', 4, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_UP, 4, 0) &&
            expect_output_csi_cursor_plan('B', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_DOWN, 1, 0) &&
            expect_output_csi_cursor_plan('C', 2, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_FORWARD, 2, 0) &&
            expect_output_csi_cursor_plan('D', 3, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_BACKWARD, 3, 0) &&
            expect_output_csi_cursor_plan('E', 5, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NEXT_LINE, 5, 0) &&
            expect_output_csi_cursor_plan('F', 6, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_PREVIOUS_LINE, 6, 0) &&
            expect_output_csi_cursor_plan('G', 7, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_HORIZONTAL_ABSOLUTE, 7, 0) &&
            expect_output_csi_cursor_plan('`', 8, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_HORIZONTAL_ABSOLUTE, 8, 0) &&
            expect_output_csi_cursor_plan('d', 9, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_VERTICAL_ABSOLUTE, 9, 0) &&
            expect_output_csi_cursor_plan('a', 10, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_HORIZONTAL_RELATIVE, 10, 0) &&
            expect_output_csi_cursor_plan('e', 11, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_VERTICAL_RELATIVE, 11, 0) &&
            expect_output_csi_cursor_plan('H', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_POSITION, 1, 1) &&
            expect_output_csi_cursor_plan('f', 14, 15, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_POSITION, 14, 15) &&
            expect_output_csi_cursor_plan('m', 1, 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NONE, 0, 0);
    }
}
