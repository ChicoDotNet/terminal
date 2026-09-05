#pragma once

#include "terminal_parser_ffi_output_csi_terminal_parameters.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_terminal_parameters_id(const char character)
    {
        return static_cast<uint64_t>(static_cast<unsigned char>(character));
    }

    inline bool expect_output_csi_terminal_parameters_plan(
        const char id,
        const int32_t parameter0,
        const uint32_t expectedKind,
        const int32_t expectedParameter)
    {
        terminal_parser_ffi_output_csi_terminal_parameters_result plan{};
        const auto status = terminal_parser_ffi_output_csi_terminal_parameters_plan(
            packed_csi_terminal_parameters_id(id), parameter0, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(stderr, "output CSI terminal parameters status %u for CSI %c\n", static_cast<unsigned>(status), id);
            return false;
        }

        if (plan.kind != expectedKind || plan.parameter != expectedParameter)
        {
            std::fprintf(
                stderr,
                "output CSI terminal parameters mismatch for CSI %c: kind=%u parameter=%d\n",
                id,
                plan.kind,
                plan.parameter);
            return false;
        }

        return true;
    }

    inline bool output_csi_terminal_parameters_replay()
    {
        return
            expect_output_csi_terminal_parameters_plan('x', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_REQUEST, 0) &&
            expect_output_csi_terminal_parameters_plan('x', 1, TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_REQUEST, 1) &&
            expect_output_csi_terminal_parameters_plan('x', 9, TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_REQUEST, 9) &&
            expect_output_csi_terminal_parameters_plan('m', 3, TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_NONE, 0);
    }
}
