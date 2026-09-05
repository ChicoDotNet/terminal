#pragma once

#include "terminal_parser_ffi_output_csi_device_status_report.h"

#include <cstdint>
#include <cstdio>

namespace r09
{
    inline uint64_t packed_csi_device_status_report_id()
    {
        return static_cast<uint64_t>(static_cast<unsigned char>('n'));
    }

    inline uint64_t packed_csi_private_device_status_report_id()
    {
        return static_cast<uint64_t>(static_cast<unsigned char>('?')) |
               (static_cast<uint64_t>(static_cast<unsigned char>('n')) << 8);
    }

    inline bool output_csi_device_status_report_replay()
    {
        terminal_parser_ffi_output_csi_device_status_report_result ansi{};
        const auto ansiStatus = terminal_parser_ffi_output_csi_device_status_report_plan(
            packed_csi_device_status_report_id(), 5, 0, 0, &ansi);
        if (ansiStatus != TERMINAL_PARSER_FFI_OK ||
            ansi.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_REPORT ||
            ansi.private_mode != 0 ||
            ansi.status != 5 ||
            ansi.has_id != 0 ||
            ansi.id != 0)
        {
            std::fprintf(stderr, "output CSI ANSI device status report mismatch: status=%u kind=%u private=%u report=%d hasId=%u id=%d\n", static_cast<unsigned>(ansiStatus), ansi.kind, ansi.private_mode, ansi.status, ansi.has_id, ansi.id);
            return false;
        }

        terminal_parser_ffi_output_csi_device_status_report_result dec{};
        const auto decStatus = terminal_parser_ffi_output_csi_device_status_report_plan(
            packed_csi_private_device_status_report_id(), 6, 1, 42, &dec);
        if (decStatus != TERMINAL_PARSER_FFI_OK ||
            dec.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_REPORT ||
            dec.private_mode != 1 ||
            dec.status != 6 ||
            dec.has_id != 1 ||
            dec.id != 42)
        {
            std::fprintf(stderr, "output CSI DEC device status report mismatch: status=%u kind=%u private=%u report=%d hasId=%u id=%d\n", static_cast<unsigned>(decStatus), dec.kind, dec.private_mode, dec.status, dec.has_id, dec.id);
            return false;
        }

        terminal_parser_ffi_output_csi_device_status_report_result unrelated{};
        const auto unrelatedStatus = terminal_parser_ffi_output_csi_device_status_report_plan(
            static_cast<uint64_t>(static_cast<unsigned char>('m')), 3, 0, 0, &unrelated);
        if (unrelatedStatus != TERMINAL_PARSER_FFI_OK ||
            unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_NONE)
        {
            std::fprintf(stderr, "output CSI device status report unrelated mismatch: status=%u kind=%u\n", static_cast<unsigned>(unrelatedStatus), unrelated.kind);
            return false;
        }

        return true;
    }
}
