#pragma once

#include "terminal_parser_ffi_output_vt52.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_vt52_id(const char character)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(character));
    }

    inline bool expect_output_vt52_plan(
        const char id,
        const int32_t parameter0,
        const int32_t parameter1,
        const uint32_t expectedKind,
        const int32_t expectedArgument1,
        const int32_t expectedArgument2,
        const uint64_t expectedPayload)
    {
        terminal_parser_ffi_output_vt52_result plan{};
        const auto status = terminal_parser_ffi_output_vt52_plan(
            packed_vt52_id(id), parameter0, parameter1, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(stderr, "output VT52 status %u for ESC %c\n", static_cast<unsigned>(status), id);
            return false;
        }

        if (plan.kind != expectedKind ||
            plan.argument1 != expectedArgument1 ||
            plan.argument2 != expectedArgument2 ||
            plan.payload != expectedPayload)
        {
            std::fprintf(
                stderr,
                "output VT52 mismatch for ESC %c: kind=%u arg1=%d arg2=%d payload=%llu\n",
                id,
                plan.kind,
                plan.argument1,
                plan.argument2,
                static_cast<unsigned long long>(plan.payload));
            return false;
        }

        return true;
    }

    inline bool output_vt52_replay()
    {
        constexpr int32_t space = 0x20;
        return
            expect_output_vt52_plan('A', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_UP, 1, 0, 0) &&
            expect_output_vt52_plan('B', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_DOWN, 1, 0, 0) &&
            expect_output_vt52_plan('C', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_FORWARD, 1, 0, 0) &&
            expect_output_vt52_plan('D', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_BACKWARD, 1, 0, 0) &&
            expect_output_vt52_plan('F', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_DESIGNATE_94_CHARSET, 0, 0, '0') &&
            expect_output_vt52_plan('G', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_DESIGNATE_94_CHARSET, 0, 0, 'B') &&
            expect_output_vt52_plan('H', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_POSITION, 1, 1, 0) &&
            expect_output_vt52_plan('I', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_REVERSE_LINE_FEED, 0, 0, 0) &&
            expect_output_vt52_plan('J', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_ERASE_IN_DISPLAY, 0, 0, 0) &&
            expect_output_vt52_plan('K', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_ERASE_IN_LINE, 0, 0, 0) &&
            expect_output_vt52_plan('Y', space + 4, space + 9, TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_POSITION, 5, 10, 0) &&
            expect_output_vt52_plan('Z', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_DEVICE_ATTRIBUTES, 0, 0, 0) &&
            expect_output_vt52_plan('=', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_SET_KEYPAD_MODE, 1, 0, 0) &&
            expect_output_vt52_plan('>', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_SET_KEYPAD_MODE, 0, 0, 0) &&
            expect_output_vt52_plan('<', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_SET_ANSI_MODE, 0, 0, 0) &&
            expect_output_vt52_plan('?', 0, 0, TERMINAL_PARSER_FFI_OUTPUT_VT52_NONE, 0, 0, 0);
    }
}
