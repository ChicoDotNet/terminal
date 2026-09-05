$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_erase_characters.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiEraseCharactersAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI erase characters ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI erase characters ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI erase characters ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_erase_characters.h"')) { throw 'R09 CSI erase characters ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_erase_characters_plan')) { throw 'R09 CSI erase characters ownership gate: ActionCsiDispatch no longer delegates erase-character classification to Rust.' }
if (-not $csiBody.Contains('switch (eraseCharactersPlan.kind)')) { throw 'R09 CSI erase characters ownership gate: native dispatch no longer materializes the Rust erase-character plan.' }
if (-not $csiBody.Contains('_dispatch->EraseCharacters(eraseCharactersPlan.count);')) { throw 'R09 CSI erase characters ownership gate: native dispatch no longer uses the Rust-owned erase count.' }
if (-not $csiBody.Contains('if (eraseCharactersPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_CHARACTERS_NONE)')) { throw 'R09 CSI erase characters ownership gate: Rust erase-character ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::ECH_EraseCharacters:')) { throw 'R09 CSI erase characters ownership gate: portable ECH classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_erase_characters_plan')) { throw 'R09 CSI erase characters ownership gate: terminal-parser-ffi no longer exports the erase-character planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI erase characters ownership gate: erase-character FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('terminal_parser_ffi_output_csi_erase_characters_plan')) { throw 'R09 CSI erase characters ownership gate: native replay no longer exercises the erase-character planning seam.' }
if (-not $probe.Contains("expect_output_csi_erase_characters_plan('X', 0")) { throw 'R09 CSI erase characters ownership gate: native replay no longer protects the default-count witness.' }
if (-not $probe.Contains("expect_output_csi_erase_characters_plan('X', 5")) { throw 'R09 CSI erase characters ownership gate: native replay no longer protects the explicit-count witness.' }

Write-Host 'R09 CSI erase characters ownership gate passed: Rust owns ECH classification and count normalization; C++ retains only native dispatch materialization.'
