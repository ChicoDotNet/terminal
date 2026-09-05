$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourcePath = Join-Path $repoRoot 'src/terminal/parser/OutputStateMachineEngine.cpp'
$escFfiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_esc.rs'
$escProbePath = Join-Path $repoRoot 'tools/rust/R09OutputEscAbiProbe.hpp'
$vt52FfiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_vt52.rs'
$vt52ProbePath = Join-Path $repoRoot 'tools/rust/R09OutputVt52AbiProbe.hpp'
$csiCursorFfiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_cursor.rs'
$csiCursorProbePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiCursorAbiProbe.hpp'
$csiMarginsFfiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_margins.rs'
$csiMarginsProbePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiMarginsAbiProbe.hpp'
$csiEditFfiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_edit.rs'
$csiEditProbePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiEditAbiProbe.hpp'
$csiLineEditFfiPath = Join-Path $repoRoot 'rust/terminal-parser-ffi/src/output_csi_line_edit.rs'
$csiLineEditProbePath = Join-Path $repoRoot 'tools/rust/R09OutputCsiLineEditAbiProbe.hpp'
$source = Get-Content -Raw -LiteralPath $sourcePath
$escFfi = Get-Content -Raw -LiteralPath $escFfiPath
$escProbe = Get-Content -Raw -LiteralPath $escProbePath
$vt52Ffi = Get-Content -Raw -LiteralPath $vt52FfiPath
$vt52Probe = Get-Content -Raw -LiteralPath $vt52ProbePath
$csiCursorFfi = Get-Content -Raw -LiteralPath $csiCursorFfiPath
$csiCursorProbe = Get-Content -Raw -LiteralPath $csiCursorProbePath
$csiMarginsFfi = Get-Content -Raw -LiteralPath $csiMarginsFfiPath
$csiMarginsProbe = Get-Content -Raw -LiteralPath $csiMarginsProbePath
$csiEditFfi = Get-Content -Raw -LiteralPath $csiEditFfiPath
$csiEditProbe = Get-Content -Raw -LiteralPath $csiEditProbePath
$csiLineEditFfi = Get-Content -Raw -LiteralPath $csiLineEditFfiPath
$csiLineEditProbe = Get-Content -Raw -LiteralPath $csiLineEditProbePath

function Get-FunctionBody([string] $Signature)
{
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "R09 Output ownership gate: $Signature signature not found." }
    $openBrace = $source.IndexOf('{', $start)
    if ($openBrace -lt 0) { throw "R09 Output ownership gate: $Signature opening brace not found." }
    $depth = 0
    for ($i = $openBrace; $i -lt $source.Length; $i++)
    {
        switch ($source[$i])
        {
            '{' { $depth++ }
            '}' { $depth--; if ($depth -eq 0) { return $source.Substring($start, $i - $start + 1) } }
        }
    }
    throw "R09 Output ownership gate: $Signature closing brace not found."
}

$executeBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionExecute(const wchar_t wch)'
if (-not $executeBody.Contains('terminal_parser_ffi_output_execute_plan')) { throw 'R09 Output ownership gate: ActionExecute no longer delegates C0 classification to Rust.' }
if ($executeBody.Contains('case AsciiChars::')) { throw 'R09 Output ownership gate: portable C0 classification returned to C++.' }
if (-not $executeBody.Contains('_ClearLastChar();')) { throw 'R09 Output ownership gate: native C0 last-character sequencing was removed.' }

$escBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionEscDispatch(const VTID id)'
if (-not $escBody.Contains('terminal_parser_ffi_output_esc_plan')) { throw 'R09 Output ownership gate: ActionEscDispatch no longer delegates ESC classification to Rust.' }
if ($escBody.Contains('switch (id)')) { throw 'R09 Output ownership gate: portable ESC classification returned to C++.' }
if (-not $escBody.Contains('switch (plan.kind)')) { throw 'R09 Output ownership gate: native ESC dispatch materialization no longer consumes the Rust plan.' }
if (-not $escBody.Contains('_ClearLastChar();')) { throw 'R09 Output ownership gate: native ESC last-character sequencing was removed.' }
if (-not $escFfi.Contains('terminal_parser_ffi_output_esc_plan')) { throw 'R09 Output ownership gate: terminal-parser-ffi no longer exports the Output ESC planning seam.' }
if (-not $escFfi.Contains('engine.action_esc_dispatch(id)')) { throw 'R09 Output ownership gate: Output ESC FFI no longer delegates to the Rust output engine.' }
if (-not $escProbe.Contains('terminal_parser_ffi_output_esc_plan')) { throw 'R09 Output ownership gate: native replay no longer exercises the Output ESC planning seam.' }

$vt52Body = Get-FunctionBody 'bool OutputStateMachineEngine::ActionVt52EscDispatch(const VTID id, const VTParameters parameters)'
if (-not $vt52Body.Contains('terminal_parser_ffi_output_vt52_plan')) { throw 'R09 Output ownership gate: ActionVt52EscDispatch no longer delegates VT52 classification to Rust.' }
if ($vt52Body.Contains('switch (id)') -or $vt52Body.Contains('case Vt52ActionCodes::')) { throw 'R09 Output ownership gate: portable VT52 classification returned to C++.' }
if (-not $vt52Body.Contains('switch (plan.kind)')) { throw 'R09 Output ownership gate: native VT52 dispatch materialization no longer consumes the Rust plan.' }
if (-not $vt52Body.Contains('_ClearLastChar();')) { throw 'R09 Output ownership gate: native VT52 last-character sequencing was removed.' }
if (-not $vt52Ffi.Contains('terminal_parser_ffi_output_vt52_plan')) { throw 'R09 Output ownership gate: terminal-parser-ffi no longer exports the Output VT52 planning seam.' }
if (-not $vt52Ffi.Contains('engine.action_vt52_esc_dispatch(id, &parameters)')) { throw 'R09 Output ownership gate: Output VT52 FFI no longer delegates to the Rust output engine.' }
if (-not $vt52Probe.Contains('terminal_parser_ffi_output_vt52_plan')) { throw 'R09 Output ownership gate: native replay no longer exercises the Output VT52 planning seam.' }

$csiBody = Get-FunctionBody 'bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)'
if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_cursor_plan')) { throw 'R09 Output ownership gate: ActionCsiDispatch no longer delegates cursor/navigation classification to Rust.' }
if (-not $csiBody.Contains('switch (cursorPlan.kind)')) { throw 'R09 Output ownership gate: native CSI cursor dispatch materialization no longer consumes the Rust plan.' }
$legacyCursorCases = @('case CsiActionCodes::CUU_CursorUp:','case CsiActionCodes::CUD_CursorDown:','case CsiActionCodes::CUF_CursorForward:','case CsiActionCodes::CUB_CursorBackward:','case CsiActionCodes::CNL_CursorNextLine:','case CsiActionCodes::CPL_CursorPrevLine:','case CsiActionCodes::CHA_CursorHorizontalAbsolute:','case CsiActionCodes::HPA_HorizontalPositionAbsolute:','case CsiActionCodes::VPA_VerticalLinePositionAbsolute:','case CsiActionCodes::HPR_HorizontalPositionRelative:','case CsiActionCodes::VPR_VerticalPositionRelative:','case CsiActionCodes::CUP_CursorPosition:','case CsiActionCodes::HVP_HorizontalVerticalPosition:')
foreach ($legacyCase in $legacyCursorCases) { if ($csiBody.Contains($legacyCase)) { throw "R09 Output ownership gate: portable CSI cursor classification returned to C++ at $legacyCase" } }
if (-not $csiBody.Contains('if (cursorPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NONE)')) { throw 'R09 Output ownership gate: CSI cursor Rust ownership no longer short-circuits before the remaining native CSI switch.' }
if (-not $csiCursorFfi.Contains('terminal_parser_ffi_output_csi_cursor_plan')) { throw 'R09 Output ownership gate: terminal-parser-ffi no longer exports the CSI cursor planning seam.' }
if (-not $csiCursorFfi.Contains('engine.action_csi_dispatch')) { throw 'R09 Output ownership gate: CSI cursor FFI no longer delegates to the Rust output engine.' }
if (-not $csiCursorProbe.Contains('terminal_parser_ffi_output_csi_cursor_plan')) { throw 'R09 Output ownership gate: native replay no longer exercises the CSI cursor planning seam.' }

if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_margins_plan')) { throw 'R09 Output ownership gate: ActionCsiDispatch no longer delegates scrolling-margin classification to Rust.' }
if (-not $csiBody.Contains('switch (marginsPlan.kind)')) { throw 'R09 Output ownership gate: native CSI margins dispatch materialization no longer consumes the Rust plan.' }
$legacyMarginsCases = @('case CsiActionCodes::DECSTBM_SetTopBottomMargins:','case CsiActionCodes::DECSLRM_SetLeftRightMargins:')
foreach ($legacyCase in $legacyMarginsCases) { if ($csiBody.Contains($legacyCase)) { throw "R09 Output ownership gate: portable CSI margins classification returned to C++ at $legacyCase" } }
if (-not $csiBody.Contains('if (marginsPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_NONE)')) { throw 'R09 Output ownership gate: CSI margins Rust ownership no longer short-circuits before the remaining native CSI switch.' }
if (-not $csiMarginsFfi.Contains('terminal_parser_ffi_output_csi_margins_plan')) { throw 'R09 Output ownership gate: terminal-parser-ffi no longer exports the CSI margins planning seam.' }
if (-not $csiMarginsFfi.Contains('engine.action_csi_dispatch')) { throw 'R09 Output ownership gate: CSI margins FFI no longer delegates to the Rust output engine.' }
if (-not $csiMarginsProbe.Contains('terminal_parser_ffi_output_csi_margins_plan')) { throw 'R09 Output ownership gate: native replay no longer exercises the CSI margins planning seam.' }

if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_edit_plan')) { throw 'R09 Output ownership gate: ActionCsiDispatch no longer delegates character-editing classification to Rust.' }
if (-not $csiBody.Contains('switch (editPlan.kind)')) { throw 'R09 Output ownership gate: native CSI character-edit dispatch materialization no longer consumes the Rust plan.' }
$legacyEditCases = @('case CsiActionCodes::ICH_InsertCharacter:','case CsiActionCodes::DCH_DeleteCharacter:')
foreach ($legacyCase in $legacyEditCases) { if ($csiBody.Contains($legacyCase)) { throw "R09 Output ownership gate: portable CSI character-edit classification returned to C++ at $legacyCase" } }
if (-not $csiBody.Contains('if (editPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_EDIT_NONE)')) { throw 'R09 Output ownership gate: CSI character-edit Rust ownership no longer short-circuits before the remaining native CSI switch.' }
if (-not $csiEditFfi.Contains('terminal_parser_ffi_output_csi_edit_plan')) { throw 'R09 Output ownership gate: terminal-parser-ffi no longer exports the CSI character-edit planning seam.' }
if (-not $csiEditFfi.Contains('engine.action_csi_dispatch')) { throw 'R09 Output ownership gate: CSI character-edit FFI no longer delegates to the Rust output engine.' }
if (-not $csiEditProbe.Contains('terminal_parser_ffi_output_csi_edit_plan')) { throw 'R09 Output ownership gate: native replay no longer exercises the CSI character-edit planning seam.' }

if (-not $csiBody.Contains('terminal_parser_ffi_output_csi_line_edit_plan')) { throw 'R09 Output ownership gate: ActionCsiDispatch no longer delegates line-editing classification to Rust.' }
if (-not $csiBody.Contains('switch (lineEditPlan.kind)')) { throw 'R09 Output ownership gate: native CSI line-edit dispatch materialization no longer consumes the Rust plan.' }
$legacyLineEditCases = @('case CsiActionCodes::IL_InsertLine:','case CsiActionCodes::DL_DeleteLine:')
foreach ($legacyCase in $legacyLineEditCases) { if ($csiBody.Contains($legacyCase)) { throw "R09 Output ownership gate: portable CSI line-edit classification returned to C++ at $legacyCase" } }
if (-not $csiBody.Contains('if (lineEditPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_LINE_EDIT_NONE)')) { throw 'R09 Output ownership gate: CSI line-edit Rust ownership no longer short-circuits before the remaining native CSI switch.' }
if (-not $csiLineEditFfi.Contains('terminal_parser_ffi_output_csi_line_edit_plan')) { throw 'R09 Output ownership gate: terminal-parser-ffi no longer exports the CSI line-edit planning seam.' }
if (-not $csiLineEditFfi.Contains('engine.action_csi_dispatch')) { throw 'R09 Output ownership gate: CSI line-edit FFI no longer delegates to the Rust output engine.' }
if (-not $csiLineEditProbe.Contains('terminal_parser_ffi_output_csi_line_edit_plan')) { throw 'R09 Output ownership gate: native replay no longer exercises the CSI line-edit planning seam.' }

Write-Host 'R09 Output ownership gate passed: Rust owns C0, ESC, VT52, CSI cursor/navigation, CSI scrolling margins, CSI character editing, and CSI line editing; native dispatch sequencing remains at the Windows seam.'