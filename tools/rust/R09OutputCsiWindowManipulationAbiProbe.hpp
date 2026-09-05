#pragma once

#include "terminal_parser_ffi_output_csi_window_manipulation.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_window_manipulation_id()
    {
        return static_cast<uint64_t>(static_cast<unsigned char>('t'));
    }

    inline bool output_csi_window_manipulation_replay()
    {
        terminal_parser_ffi_output_csi_window_manipulation_result plan{};
        const auto status = terminal_parser_ffi_output_csi_window_manipulation_plan(
            packed_csi_window_manipulation_id(), 8, 24, 80, &plan);
        if (status != TERMINAL_PARSER_FFI_OK ||
            plan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_WINDOW_MANIPULATION ||
            plan.function != 8 || plan.parameter1 != 24 || plan.parameter2 != 80)
        {
            std::fprintf(
                stderr,
                "output CSI window manipulation mismatch: status=%u kind=%u function=%d p1=%d p2=%d\n",
                static_cast<unsigned>(status),
                plan.kind,
                plan.function,
                plan.parameter1,
                plan.parameter2);
            return false;
        }

        terminal_parser_ffi_output_csi_window_manipulation_result defaulted{};
        const auto defaultStatus = terminal_parser_ffi_output_csi_window_manipulation_plan(
            packed_csi_window_manipulation_id(), 0, 0, 0, &defaulted);
        if (defaultStatus != TERMINAL_PARSER_FFI_OK ||
            defaulted.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_WINDOW_MANIPULATION ||
            defaulted.function != 1 || defaulted.parameter1 != 1 || defaulted.parameter2 != 1)
        {
            std::fprintf(
                stderr,
                "output CSI window manipulation default mismatch: status=%u kind=%u function=%d p1=%d p2=%d\n",
                static_cast<unsigned>(defaultStatus),
                defaulted.kind,
                defaulted.function,
                defaulted.parameter1,
                defaulted.parameter2);
            return false;
        }

        terminal_parser_ffi_output_csi_window_manipulation_result unrelated{};
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_window_manipulation_plan(
            static_cast<uint64_t>(static_cast<unsigned char>('m')), 8, 24, 80, &unrelated);
        if (unrelatedStatus != TERMINAL_PARSER_FFI_OK ||
            unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_WINDOW_MANIPULATION_NONE)
        {
            std::fprintf(
                stderr,
                "output CSI unrelated window manipulation mismatch: status=%u kind=%u\n",
                static_cast<unsigned>(unrelatedStatus),
                unrelated.kind);
            return false;
        }

        const auto nullStatus = terminal_parser_ffi_output_csi_window_manipulation_plan(
            packed_csi_window_manipulation_id(), 8, 24, 80, nullptr);
        if (nullStatus != TERMINAL_PARSER_FFI_INVALID_ARGUMENT)
        {
            std::fprintf(stderr, "output CSI window manipulation null pointer mismatch: status=%u\n", static_cast<unsigned>(nullStatus));
            return false;
        }

        return true;
    }
}
