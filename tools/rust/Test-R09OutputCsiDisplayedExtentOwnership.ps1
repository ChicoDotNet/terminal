$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_displayed_extent.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiDisplayedExtentAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI displayed extent ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI displayed extent ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI displayed extent ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_displayed_extent.h"')) { throw 'R09 CSI displayed extent ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_displayed_extent_plan')) { throw 'R09 CSI displayed extent ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (displayedExtentPlan.kind)')) { throw 'R09 CSI displayed extent ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('_dispatch->RequestDisplayedExtent();')) { throw 'R09 CSI displayed extent ownership gate: native displayed extent materialization is missing.' }
if (-not $csiBody.Contains('if (displayedExtentPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_NONE)')) { throw 'R09 CSI displayed extent ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::DECRQDE_RequestDisplayedExtent:')) { throw 'R09 CSI displayed extent ownership gate: portable DECRQDE classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_displayed_extent_plan')) { throw 'R09 CSI displayed extent ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI displayed extent ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('request.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_REQUEST')) { throw 'R09 CSI displayed extent ownership gate: native replay no longer protects the DECRQDE witness.' }
if (-not $probe.Contains('unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_NONE')) { throw 'R09 CSI displayed extent ownership gate: native replay no longer protects the unrelated CSI rejection witness.' }

Write-Host 'R09 CSI displayed extent ownership gate passed: Rust owns DECRQDE classification; C++ retains only native dispatch materialization.'
