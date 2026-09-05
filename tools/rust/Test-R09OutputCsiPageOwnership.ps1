$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_page.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiPageAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI page ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI page ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI page ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_page.h"')) { throw 'R09 CSI page ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_page_plan')) { throw 'R09 CSI page ownership gate: ActionCsiDispatch no longer delegates page navigation classification to Rust.' }
if (-not $csiBody.Contains('switch (pagePlan.kind)')) { throw 'R09 CSI page ownership gate: native dispatch no longer materializes the Rust page navigation plan.' }
if (-not $csiBody.Contains('_dispatch->NextPage(pagePlan.count);')) { throw 'R09 CSI page ownership gate: native dispatch no longer uses the Rust-owned next-page count.' }
if (-not $csiBody.Contains('_dispatch->PrecedingPage(pagePlan.count);')) { throw 'R09 CSI page ownership gate: native dispatch no longer uses the Rust-owned preceding-page count.' }
if (-not $csiBody.Contains('if (pagePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_NONE)')) { throw 'R09 CSI page ownership gate: Rust page navigation ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::NP_NextPage:')) { throw 'R09 CSI page ownership gate: portable NP classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::PP_PrecedingPage:')) { throw 'R09 CSI page ownership gate: portable PP classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_page_plan')) { throw 'R09 CSI page ownership gate: terminal-parser-ffi no longer exports the page navigation planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI page ownership gate: page navigation FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('terminal_parser_ffi_output_csi_page_plan')) { throw 'R09 CSI page ownership gate: native replay no longer exercises the page navigation planning seam.' }
if (-not $probe.Contains("expect_output_csi_page_plan('U', 0")) { throw 'R09 CSI page ownership gate: native replay no longer protects the next-page default-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_plan('U', 4")) { throw 'R09 CSI page ownership gate: native replay no longer protects the next-page explicit-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_plan('V', 0")) { throw 'R09 CSI page ownership gate: native replay no longer protects the preceding-page default-count witness.' }
if (-not $probe.Contains("expect_output_csi_page_plan('V', 7")) { throw 'R09 CSI page ownership gate: native replay no longer protects the preceding-page explicit-count witness.' }

Write-Host 'R09 CSI page ownership gate passed: Rust owns NP/PP classification and count normalization; C++ retains only native dispatch materialization.'
