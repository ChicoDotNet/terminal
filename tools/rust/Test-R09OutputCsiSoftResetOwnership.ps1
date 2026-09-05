$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_soft_reset.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiSoftResetAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI soft reset ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI soft reset ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI soft reset ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_soft_reset.h"')) { throw 'R09 CSI soft reset ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_soft_reset_plan')) { throw 'R09 CSI soft reset ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (softResetPlan.kind)')) { throw 'R09 CSI soft reset ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('_dispatch->SoftReset();')) { throw 'R09 CSI soft reset ownership gate: native soft reset materialization is missing.' }
if (-not $csiBody.Contains('if (softResetPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_NONE)')) { throw 'R09 CSI soft reset ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::DECSTR_SoftReset:')) { throw 'R09 CSI soft reset ownership gate: portable DECSTR classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_soft_reset_plan')) { throw 'R09 CSI soft reset ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI soft reset ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains("expect_output_csi_soft_reset_plan('p', TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_SOFT_RESET)")) { throw 'R09 CSI soft reset ownership gate: native replay no longer protects the DECSTR witness.' }
if (-not $probe.Contains('unrelated.kind == TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_NONE')) { throw 'R09 CSI soft reset ownership gate: native replay no longer protects the unrelated CSI rejection witness.' }

Write-Host 'R09 CSI soft reset ownership gate passed: Rust owns DECSTR classification; C++ retains only native dispatch materialization.'
