$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$ffiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_device_attributes.rs'
$probePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiDeviceAttributesAbiProbe.hpp'

$source = Get-Content -Raw -LiteralPath $sourcePath
$ffi = Get-Content -Raw -LiteralPath $ffiPath
$probe = Get-Content -Raw -LiteralPath $probePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 CSI device attributes ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 CSI device attributes ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 CSI device attributes ownership gate: $Signature closing brace not found."
}

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'

if (-not $source.Contains('#include "terminal_parser_ffi_output_csi_device_attributes.h"')) { throw 'R09 CSI device attributes ownership gate: native product no longer includes the Rust ABI contract.' }
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_device_attributes_plan')) { throw 'R09 CSI device attributes ownership gate: ActionCsiDispatch no longer delegates classification to Rust.' }
if (-not $csiBody.Contains('switch (deviceAttributesPlan.kind)')) { throw 'R09 CSI device attributes ownership gate: native dispatch no longer materializes the Rust plan.' }
if (-not $csiBody.Contains('_dispatch->DeviceAttributes();')) { throw 'R09 CSI device attributes ownership gate: primary native dispatch materialization is missing.' }
if (-not $csiBody.Contains('_dispatch->SecondaryDeviceAttributes();')) { throw 'R09 CSI device attributes ownership gate: secondary native dispatch materialization is missing.' }
if (-not $csiBody.Contains('_dispatch->TertiaryDeviceAttributes();')) { throw 'R09 CSI device attributes ownership gate: tertiary native dispatch materialization is missing.' }
if (-not $csiBody.Contains('if (deviceAttributesPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE)')) { throw 'R09 CSI device attributes ownership gate: Rust ownership no longer short-circuits before the residual CSI switch.' }
if ($csiBody.Contains('case CsiActionCodes::DA_DeviceAttributes:')) { throw 'R09 CSI device attributes ownership gate: portable DA classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::DA2_SecondaryDeviceAttributes:')) { throw 'R09 CSI device attributes ownership gate: portable DA2 classification returned to C++.' }
if ($csiBody.Contains('case CsiActionCodes::DA3_TertiaryDeviceAttributes:')) { throw 'R09 CSI device attributes ownership gate: portable DA3 classification returned to C++.' }

if (-not $ffi.Contains('terminal_parser_ffi_output_csi_device_attributes_plan')) { throw 'R09 CSI device attributes ownership gate: terminal-parser-ffi no longer exports the planning seam.' }
if (-not $ffi.Contains('engine.action_csi_dispatch')) { throw 'R09 CSI device attributes ownership gate: FFI no longer delegates to the Rust output engine.' }
if (-not $probe.Contains("expect_output_csi_device_attributes_plan('\0', 'c', 0")) { throw 'R09 CSI device attributes ownership gate: native replay no longer protects the primary witness.' }
if (-not $probe.Contains("expect_output_csi_device_attributes_plan('>', 'c', 0")) { throw 'R09 CSI device attributes ownership gate: native replay no longer protects the secondary witness.' }
if (-not $probe.Contains("expect_output_csi_device_attributes_plan('=', 'c', 0")) { throw 'R09 CSI device attributes ownership gate: native replay no longer protects the tertiary witness.' }
if (-not $probe.Contains("expect_output_csi_device_attributes_plan('\0', 'c', 1")) { throw 'R09 CSI device attributes ownership gate: native replay no longer protects the nonzero primary rejection witness.' }

Write-Host 'R09 CSI device attributes ownership gate passed: Rust owns DA/DA2/DA3 classification; C++ retains only native dispatch materialization.'
