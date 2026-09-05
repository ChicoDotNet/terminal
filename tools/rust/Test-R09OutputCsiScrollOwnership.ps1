$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_scroll.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiScrollAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI scroll ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI scroll ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI scroll ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_scroll.h"')) { throw 'R09 CSI scroll ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_scroll_plan')) { throw 'R09 CSI scroll ownership gate: ActionCsiDispatch no longer delegates scrolling classification to Rust.' }
if (-not $csiBody.Contains('switch (scrollPlan.kind)')) { throw 'R09 CSI scroll ownership gate: native dispatch no longer materializes the Rust scrolling plan.' }
if (-not $csiBody.Contains('_dispatch->ScrollUp(scrollPlan.count);')) { throw 'R09 CSI scroll ownership gate: native dispatch no longer uses the Rust-owned scroll-up count.' }
if (-not $csiBody.Contains('_dispatch->ScrollDown(scrollPlan.count);')) { throw 'R09 CSI scroll ownership gate: native dispatch no longer uses the Rust-owned scroll-down count.' }
if (-not $csiBody.Contains('if (scrollPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_NONE)')) { throw 'R09 CSI scroll ownership gate: Rust scrolling ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::SU_ScrollUp:')) { throw 'R09 CSI scroll ownership gate: portable SU classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::SD_ScrollDown:')) { throw 'R09 CSI scroll ownership gate: portable SD classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_scroll_plan')) { throw 'R09 CSI scroll ownership gate: terminal-parser-ffi no longer exports the scrolling planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI scroll ownership gate: scrolling FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains('terminal_parser_ffi_output_csi_scroll_plan')) { throw 'R09 CSI scroll ownership gate: native replay no longer exercises the scrolling planning seam.' }
if (-not $probe.Contains("expect_output_csi_scroll_plan('S', 0")) { throw 'R09 CSI scroll ownership gate: native replay no longer protects the scroll-up default-count witness.' }
if (-not $probe.Contains("expect_output_csi_scroll_plan('S', 4")) { throw 'R09 CSI scroll ownership gate: native replay no longer protects the scroll-up explicit-count witness.' }
if (-not $probe.Contains("expect_output_csi_scroll_plan('T', 0")) { throw 'R09 CSI scroll ownership gate: native replay no longer protects the scroll-down default-count witness.' }
if (-not $probe.Contains("expect_output_csi_scroll_plan('T', 7")) { throw 'R09 CSI scroll ownership gate: native replay no longer protects the scroll-down explicit-count witness.' }

Write-Host 'R09 CSI scroll ownership gate passed: Rust owns SU/SD classification and count normalization; C++ retains only native dispatch materialization.'
