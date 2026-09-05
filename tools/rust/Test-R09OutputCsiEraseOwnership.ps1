$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_erase.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiEraseAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI erase ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI erase ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI erase ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_erase.h"')) { throw 'R09 CSI erase ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_erase_plans')) { throw 'R09 CSI erase ownership gate: ActionCsiDispatch no longer delegates erase planning to Rust.' }
if (-not $csiBody.Contains('constexpr size_t erasePlanCapacity = 32;')) { throw 'R09 CSI erase ownership gate: bounded caller-owned erase plan capacity is missing.' }
if (-not $csiBody.Contains('parameters.for_each([&](const auto eraseType)')) { throw 'R09 CSI erase ownership gate: native parameter serialization into the Rust ABI is missing.' }
if (-not $csiBody.Contains('for (size_t index = 0; index < erasePlanCount; ++index)')) { throw 'R09 CSI erase ownership gate: native materialization no longer preserves Rust plan ordering.' }
if (-not $csiBody.Contains('const auto eraseType = static_cast<DispatchTypes::EraseType>(erasePlan.value);')) { throw 'R09 CSI erase ownership gate: native EraseType adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->EraseInDisplay(eraseType);')) { throw 'R09 CSI erase ownership gate: EraseInDisplay adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->SelectiveEraseInDisplay(eraseType);')) { throw 'R09 CSI erase ownership gate: SelectiveEraseInDisplay adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->EraseInLine(eraseType);')) { throw 'R09 CSI erase ownership gate: EraseInLine adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->SelectiveEraseInLine(eraseType);')) { throw 'R09 CSI erase ownership gate: SelectiveEraseInLine adaptation is missing.' }
if (-not $csiBody.Contains('if (erasePlanCount != 0)')) { throw 'R09 CSI erase ownership gate: Rust-owned erase batches no longer short-circuit before the residual CSI switch.' }

foreach ($legacyCase in @(
    'case CsiActionCodes::ED_EraseDisplay:',
    'case CsiActionCodes::DECSED_SelectiveEraseDisplay:',
    'case CsiActionCodes::EL_EraseLine:',
    'case CsiActionCodes::DECSEL_SelectiveEraseLine:'
))
{
    if ($csiBody.Contains($legacyCase)) { throw "R09 CSI erase ownership gate: duplicate C++ classification returned: $legacyCase" }
}

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_erase_plans')) { throw 'R09 CSI erase ownership gate: terminal-parser-ffi no longer exports the batch planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI erase ownership gate: FFI no longer delegates classification to the Rust output engine.' }
if (-not $ffi.Contains('value_count > MAX_PARAMETER_COUNT')) { throw 'R09 CSI erase ownership gate: FFI no longer enforces the parser parameter bound.' }
if (-not $probe.Contains('constexpr std::array<int32_t, 3> values{ 0, 1, 2 };')) { throw 'R09 CSI erase ownership gate: native replay no longer protects ordered multiparameter witnesses.' }
if (-not $probe.Contains('TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_DISPLAY')) { throw 'R09 CSI erase ownership gate: ED witness is missing.' }
if (-not $probe.Contains('TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_SELECTIVE_DISPLAY')) { throw 'R09 CSI erase ownership gate: DECSED witness is missing.' }
if (-not $probe.Contains('TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_LINE')) { throw 'R09 CSI erase ownership gate: EL witness is missing.' }
if (-not $probe.Contains('TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_SELECTIVE_LINE')) { throw 'R09 CSI erase ownership gate: DECSEL witness is missing.' }
if (-not $probe.Contains('defaultPlan.value != 0')) { throw 'R09 CSI erase ownership gate: empty-parameter default witness is missing.' }
if (-not $probe.Contains('unrelatedStatus != TERMINAL_PARSER_FFI_OK || required != 0')) { throw 'R09 CSI erase ownership gate: unrelated CSI rejection witness is missing.' }

Write-Host 'R09 CSI erase ownership gate passed: Rust owns display/line erase classification and ordered parameter expansion; C++ retains only bounded ABI serialization, native EraseType adaptation, and dispatch materialization.'
