#pragma once

#include "terminal_parser_ffi_output_csi_mode.h"

#include <array>
#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_mode_id(const bool privateMode, const bool enabled)
    {
        const auto final = static_cast<unsigned char>(enabled ? 'h' : 'l');
        if (!privateMode)
        {
            return static_cast<uint64_t>(final);
        }
        return static_cast<uint64_t>(static_cast<unsigned char>('?')) |
               (static_cast<uint64_t>(final) << 8);
    }

    inline bool expect_output_csi_mode_batch(const bool privateMode, const bool enabled)
    {
        constexpr std::array<int32_t, 3> modes{ 4, 20, 25 };
        size_t required = 0;
        const auto sizingStatus = terminal_parser_ffi_output_csi_mode_plans(
            packed_csi_mode_id(privateMode, enabled),
            modes.data(),
            modes.size(),
            nullptr,
            0,
            &required);
        if (sizingStatus != TERMINAL_PARSER_FFI_BUFFER_TOO_SMALL || required != modes.size())
        {
            std::fprintf(stderr, "output CSI mode sizing mismatch: private=%u enabled=%u status=%u required=%zu\n", privateMode, enabled, static_cast<unsigned>(sizingStatus), required);
            return false;
        }

        std::array<terminal_parser_ffi_output_csi_mode_result, 3> plans{};
        const auto status = terminal_parser_ffi_output_csi_mode_plans(
            packed_csi_mode_id(privateMode, enabled),
            modes.data(),
            modes.size(),
            plans.data(),
            plans.size(),
            &required);
        if (status != TERMINAL_PARSER_FFI_OK || required != plans.size())
        {
            std::fprintf(stderr, "output CSI mode replay status mismatch: private=%u enabled=%u status=%u required=%zu\n", privateMode, enabled, static_cast<unsigned>(status), required);
            return false;
        }

        for (size_t index = 0; index < plans.size(); ++index)
        {
            const auto& plan = plans[index];
            if (plan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_MODE_MODE ||
                plan.private_mode != static_cast<uint32_t>(privateMode) ||
                plan.enabled != static_cast<uint32_t>(enabled) ||
                plan.mode != modes[index])
            {
                std::fprintf(stderr, "output CSI mode batch mismatch at %zu: kind=%u private=%u enabled=%u mode=%d\n", index, plan.kind, plan.private_mode, plan.enabled, plan.mode);
                return false;
            }
        }

        return true;
    }

    inline bool output_csi_mode_replay()
    {
        if (!expect_output_csi_mode_batch(false, true) ||
            !expect_output_csi_mode_batch(true, true) ||
            !expect_output_csi_mode_batch(false, false) ||
            !expect_output_csi_mode_batch(true, false))
        {
            return false;
        }

        size_t required = 0;
        const auto emptySizingStatus = terminal_parser_ffi_output_csi_mode_plans(
            packed_csi_mode_id(false, true), nullptr, 0, nullptr, 0, &required);
        if (emptySizingStatus != TERMINAL_PARSER_FFI_BUFFER_TOO_SMALL || required != 1)
        {
            std::fprintf(stderr, "output CSI empty mode sizing mismatch: status=%u required=%zu\n", static_cast<unsigned>(emptySizingStatus), required);
            return false;
        }

        terminal_parser_ffi_output_csi_mode_result defaultPlan{};
        const auto emptyStatus = terminal_parser_ffi_output_csi_mode_plans(
            packed_csi_mode_id(false, true), nullptr, 0, &defaultPlan, 1, &required);
        if (emptyStatus != TERMINAL_PARSER_FFI_OK || required != 1 ||
            defaultPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_MODE_MODE ||
            defaultPlan.private_mode != 0 || defaultPlan.enabled != 1 || defaultPlan.mode != 0)
        {
            std::fprintf(stderr, "output CSI empty mode default mismatch: status=%u required=%zu kind=%u private=%u enabled=%u mode=%d\n", static_cast<unsigned>(emptyStatus), required, defaultPlan.kind, defaultPlan.private_mode, defaultPlan.enabled, defaultPlan.mode);
            return false;
        }

        required = 99;
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_mode_plans(
            static_cast<uint64_t>(static_cast<unsigned char>('m')), nullptr, 0, nullptr, 0, &required);
        if (unrelatedStatus != TERMINAL_PARSER_FFI_OK || required != 0)
        {
            std::fprintf(stderr, "output CSI unrelated mode mismatch: status=%u required=%zu\n", static_cast<unsigned>(unrelatedStatus), required);
            return false;
        }

        return true;
    }
}
