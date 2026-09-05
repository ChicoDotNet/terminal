$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_device_status_report.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiDeviceStatusReportAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI device status report ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI device status report ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI device status report ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_device_status_report.h"')) { throw 'R09 CSI device status report ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_device_status_report_plan')) { throw 'R09 CSI device status report ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (deviceStatusReportPlan.kind)')) { throw 'R09 CSI device status report ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('const auto reportId = deviceStatusReportPlan.has_id != 0 ? VTParameter{ deviceStatusReportPlan.id } : VTParameter{};')) { throw 'R09 CSI device status report ownership gate: optional report id adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->DeviceStatusReport(DispatchTypes::DECPrivateStatus(deviceStatusReportPlan.status), reportId);')) { throw 'R09 CSI device status report ownership gate: DEC-private status adaptation/materialization is missing.' }
if (-not $csiBody.Contains('_dispatch->DeviceStatusReport(DispatchTypes::ANSIStandardStatus(deviceStatusReportPlan.status), reportId);')) { throw 'R09 CSI device status report ownership gate: ANSI status adaptation/materialization is missing.' }
if (-not $csiBody.Contains('if (deviceStatusReportPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_NONE)')) { throw 'R09 CSI device status report ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::DSR_DeviceStatusReport:')) { throw 'R09 CSI device status report ownership gate: ANSI DSR classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::DSR_PrivateDeviceStatusReport:')) { throw 'R09 CSI device status report ownership gate: DEC-private DSR classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_device_status_report_plan')) { throw 'R09 CSI device status report ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI device status report ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('ansi.private_mode != 0')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects the ANSI witness.' }
if (-not $probe.Contains('ansi.status != 5')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects the ANSI status payload.' }
if (-not $probe.Contains('ansi.has_id != 0')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects absent ANSI report id.' }
if (-not $probe.Contains('dec.private_mode != 1')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects the DEC-private witness.' }
if (-not $probe.Contains('dec.status != 6')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects the DEC-private status payload.' }
if (-not $probe.Contains('dec.has_id != 1')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects DEC report id presence.' }
if (-not $probe.Contains('dec.id != 42')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects the DEC report id payload.' }
if (-not $probe.Contains('unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_NONE')) { throw 'R09 CSI device status report ownership gate: native replay no longer protects unrelated CSI rejection.' }

Write-Host 'R09 CSI device status report ownership gate passed: Rust owns ANSI/DEC DSR classification; C++ retains only native enum/VTParameter adaptation and dispatch materialization.'
