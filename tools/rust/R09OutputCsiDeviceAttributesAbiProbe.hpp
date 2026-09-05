#pragma once

#include "terminal_parser_ffi_output_csi_device_attributes.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_device_attributes_id(const char prefix, const char finalCharacter)
    {
        if (prefix == '\0')
        {
            return static_cast<uint64_t>(static_cast<unsigned char>(finalCharacter));
        }

        return static_cast<uint64_t>(static_cast<unsigned char>(prefix)) |
               (static_cast<uint64_t>(static_cast<unsigned char>(finalCharacter)) << 8);
    }

    inline bool expect_output_csi_device_attributes_plan(
        const char prefix,
        const char finalCharacter,
        const int32_t parameter0,
        const uint32_t expectedKind)
    {
        terminal_parser_ffi_output_csi_device_attributes_result plan{};
        const auto status = terminal_parser_ffi_output_csi_device_attributes_plan(
            packed_csi_device_attributes_id(prefix, finalCharacter), parameter0, &plan);
        if (status != TERMINAL_PARSER_FFI_OK)
        {
            std::fprintf(
                stderr,
                "output CSI device attributes status %u for CSI %c%c\n",
                static_cast<unsigned>(status),
                prefix == '\0' ? ' ' : prefix,
                finalCharacter);
            return false;
        }

        if (plan.kind != expectedKind)
        {
            std::fprintf(
                stderr,
                "output CSI device attributes mismatch for CSI %c%c: kind=%u\n",
                prefix == '\0' ? ' ' : prefix,
                finalCharacter,
                plan.kind);
            return false;
        }

        return true;
    }

    inline bool output_csi_device_attributes_replay()
    {
        return
            expect_output_csi_device_attributes_plan('\0', 'c', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_PRIMARY) &&
            expect_output_csi_device_attributes_plan('>', 'c', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_SECONDARY) &&
            expect_output_csi_device_attributes_plan('=', 'c', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_TERTIARY) &&
            expect_output_csi_device_attributes_plan('\0', 'c', 1, TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE) &&
            expect_output_csi_device_attributes_plan('>', 'c', 7, TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE) &&
            expect_output_csi_device_attributes_plan('=', 'c', 9, TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE) &&
            expect_output_csi_device_attributes_plan('\0', 'm', 0, TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE);
    }
}
