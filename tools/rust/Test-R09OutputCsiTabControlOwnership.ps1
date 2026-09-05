$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_tab_control.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiTabControlAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI tab control ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI tab control ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI tab control ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_tab_control.h"')) { throw 'R09 CSI tab control ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_tab_control_plans')) { throw 'R09 CSI tab control ownership gate: ActionCsiDispatch no longer delegates tab control planning to Rust.' }
if (-not $csiBody.Contains('constexpr size_t tabControlPlanCapacity = 32;')) { throw 'R09 CSI tab control ownership gate: bounded caller-owned tab control capacity is missing.' }
if (-not $csiBody.Contains('parameters.for_each([&](const auto tabControlType)')) { throw 'R09 CSI tab control ownership gate: native parameter serialization into the Rust ABI is missing.' }
if (-not $csiBody.Contains('for (size_t index = 0; index < tabControlPlanCount; ++index)')) { throw 'R09 CSI tab control ownership gate: native materialization no longer preserves Rust plan ordering.' }
if (-not $csiBody.Contains('_dispatch->TabClear(static_cast<DispatchTypes::TabClearType>(tabControlPlan.value));')) { throw 'R09 CSI tab control ownership gate: native TabClearType adaptation is missing.' }
if (-not $csiBody.Contains('_dispatch->TabSet(tabControlPlan.value);')) { throw 'R09 CSI tab control ownership gate: native TabSet adaptation is missing.' }
if (-not $csiBody.Contains('if (tabControlPlanCount != 0)')) { throw 'R09 CSI tab control ownership gate: Rust-owned tab control batches no longer short-circuit before the residual CSI switch.' }

foreach ($legacyCase in @(
    'case CsiActionCodes::TBC_TabClear:',
    'case CsiActionCodes::DECST8C_SetTabEvery8Columns:'
))
{
    if ($csiBody.Contains($legacyCase)) { throw "R09 CSI tab control ownership gate: duplicate C++ classification returned: $legacyCase" }
}

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_tab_control_plans')) { throw 'R09 CSI tab control ownership gate: terminal-parser-ffi no longer exports the batch planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI tab control ownership gate: FFI no longer delegates classification to the Rust output engine.' }
if (-not $ffi.Contains('value_count > MAX_PARAMETER_COUNT')) { throw 'R09 CSI tab control ownership gate: FFI no longer enforces the parser parameter bound.' }
if (-not $probe.Contains('constexpr std::array<int32_t, 3> values{ 0, 1, 3 };')) { throw 'R09 CSI tab control ownership gate: native replay no longer protects ordered multiparameter witnesses.' }
if (-not $probe.Contains('TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_CLEAR')) { throw 'R09 CSI tab control ownership gate: TBC witness is missing.' }
if (-not $probe.Contains('TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_SET')) { throw 'R09 CSI tab control ownership gate: DECST8C witness is missing.' }
if (-not $probe.Contains('defaultPlan.value != 0')) { throw 'R09 CSI tab control ownership gate: empty-parameter default witness is missing.' }
if (-not $probe.Contains('unrelatedStatus != TERMINAL_PARSER_FFI_OK || required != 0')) { throw 'R09 CSI tab control ownership gate: unrelated CSI rejection witness is missing.' }

Write-Host 'R09 CSI tab control ownership gate passed: Rust owns TBC/DECST8C classification and ordered parameter expansion; C++ retains only bounded ABI serialization, TabClearType adaptation, and native dispatch materialization.'
