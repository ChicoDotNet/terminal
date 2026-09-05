$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_cursor_style.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiCursorStyleAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI cursor style ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI cursor style ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI cursor style ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_cursor_style.h"')) { throw 'R09 CSI cursor style ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_cursor_style_plan')) { throw 'R09 CSI cursor style ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (cursorStylePlan.kind)')) { throw 'R09 CSI cursor style ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('_dispatch->SetCursorStyle(static_cast<DispatchTypes::CursorStyle>(cursorStylePlan.style));')) { throw 'R09 CSI cursor style ownership gate: native enum adaptation/materialization is missing.' }
if (-not $csiBody.Contains('if (cursorStylePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_NONE)')) { throw 'R09 CSI cursor style ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::DECSCUSR_SetCursorStyle:')) { throw 'R09 CSI cursor style ownership gate: portable DECSCUSR classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_cursor_style_plan')) { throw 'R09 CSI cursor style ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI cursor style ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('defaultStyle.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_SET_CURSOR_STYLE')) { throw 'R09 CSI cursor style ownership gate: native replay no longer protects the default cursor-style witness.' }
if (-not $probe.Contains('explicitStyle.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_SET_CURSOR_STYLE')) { throw 'R09 CSI cursor style ownership gate: native replay no longer protects the explicit cursor-style witness.' }
if (-not $probe.Contains('explicitStyle.style != 6')) { throw 'R09 CSI cursor style ownership gate: native replay no longer protects the cursor-style payload witness.' }
if (-not $probe.Contains('unrelated.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_NONE')) { throw 'R09 CSI cursor style ownership gate: native replay no longer protects the unrelated CSI rejection witness.' }

Write-Host 'R09 CSI cursor style ownership gate passed: Rust owns DECSCUSR classification; C++ retains only native enum adaptation and dispatch materialization.'
