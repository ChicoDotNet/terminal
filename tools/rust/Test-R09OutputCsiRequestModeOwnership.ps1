$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_request_mode.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiRequestModeAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI request mode ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI request mode ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI request mode ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_request_mode.h"')) { throw 'R09 CSI request mode ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_request_mode_plan')) { throw 'R09 CSI request mode ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (requestModePlan.kind)')) { throw 'R09 CSI request mode ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('_dispatch->RequestMode(static_cast<DispatchTypes::DECPrivateMode>(requestModePlan.mode));')) { throw 'R09 CSI request mode ownership gate: DEC private enum adaptation/materialization is missing.' }
if (-not $csiBody.Contains('_dispatch->RequestMode(static_cast<DispatchTypes::ANSIStandardMode>(requestModePlan.mode));')) { throw 'R09 CSI request mode ownership gate: ANSI enum adaptation/materialization is missing.' }
if (-not $csiBody.Contains('if (requestModePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_NONE)')) { throw 'R09 CSI request mode ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::DECRQM_RequestMode:')) { throw 'R09 CSI request mode ownership gate: ANSI DECRQM classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::DECRQM_PrivateRequestMode:')) { throw 'R09 CSI request mode ownership gate: DEC-private DECRQM classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_request_mode_plan')) { throw 'R09 CSI request mode ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI request mode ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('ansi.private_mode != 0')) { throw 'R09 CSI request mode ownership gate: native replay no longer protects the ANSI mode witness.' }
if (-not $probe.Contains('ansi.mode != 4')) { throw 'R09 CSI request mode ownership gate: native replay no longer protects the ANSI mode payload.' }
if (-not $probe.Contains('dec.private_mode != 1')) { throw 'R09 CSI request mode ownership gate: native replay no longer protects the DEC-private witness.' }
if (-not $probe.Contains('dec.mode != 25')) { throw 'R09 CSI request mode ownership gate: native replay no longer protects the DEC-private payload.' }
if (-not $probe.Contains('unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_NONE')) { throw 'R09 CSI request mode ownership gate: native replay no longer protects unrelated CSI rejection.' }

Write-Host 'R09 CSI request mode ownership gate passed: Rust owns ANSI/DEC DECRQM classification; C++ retains only native enum adaptation and dispatch materialization.'
