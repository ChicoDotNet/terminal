# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$parserRoot = Join-Path $repoRoot 'src\terminal\parser'
$base64Cpp = Join-Path $parserRoot 'base64.cpp'
$base64Header = Join-Path $parserRoot 'base64.hpp'
$parserBuild = Join-Path $parserRoot 'parser-common.vcxitems'
$inputEngine = Join-Path $parserRoot 'InputStateMachineEngine.cpp'
$ffiHeader = Join-Path $repoRoot 'rust\terminal-parser-ffi\include\terminal_parser_ffi.h'
$ffiKeymap = Join-Path $repoRoot 'rust\terminal-parser-ffi\src\input_keymap.rs'
$ffiWin32 = Join-Path $repoRoot 'rust\terminal-parser-ffi\src\input_win32.rs'
$ffiControl = Join-Path $repoRoot 'rust\terminal-parser-ffi\src\input_control.rs'
$ffiMouse = Join-Path $repoRoot 'rust\terminal-parser-ffi\src\input_mouse.rs'
$rustKeymap = Join-Path $repoRoot 'rust\terminal-parser\src\input_keymap.rs'
$rustInputEngine = Join-Path $repoRoot 'rust\terminal-parser\src\input_engine.rs'
$rustControl = Join-Path $repoRoot 'rust\terminal-parser\src\input_control.rs'
$rustMouse = Join-Path $repoRoot 'rust\terminal-parser\src\input_mouse.rs'
$nativeAbiProbe = Join-Path $repoRoot 'tools\rust\R09ControlCharacterAbiProbe.cpp'

if (Test-Path $base64Cpp)
{
    throw 'R09 parser ownership regression: base64.cpp must remain deleted after Rust ownership promotion.'
}

$staleBuildReferences = Get-ChildItem -Path $parserRoot -Recurse -File |
    Select-String -SimpleMatch 'base64.cpp' -ErrorAction SilentlyContinue
if ($staleBuildReferences)
{
    $locations = ($staleBuildReferences | ForEach-Object { "{0}:{1}" -f $_.Path, $_.LineNumber }) -join ', '
    throw "R09 parser ownership regression: stale base64.cpp reference(s) found at $locations"
}

$headerText = Get-Content -LiteralPath $base64Header -Raw
if ($headerText -notmatch 'terminal_parser_ffi_base64_decode_utf16')
{
    throw 'R09 parser ownership regression: base64.hpp no longer routes Base64 decoding through terminal-parser-ffi.'
}

$buildText = Get-Content -LiteralPath $parserBuild -Raw
if ($buildText -notmatch 'terminal_parser_ffi\.lib' -or $buildText -notmatch 'cargo build --locked -p terminal-parser-ffi')
{
    throw 'R09 parser ownership regression: parser-common.vcxitems no longer builds and links terminal-parser-ffi.'
}

$expectedKeymapFunctions = @(
    'terminal_parser_ffi_input_cursor_vkey',
    'terminal_parser_ffi_input_generic_vkey',
    'terminal_parser_ffi_input_ss3_vkey'
)
$expectedModifierFunctions = @(
    'terminal_parser_ffi_input_vt_modifier_state',
    'terminal_parser_ffi_input_cursor_modifier_state',
    'terminal_parser_ffi_input_generic_modifier_state',
    'terminal_parser_ffi_input_sgr_mouse_modifier_state'
)
$win32KeyFunction = 'terminal_parser_ffi_input_win32_key_fields'
$controlCharacterFunction = 'terminal_parser_ffi_input_control_character_plan'
$sgrMousePlanFunction = 'terminal_parser_ffi_input_sgr_mouse_plan'

$ffiHeaderText = Get-Content -LiteralPath $ffiHeader -Raw
$ffiKeymapText = Get-Content -LiteralPath $ffiKeymap -Raw
$ffiWin32Text = Get-Content -LiteralPath $ffiWin32 -Raw
$ffiControlText = Get-Content -LiteralPath $ffiControl -Raw
$ffiMouseText = Get-Content -LiteralPath $ffiMouse -Raw
$rustKeymapText = Get-Content -LiteralPath $rustKeymap -Raw
$rustInputEngineText = Get-Content -LiteralPath $rustInputEngine -Raw
$rustControlText = Get-Content -LiteralPath $rustControl -Raw
$rustMouseText = Get-Content -LiteralPath $rustMouse -Raw
$nativeAbiProbeText = Get-Content -LiteralPath $nativeAbiProbe -Raw
$inputEngineText = Get-Content -LiteralPath $inputEngine -Raw
$win32KeyMarker = 'INPUT_RECORD InputStateMachineEngine::_GenerateWin32Key'
$win32KeyStart = $inputEngineText.IndexOf($win32KeyMarker, [System.StringComparison]::Ordinal)
if ($win32KeyStart -lt 0)
{
    throw 'R09 parser ownership regression: InputStateMachineEngine no longer contains the native Win32 key adapter.'
}
$win32KeyText = $inputEngineText.Substring($win32KeyStart)

foreach ($function in $expectedKeymapFunctions)
{
    if ($ffiHeaderText -notmatch [regex]::Escape($function))
    {
        throw "R09 parser ownership regression: terminal_parser_ffi.h no longer declares $function."
    }
    if ($ffiKeymapText -notmatch [regex]::Escape($function))
    {
        throw "R09 parser ownership regression: terminal-parser-ffi no longer exports $function."
    }
    if ($inputEngineText -notmatch [regex]::Escape($function))
    {
        throw "R09 parser ownership regression: InputStateMachineEngine no longer routes key mapping through $function."
    }
}

foreach ($function in $expectedModifierFunctions)
{
    if ($ffiHeaderText -notmatch [regex]::Escape($function))
    {
        throw "R09 parser ownership regression: terminal_parser_ffi.h no longer declares modifier seam $function."
    }
    if ($ffiKeymapText -notmatch [regex]::Escape($function))
    {
        throw "R09 parser ownership regression: terminal-parser-ffi no longer exports modifier seam $function."
    }
    if ($inputEngineText -notmatch [regex]::Escape($function))
    {
        throw "R09 parser ownership regression: InputStateMachineEngine no longer routes modifier composition through $function."
    }
}

if ($ffiHeaderText -notmatch [regex]::Escape($win32KeyFunction))
{
    throw "R09 parser ownership regression: terminal_parser_ffi.h no longer declares Win32 key seam $win32KeyFunction."
}
if ($ffiWin32Text -notmatch [regex]::Escape($win32KeyFunction))
{
    throw "R09 parser ownership regression: terminal-parser-ffi no longer exports Win32 key seam $win32KeyFunction."
}
if ($win32KeyText -notmatch [regex]::Escape($win32KeyFunction))
{
    throw "R09 parser ownership regression: InputStateMachineEngine no longer routes Win32 key normalization through $win32KeyFunction."
}
if ($rustInputEngineText -notmatch 'generate_win32_key')
{
    throw 'R09 parser ownership regression: terminal-parser no longer owns Win32 key normalization.'
}

if ($ffiHeaderText -notmatch [regex]::Escape($controlCharacterFunction))
{
    throw "R09 parser ownership regression: terminal_parser_ffi.h no longer declares control-character seam $controlCharacterFunction."
}
if ($ffiControlText -notmatch [regex]::Escape($controlCharacterFunction))
{
    throw "R09 parser ownership regression: terminal-parser-ffi no longer exports control-character seam $controlCharacterFunction."
}
if ($ffiControlText -notmatch 'classify_control_character')
{
    throw 'R09 parser ownership regression: control-character FFI no longer delegates to the terminal-parser owner.'
}
if ($rustControlText -notmatch 'pub const fn classify_control_character')
{
    throw 'R09 parser ownership regression: terminal-parser no longer owns deterministic control-character classification.'
}
foreach ($controlContractToken in @(
    'ControlCharacterKind::CtrlC',
    'ControlCharacterKind::MappedC0',
    'ControlCharacterKind::DeleteAsBackspace',
    'clear_layout_modifiers'
))
{
    if ($rustControlText -notmatch [regex]::Escape($controlContractToken))
    {
        throw "R09 parser ownership regression: control-character owner lost contract token $controlContractToken."
    }
}

if ($inputEngineText -notmatch [regex]::Escape($controlCharacterFunction))
{
    throw "R09 parser ownership regression: InputStateMachineEngine no longer routes control-character classification through $controlCharacterFunction."
}
foreach ($controlKindToken in @(
    'TERMINAL_PARSER_FFI_CONTROL_CHARACTER_CTRL_C',
    'TERMINAL_PARSER_FFI_CONTROL_CHARACTER_MAPPED_C0',
    'TERMINAL_PARSER_FFI_CONTROL_CHARACTER_DELETE_AS_BACKSPACE',
    'TERMINAL_PARSER_FFI_CONTROL_CHARACTER_PRINT'
))
{
    if ($inputEngineText -notmatch [regex]::Escape($controlKindToken))
    {
        throw "R09 parser ownership regression: native control-character adapter lost ABI kind $controlKindToken."
    }
}
foreach ($legacyControlImplementation in @(
    'if (wch == UNICODE_ETX && !writeAlt)',
    "else if (wch >= '\x0' && wch < '\x20')",
    "else if (wch == '\x7f')"
))
{
    if ($inputEngineText -match [regex]::Escape($legacyControlImplementation))
    {
        throw "R09 parser ownership regression: portable control-character classification returned to C++: $legacyControlImplementation"
    }
}

if ($ffiHeaderText -notmatch [regex]::Escape($sgrMousePlanFunction))
{
    throw "R09 parser ownership regression: terminal_parser_ffi.h no longer declares SGR mouse seam $sgrMousePlanFunction."
}
if ($ffiMouseText -notmatch [regex]::Escape($sgrMousePlanFunction))
{
    throw "R09 parser ownership regression: terminal-parser-ffi no longer exports SGR mouse seam $sgrMousePlanFunction."
}
if ($ffiMouseText -notmatch 'plan_sgr_mouse')
{
    throw 'R09 parser ownership regression: SGR mouse FFI no longer delegates to the terminal-parser owner.'
}
if ($rustMouseText -notmatch 'pub fn plan_sgr_mouse')
{
    throw 'R09 parser ownership regression: terminal-parser no longer owns deterministic SGR mouse state decoding.'
}
foreach ($sgrMouseContractToken in @(
    'persistent_button_state',
    'event_flags',
    'track_click',
    'MOUSE_WHEELED',
    'MOUSE_HWHEELED'
))
{
    if ($rustMouseText -notmatch [regex]::Escape($sgrMouseContractToken))
    {
        throw "R09 parser ownership regression: SGR mouse owner lost contract token $sgrMouseContractToken."
    }
}
if ($nativeAbiProbeText -notmatch [regex]::Escape($sgrMousePlanFunction))
{
    throw "R09 parser ownership regression: native ABI replay no longer exercises SGR mouse seam $sgrMousePlanFunction."
}
if ($inputEngineText -notmatch [regex]::Escape($sgrMousePlanFunction))
{
    throw "R09 parser ownership regression: InputStateMachineEngine no longer routes SGR mouse state through $sgrMousePlanFunction."
}
foreach ($sgrMouseAdapterToken in @(
    'plan.button_state',
    'plan.event_flags',
    'plan.track_click',
    'plan.persistent_button_state'
))
{
    if ($inputEngineText -notmatch [regex]::Escape($sgrMouseAdapterToken))
    {
        throw "R09 parser ownership regression: native SGR mouse adapter lost ABI field $sgrMouseAdapterToken."
    }
}
foreach ($legacySgrMouseImplementation in @(
    'const auto buttonID = (sgrEncoding & 0x3)',
    'case CsiMouseButtonCodes::ScrollBack:',
    'case CsiMouseButtonCodes::ScrollForward:',
    'WI_IsFlagSet(sgrEncoding, CsiMouseModifierCodes::Drag)',
    '_mouseButtonState = LOWORD(buttonState);'
))
{
    if ($inputEngineText -match [regex]::Escape($legacySgrMouseImplementation))
    {
        throw "R09 parser ownership regression: portable SGR mouse implementation returned to C++: $legacySgrMouseImplementation"
    }
}

foreach ($ownerFunction in @(
    'cursor_virtual_key',
    'generic_virtual_key',
    'ss3_virtual_key',
    'vt_modifier_state',
    'cursor_modifier_state',
    'generic_modifier_state',
    'sgr_mouse_modifier_state'
))
{
    if ($rustKeymapText -notmatch [regex]::Escape($ownerFunction))
    {
        throw "R09 parser ownership regression: terminal-parser no longer owns $ownerFunction."
    }
}

foreach ($rustKeymapAdapter in @(
    'keymap_cursor_virtual_key(',
    'keymap_generic_virtual_key(',
    'keymap_ss3_virtual_key('
))
{
    if ($rustInputEngineText -notmatch [regex]::Escape($rustKeymapAdapter))
    {
        throw "R09 parser ownership regression: terminal-parser input engine no longer delegates key mapping through $rustKeymapAdapter"
    }
}

foreach ($legacyRustKeymapImplementation in @(
    'fn cursor_virtual_key(',
    'fn generic_virtual_key(',
    'fn ss3_virtual_key('
))
{
    if ($rustInputEngineText -match [regex]::Escape($legacyRustKeymapImplementation))
    {
        throw "R09 parser ownership regression: duplicate Rust key-map implementation returned to input_engine.rs: $legacyRustKeymapImplementation"
    }
}

foreach ($rustModifierAdapter in @(
    'vt_modifier_state_from_parameter',
    'sgr_mouse_modifier_state_from_encoding'
))
{
    if ($rustInputEngineText -notmatch [regex]::Escape($rustModifierAdapter))
    {
        throw "R09 parser ownership regression: terminal-parser input engine no longer delegates modifier normalization through $rustModifierAdapter."
    }
}

foreach ($legacyRustModifierImplementation in @(
    'const VT_SHIFT:',
    'const VT_ALT:',
    'const VT_CTRL:',
    'const SGR_SHIFT:',
    'const SGR_META:',
    'const SGR_CTRL:',
    'fn vt_modifiers(',
    'fn sgr_mouse_modifiers('
))
{
    if ($rustInputEngineText -match [regex]::Escape($legacyRustModifierImplementation))
    {
        throw "R09 parser ownership regression: duplicate Rust modifier implementation returned to input_engine.rs: $legacyRustModifierImplementation"
    }
}

foreach ($legacySymbol in @(
    'CsiToVkey',
    'GenericToVkey',
    'Ss3ToVkey',
    's_csiMap',
    's_genericMap',
    's_ss3Map'
))
{
    if ($inputEngineText -match [regex]::Escape($legacySymbol))
    {
        throw "R09 parser ownership regression: legacy C++ key-map symbol returned after Rust promotion: $legacySymbol"
    }
}

foreach ($legacyModifierImplementation in @(
    'const auto vtParam = modifierParam - 1;',
    'WI_SetFlagIf(modifiers, SHIFT_PRESSED, WI_IsFlagSet(modifierParam, CsiMouseModifierCodes::Shift));',
    'if (id < CsiActionCodes::CSI_F1 || id > CsiActionCodes::CSI_F4)',
    'if (identifier <= GenericKeyIdentifiers::Next)'
))
{
    if ($inputEngineText -match [regex]::Escape($legacyModifierImplementation))
    {
        throw "R09 parser ownership regression: portable modifier composition returned to C++: $legacyModifierImplementation"
    }
}

foreach ($legacyWin32KeyNormalization in @(
    'parameters.at(3).value_or(0)',
    'parameters.at(5).value_or(1)',
    'parameters.at(0).value_or(0)',
    'parameters.at(1).value_or(0)',
    'parameters.at(2).value_or(0)',
    'parameters.at(4).value_or(0)'
))
{
    if ($win32KeyText -match [regex]::Escape($legacyWin32KeyNormalization))
    {
        throw "R09 parser ownership regression: portable Win32 key normalization returned to C++: $legacyWin32KeyNormalization"
    }
}

Write-Host 'R09 parser ownership gate passed: Base64, input key maps, modifier translation, enhanced-key modifier composition, Win32 key normalization, control-character classification, and SGR mouse deterministic decoding are Rust-owned; native product consumers route through canonical promoted owners and duplicate portable implementations are absent.'