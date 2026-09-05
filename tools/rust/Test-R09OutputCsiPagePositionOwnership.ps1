$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_page_position.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiPagePositionAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI page positioning ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI page positioning ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI page positioning ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_page_position.h"')) { throw 'R09 CSI page positioning ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_page_position_plan')) { throw 'R09 CSI page positioning ownership gate: ActionCsiDispatch no longer delegates page-position classification to Rust.' }
if (-not $csiBody.Contains('switch (pagePositionPlan.kind)')) { throw 'R09 CSI page positioning ownership gate: native dispatch no longer materializes the Rust page-position plan.' }
if (-not $csiBody.Contains('_dispatch->PagePositionAbsolute(pagePositionPlan.count);')) { throw 'R09 CSI page positioning ownership gate: native dispatch no longer uses the Rust-owned absolute count.' }
if (-not $csiBody.Contains('_dispatch->PagePositionRelative(pagePositionPlan.count);')) { throw 'R09 CSI page positioning ownership gate: native dispatch no longer uses the Rust-owned relative count.' }
if (-not $csiBody.Contains('_dispatch->PagePositionBack(pagePositionPlan.count);')) { throw 'R09 CSI page positioning ownership gate: native dispatch no longer uses the Rust-owned back count.' }
if (-not $csiBody.Contains('if (pagePositionPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_NONE)')) { throw 'R09 CSI page positioning ownership gate: Rust page-position ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::PPA_PagePositionAbsolute:')) { throw 'R09 CSI page positioning ownership gate: portable PPA classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::PPR_PagePositionRelative:')) { throw 'R09 CSI page positioning ownership gate: portable PPR classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::PPB_PagePositionBack:')) { throw 'R09 CSI page positioning ownership gate: portable PPB classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_page_position_plan')) { throw 'R09 CSI page positioning ownership gate: terminal-parser-ffi no longer exports the page-position planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI page positioning ownership gate: page-position FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('terminal_parser_ffi_output_csi_page_position_plan')) { throw 'R09 CSI page positioning ownership gate: native replay no longer exercises the page-position planning seam.' }
if (-not $probe.Contains("expect_output_csi_page_position_plan('P', 0")) { throw 'R09 CSI page positioning ownership gate: native replay no longer protects the absolute default-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_position_plan('P', 4")) { throw 'R09 CSI page positioning ownership gate: native replay no longer protects the absolute explicit-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_position_plan('Q', 0")) { throw 'R09 CSI page positioning ownership gate: native replay no longer protects the relative default-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_position_plan('Q', 5")) { throw 'R09 CSI page positioning ownership gate: native replay no longer protects the relative explicit-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_position_plan('R', 0")) { throw 'R09 CSI page positioning ownership gate: native replay no longer protects the back default-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_position_plan('R', 6")) { throw 'R09 CSI page positioning ownership gate: native replay no longer protects the back explicit-count witness.' }

Write-Host 'R09 CSI page positioning ownership gate passed: Rust owns PPA/PPR/PPB classification and count normalization; C++ retains only native dispatch materialization.'
