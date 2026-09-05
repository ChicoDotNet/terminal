$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_terminal_parameters.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiTerminalParametersAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI terminal parameters ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI terminal parameters ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI terminal parameters ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_terminal_parameters.h"')) { throw 'R09 CSI terminal parameters ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_terminal_parameters_plan')) { throw 'R09 CSI terminal parameters ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (terminalParametersPlan.kind)')) { throw 'R09 CSI terminal parameters ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('static_cast<DispatchTypes::ReportingPermission>(terminalParametersPlan.parameter)')) { throw 'R09 CSI terminal parameters ownership gate: native seam no longer adapts the Rust ABI value to ReportingPermission.' }
if (-not $csiBody.Contains('if (terminalParametersPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_NONE)')) { throw 'R09 CSI terminal parameters ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::DECREQTPARM_RequestTerminalParameters:')) { throw 'R09 CSI terminal parameters ownership gate: portable DECREQTPARM classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_terminal_parameters_plan')) { throw 'R09 CSI terminal parameters ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI terminal parameters ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('terminal_parser_ffi_output_csi_terminal_parameters_plan')) { throw 'R09 CSI terminal parameters ownership gate: native replay no longer exercises the planning seam.' }
if (-not $probe.Contains("expect_output_csi_terminal_parameters_plan('x', 0")) { throw 'R09 CSI terminal parameters ownership gate: native replay no longer protects the unsolicited witness.' }
if (-not $probe.Contains("expect_output_csi_terminal_parameters_plan('x', 1")) { throw 'R09 CSI terminal parameters ownership gate: native replay no longer protects the solicited witness.' }
if (-not $probe.Contains("expect_output_csi_terminal_parameters_plan('x', 9")) { throw 'R09 CSI terminal parameters ownership gate: native replay no longer protects the raw parameter witness.' }

Write-Host 'R09 CSI terminal parameters ownership gate passed: Rust owns DECREQTPARM classification; C++ retains only native enum adaptation and dispatch materialization.'
