$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_cursor_restore.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiCursorRestoreAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI cursor restore ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI cursor restore ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI cursor restore ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_cursor_restore.h"')) { throw 'R09 CSI cursor restore ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_cursor_restore_plan')) { throw 'R09 CSI cursor restore ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (cursorRestorePlan.kind)')) { throw 'R09 CSI cursor restore ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('_dispatch->CursorRestoreState();')) { throw 'R09 CSI cursor restore ownership gate: native cursor restore materialization is missing.' }
if (-not $csiBody.Contains('if (cursorRestorePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_NONE)')) { throw 'R09 CSI cursor restore ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::ANSISYSRC_CursorRestore:')) { throw 'R09 CSI cursor restore ownership gate: portable ANSISYSRC classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_cursor_restore_plan')) { throw 'R09 CSI cursor restore ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI cursor restore ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains("expect_output_csi_cursor_restore_plan('u', TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_RESTORE)")) { throw 'R09 CSI cursor restore ownership gate: native replay no longer protects the cursor restore witness.' }
if (-not $probe.Contains("expect_output_csi_cursor_restore_plan('m', TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_NONE)")) { throw 'R09 CSI cursor restore ownership gate: native replay no longer protects the unrelated CSI rejection witness.' }

Write-Host 'R09 CSI cursor restore ownership gate passed: Rust owns ANSISYSRC classification; C++ retains only native dispatch materialization.'
