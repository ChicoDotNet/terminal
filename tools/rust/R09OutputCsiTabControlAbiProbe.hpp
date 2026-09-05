#pragma once

#include "terminal_parser_ffi_output_csi_tab_control.h"

#include <array>
#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_tab_control_id(const bool set)
    {
        if (!set)
        {
            return static_cast<uint64_t>(static_cast<unsigned char>('g'));
        }
        return static_cast<uint64_t>(static_cast<unsigned char>('?')) |
               (static_cast<uint64_t>(static_cast<unsigned char>('W')) << 8);
    }

    inline bool expect_output_csi_tab_control_batch(const bool set, const uint32_t expectedKind)
    {
        constexpr std::array<int32_t, 3> values{ 0, 1, 3 };
        size_t required = 0;
        const auto sizingStatus = terminal_parser_ffi_output_csi_tab_control_plans(
            packed_csi_tab_control_id(set), values.data(), values.size(), nullptr, 0, &required);
        if (sizingStatus != TERMINAL_PARSER_FFI_BUFFER_TOO_SMALL || required != values.size())
        {
            std::fprintf(stderr, "output CSI tab control sizing mismatch: set=%u status=%u required=%zu\n", set, static_cast<unsigned>(sizingStatus), required);
            return false;
        }

        std::array<terminal_parser_ffi_output_csi_tab_control_result, 3> plans{};
        const auto status = terminal_parser_ffi_output_csi_tab_control_plans(
            packed_csi_tab_control_id(set), values.data(), values.size(), plans.data(), plans.size(), &required);
        if (status != TERMINAL_PARSER_FFI_OK || required != plans.size())
        {
            std::fprintf(stderr, "output CSI tab control replay status mismatch: set=%u status=%u required=%zu\n", set, static_cast<unsigned>(status), required);
            return false;
        }

        for (size_t index = 0; index < plans.size(); ++index)
        {
            if (plans[index].kind != expectedKind || plans[index].value != values[index])
            {
                std::fprintf(stderr, "output CSI tab control batch mismatch at %zu: kind=%u value=%d\n", index, plans[index].kind, plans[index].value);
                return false;
            }
        }
        return true;
    }

    inline bool output_csi_tab_control_replay()
    {
        if (!expect_output_csi_tab_control_batch(false, TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_CLEAR) ||
            !expect_output_csi_tab_control_batch(true, TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_SET))
        {
            return false;
        }

        size_t required = 0;
        const auto emptySizingStatus = terminal_parser_ffi_output_csi_tab_control_plans(
            packed_csi_tab_control_id(false), nullptr, 0, nullptr, 0, &required);
        if (emptySizingStatus != TERMINAL_PARSER_FFI_BUFFER_TOO_SMALL || required != 1)
        {
            std::fprintf(stderr, "output CSI empty tab control sizing mismatch: status=%u required=%zu\n", static_cast<unsigned>(emptySizingStatus), required);
            return false;
        }

        terminal_parser_ffi_output_csi_tab_control_result defaultPlan{};
        const auto emptyStatus = terminal_parser_ffi_output_csi_tab_control_plans(
            packed_csi_tab_control_id(false), nullptr, 0, &defaultPlan, 1, &required);
        if (emptyStatus != TERMINAL_PARSER_FFI_OK || required != 1 ||
            defaultPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_CLEAR || defaultPlan.value != 0)
        {
            std::fprintf(stderr, "output CSI empty tab control default mismatch: status=%u required=%zu kind=%u value=%d\n", static_cast<unsigned>(emptyStatus), required, defaultPlan.kind, defaultPlan.value);
            return false;
        }

        required = 99;
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_tab_control_plans(
            static_cast<uint64_t>(static_cast<unsigned char>('m')), nullptr, 0, nullptr, 0, &required);
        if (unrelatedStatus != TERMINAL_PARSER_FFI_OK || required != 0)
        {
            std::fprintf(stderr, "output CSI unrelated tab control mismatch: status=%u required=%zu\n", static_cast<unsigned>(unrelatedStatus), required);
            return false;
        }
        return true;
    }
}
