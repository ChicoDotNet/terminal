$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_mode.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiModeAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI mode ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI mode ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI mode ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_mode.h"')) { throw 'R09 CSI mode ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_mode_plans')) { throw 'R09 CSI mode ownership gate: ActionCsiDispatch no longer delegates mode planning to Rust.' }
if (-not $csiBody.Contains('constexpr size_t modePlanCapacity = 32;')) { throw 'R09 CSI mode ownership gate: bounded caller-owned mode plan capacity is missing.' }
if (-not $csiBody.Contains('parameters.for_each([&](const auto mode)')) { throw 'R09 CSI mode ownership gate: native parameter serialization into the Rust ABI is missing.' }
if (-not $csiBody.Contains('for (size_t index = 0; index < modePlanCount; ++index)')) { throw 'R09 CSI mode ownership gate: native materialization no longer preserves Rust plan ordering.' }
if (-not $csiBody.Contains('modePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_MODE_MODE')) { throw 'R09 CSI mode ownership gate: native product no longer validates Rust plan kind.' }
if (-not $csiBody.Contains('_dispatch->SetMode(static_cast<DispatchTypes::DECPrivateMode>(modePlan.mode));')) { throw 'R09 CSI mode ownership gate: DEC-private SetMode adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->ResetMode(static_cast<DispatchTypes::DECPrivateMode>(modePlan.mode));')) { throw 'R09 CSI mode ownership gate: DEC-private ResetMode adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->SetMode(static_cast<DispatchTypes::ANSIStandardMode>(modePlan.mode));')) { throw 'R09 CSI mode ownership gate: ANSI SetMode adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->ResetMode(static_cast<DispatchTypes::ANSIStandardMode>(modePlan.mode));')) { throw 'R09 CSI mode ownership gate: ANSI ResetMode adaptation is missing.' }
if (-not $csiBody.Contains('if (modePlanCount != 0)')) { throw 'R09 CSI mode ownership gate: Rust-owned mode batches no longer short-circuit before the residual CSI switch.' }

foreach ($legacyCase in @(
    'case CsiActionCodes::SM_SetMode:',
    'case CsiActionCodes::DECSET_PrivateModeSet:',
    'case CsiActionCodes::RM_ResetMode:',
    'case CsiActionCodes::DECRST_PrivateModeReset:'
))
{
    if ($csiBody.Contains($legacyCase)) { throw "R09 CSI mode ownership gate: duplicate C++ classification returned: $legacyCase" }
}

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_mode_plans')) { throw 'R09 CSI mode ownership gate: terminal-parser-ffi no longer exports the batch planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI mode ownership gate: FFI no longer delegates classification to the Rust output engine.' }
if (-not $probe.Contains('constexpr std::array<int32_t, 3> modes{ 4, 20, 25 };')) { throw 'R09 CSI mode ownership gate: native replay no longer protects ordered multiparameter witnesses.' }
if (-not $probe.Contains('expect_output_csi_mode_batch(false, true)')) { throw 'R09 CSI mode ownership gate: ANSI SetMode witness is missing.' }
if (-not $probe.Contains('expect_output_csi_mode_batch(true, true)')) { throw 'R09 CSI mode ownership gate: DEC-private SetMode witness is missing.' }
if (-not $probe.Contains('expect_output_csi_mode_batch(false, false)')) { throw 'R09 CSI mode ownership gate: ANSI ResetMode witness is missing.' }
if (-not $probe.Contains('expect_output_csi_mode_batch(true, false)')) { throw 'R09 CSI mode ownership gate: DEC-private ResetMode witness is missing.' }
if (-not $probe.Contains('defaultPlan.mode != 0')) { throw 'R09 CSI mode ownership gate: empty-parameter default-mode witness is missing.' }
if (-not $probe.Contains('unrelatedStatus != TERMINAL_PARSER_FFI_OK || required != 0')) { throw 'R09 CSI mode ownership gate: unrelated CSI rejection witness is missing.' }

Write-Host 'R09 CSI mode ownership gate passed: Rust owns ANSI/DEC Set/Reset mode classification and ordered parameter expansion; C++ retains only bounded ABI serialization, native enum adaptation, and dispatch materialization.'
