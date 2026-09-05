#pragma once

#include "terminal_parser_ffi_output_esc.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t pack_esc_sequence(const char* text)
    {
        uint64_t value = 0;
        for (uint32_t index = 0; index < 7 && text[index] != '\0'; ++index)
        {
            value |= static_cast<uint64_t>(static_cast<uint8_t>(text[index])) << (index * 8);
        }
        return value;
    }

    inline bool expect_output_esc_plan(
        const char* id,
        const uint32_t expectedKind,
        const uint32_t expectedArgument = 0,
        const uint64_t expectedPayload = 0)
    {
        terminal_parser_ffi_output_esc_result plan{};
        const auto status = terminal_parser_ffi_output_esc_plan(pack_esc_sequence(id), &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(stderr, "output ESC status %u for %s\n", static_cast<unsigned>(status), id);
            return false;
        }
        if (plan.kind != expectedKind || plan.argument != expectedArgument || plan.payload != expectedPayload)
        {
            std::fprintf(
                stderr,
                "output ESC mismatch for %s: kind=%u argument=%u payload=%llu\n",
                id,
                plan.kind,
                plan.argument,
                static_cast<unsigned long long>(plan.payload));
            return false;
        }
        return true;
    }

    inline bool output_esc_replay()
    {
        return
            expect_output_esc_plan("\\", TERMINAL_PARSER_FFI_OUTPUT_ESC_NONE) &&
            expect_output_esc_plan("6", TERMINAL_PARSER_FFI_OUTPUT_ESC_BACK_INDEX) &&
            expect_output_esc_plan("7", TERMINAL_PARSER_FFI_OUTPUT_ESC_CURSOR_SAVE_STATE) &&
            expect_output_esc_plan("8", TERMINAL_PARSER_FFI_OUTPUT_ESC_CURSOR_RESTORE_STATE) &&
            expect_output_esc_plan("9", TERMINAL_PARSER_FFI_OUTPUT_ESC_FORWARD_INDEX) &&
            expect_output_esc_plan("=", TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_KEYPAD_MODE, 1) &&
            expect_output_esc_plan(">", TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_KEYPAD_MODE, 0) &&
            expect_output_esc_plan("E", TERMINAL_PARSER_FFI_OUTPUT_ESC_LINE_FEED_WITH_RETURN) &&
            expect_output_esc_plan("D", TERMINAL_PARSER_FFI_OUTPUT_ESC_LINE_FEED_WITHOUT_RETURN) &&
            expect_output_esc_plan("M", TERMINAL_PARSER_FFI_OUTPUT_ESC_REVERSE_LINE_FEED) &&
            expect_output_esc_plan("H", TERMINAL_PARSER_FFI_OUTPUT_ESC_HORIZONTAL_TAB_SET) &&
            expect_output_esc_plan("Z", TERMINAL_PARSER_FFI_OUTPUT_ESC_DEVICE_ATTRIBUTES_PRIMARY) &&
            expect_output_esc_plan("c", TERMINAL_PARSER_FFI_OUTPUT_ESC_HARD_RESET) &&
            expect_output_esc_plan("N", TERMINAL_PARSER_FFI_OUTPUT_ESC_SINGLE_SHIFT, 2) &&
            expect_output_esc_plan("O", TERMINAL_PARSER_FFI_OUTPUT_ESC_SINGLE_SHIFT, 3) &&
            expect_output_esc_plan("n", TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT, 2) &&
            expect_output_esc_plan("o", TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT, 3) &&
            expect_output_esc_plan("~", TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT_RIGHT, 1) &&
            expect_output_esc_plan("}", TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT_RIGHT, 2) &&
            expect_output_esc_plan("|", TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT_RIGHT, 3) &&
            expect_output_esc_plan(" 7", TERMINAL_PARSER_FFI_OUTPUT_ESC_ACCEPT_C1_CONTROLS, 1) &&
            expect_output_esc_plan(" F", TERMINAL_PARSER_FFI_OUTPUT_ESC_SEND_C1_CONTROLS, 0) &&
            expect_output_esc_plan(" G", TERMINAL_PARSER_FFI_OUTPUT_ESC_SEND_C1_CONTROLS, 1) &&
            expect_output_esc_plan(" L", TERMINAL_PARSER_FFI_OUTPUT_ESC_ANNOUNCE_CODE_STRUCTURE, 1) &&
            expect_output_esc_plan(" M", TERMINAL_PARSER_FFI_OUTPUT_ESC_ANNOUNCE_CODE_STRUCTURE, 2) &&
            expect_output_esc_plan(" N", TERMINAL_PARSER_FFI_OUTPUT_ESC_ANNOUNCE_CODE_STRUCTURE, 3) &&
            expect_output_esc_plan("#3", TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_LINE_RENDITION, TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_HEIGHT_TOP) &&
            expect_output_esc_plan("#4", TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_LINE_RENDITION, TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_HEIGHT_BOTTOM) &&
            expect_output_esc_plan("#5", TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_LINE_RENDITION, TERMINAL_PARSER_FFI_OUTPUT_ESC_SINGLE_WIDTH) &&
            expect_output_esc_plan("#6", TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_LINE_RENDITION, TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_WIDTH) &&
            expect_output_esc_plan("#8", TERMINAL_PARSER_FFI_OUTPUT_ESC_SCREEN_ALIGNMENT_PATTERN) &&
            expect_output_esc_plan("%G", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_CODING_SYSTEM, 0, 'G') &&
            expect_output_esc_plan("(B", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_94_CHARSET, 0, 'B') &&
            expect_output_esc_plan(")B", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_94_CHARSET, 1, 'B') &&
            expect_output_esc_plan("*B", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_94_CHARSET, 2, 'B') &&
            expect_output_esc_plan("+B", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_94_CHARSET, 3, 'B') &&
            expect_output_esc_plan("-A", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_96_CHARSET, 1, 'A') &&
            expect_output_esc_plan(".A", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_96_CHARSET, 2, 'A') &&
            expect_output_esc_plan("/A", TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_96_CHARSET, 3, 'A');
    }
}
