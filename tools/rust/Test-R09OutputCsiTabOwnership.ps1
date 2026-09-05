$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_tab.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiTabAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI tab ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI tab ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI tab ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_tab.h"')) { throw 'R09 CSI tab ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_tab_plan')) { throw 'R09 CSI tab ownership gate: ActionCsiDispatch no longer delegates tab classification to Rust.' }
if (-not $csiBody.Contains('switch (tabPlan.kind)')) { throw 'R09 CSI tab ownership gate: native dispatch no longer materializes the Rust tab plan.' }
if (-not $csiBody.Contains('_dispatch->ForwardTab(tabPlan.count);')) { throw 'R09 CSI tab ownership gate: native dispatch no longer uses the Rust-owned forward-tab count.' }
if (-not $csiBody.Contains('_dispatch->BackwardsTab(tabPlan.count);')) { throw 'R09 CSI tab ownership gate: native dispatch no longer uses the Rust-owned backward-tab count.' }
if (-not $csiBody.Contains('if (tabPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_NONE)')) { throw 'R09 CSI tab ownership gate: Rust tab ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::CHT_CursorForwardTab:')) { throw 'R09 CSI tab ownership gate: portable CHT classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::CBT_CursorBackTab:')) { throw 'R09 CSI tab ownership gate: portable CBT classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_tab_plan')) { throw 'R09 CSI tab ownership gate: terminal-parser-ffi no longer exports the tab planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI tab ownership gate: tab FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('terminal_parser_ffi_output_csi_tab_plan')) { throw 'R09 CSI tab ownership gate: native replay no longer exercises the tab planning seam.' }
if (-not $probe.Contains("expect_output_csi_tab_plan('I', 0")) { throw 'R09 CSI tab ownership gate: native replay no longer protects the forward default-count witness.' }
if (-not $probe.Contains("expect_output_csi_tab_plan('I', 4")) { throw 'R09 CSI tab ownership gate: native replay no longer protects the forward explicit-count witness.' }
if (-not $probe.Contains("expect_output_csi_tab_plan('Z', 0")) { throw 'R09 CSI tab ownership gate: native replay no longer protects the backward default-count witness.' }
if (-not $probe.Contains("expect_output_csi_tab_plan('Z', 7")) { throw 'R09 CSI tab ownership gate: native replay no longer protects the backward explicit-count witness.' }

Write-Host 'R09 CSI tab ownership gate passed: Rust owns CHT/CBT classification and count normalization; C++ retains only native dispatch materialization.'
