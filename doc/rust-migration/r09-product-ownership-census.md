# R09 product ownership census

R09 changes the migration question from behavioral parity to product ownership. The canonical product remains the existing `CascadiaPackage` / `WindowsTerminal.vcxproj` graph. C++/WinRT, XAML, Win32, COM, DirectX/DWrite/GDI and packaging surfaces remain native where they are the legitimate platform or UI owner. Portable behavior already certified in Rust should no longer have a second C++ implementation compiled into the product.

The classifications used here are:

- **DELETE** — Rust owns the behavior and the C++ implementation is redundant.
- **KEEP-NATIVE/UI** — the surface legitimately owns Windows/platform/UI behavior.
- **SPLIT** — the file currently mixes portable behavior with a required native seam; portable ownership should move behind the Rust boundary while the seam remains native.
- **UNRESOLVED** — more build/ownership evidence is required before changing the surface.

## Certified product owners

| Surface / behavior | Current classification | Evidence / next action |
|---|---|---|
| Base64 decoding | **DELETE C++ / Rust owner** | `src/terminal/parser/base64.cpp` is deleted. `base64.hpp` remains a narrow ABI-compatible facade over `terminal_parser_ffi_base64_decode_utf16`. |
| Input CSI cursor key mapping | **DELETE C++ / Rust owner** | Product routing uses `terminal_parser_ffi_input_cursor_vkey`; the legacy C++ map/table is absent and guarded by `Test-R09ParserOwnership.ps1`. |
| Input CSI generic key mapping | **DELETE C++ / Rust owner** | Product routing uses `terminal_parser_ffi_input_generic_vkey`; the legacy C++ map/table is absent and guarded. |
| Input SS3 key mapping | **DELETE C++ / Rust owner** | Product routing uses `terminal_parser_ffi_input_ss3_vkey`; the legacy C++ map/table is absent and guarded. |
| VT modifier normalization / enhanced-key composition | **DELETE C++ / Rust owner** | Cursor, generic and SGR modifier translation route through `terminal-parser-ffi`; duplicate C++ bit-composition patterns are prohibited by the ownership gate. |
| Win32 key-field normalization | **DELETE portable C++ / Rust owner** | `_GenerateWin32Key` remains as the native `INPUT_RECORD` adapter, while deterministic parameter/default/saturation policy routes through `terminal_parser_ffi_input_win32_key_fields`. |
| Control-character classification | **DELETE portable C++ / Rust owner** | `_DoControlCharacter` routes Ctrl+C/C0/DEL/print classification through `terminal_parser_ffi_input_control_character_plan`; C++ retains only keyboard-layout and Win32 event materialization. |
| SGR mouse button/state decoding | **DELETE portable C++ / Rust owner** | `_UpdateSGRMouseButtonState` routes deterministic press/release/drag/wheel and persistent button state through `terminal_parser_ffi_input_sgr_mouse_plan`; C++ retains only double-click timing/position history and Windows event materialization. The obsolete C++ decision tree is guarded by `Test-R09ParserOwnership.ps1`. Canonical `CascadiaPackage x64 Debug` passed in R09 Product Build #85 on `0fd9e1b80fc74c91672a32a4ac15eb5a8eaafbe9`. |
| Command-palette FZF matching | **DELETE portable C++ / Rust owner** | `CommandPalette`/`FilteredCommand` still expose the product/UI shape, while `fzf.cpp` is a narrow adapter over `terminal-app-ffi`; score and UTF-16 runs come from Rust. `Test-R09FzfOwnership.ps1` guards the real consumer route. |

## Remaining parser product surfaces

| Surface | Current classification | Evidence / next action |
|---|---|---|
| `rust/terminal-parser-ffi` | **KEEP-ABI** | Narrow C ABI/static-library boundary. Raw pointer handling stays here; product semantics stay in safe Rust crates. |
| `src/terminal/parser/InputStateMachineEngine.cpp` | **SPLIT** | Multiple deterministic input behaviors are already Rust-owned. The remaining translation unit still owns legitimate keyboard-layout, timing, `INPUT_RECORD`, dispatch and other native integration seams. Promote only individually certified portable decisions. |
| `src/terminal/parser/OutputStateMachineEngine.cpp` | **SPLIT** | Deterministic parser/dispatch policy is represented in Rust; existing C++ surface still participates in native `ITermDispatch` product integration. Promote narrow behaviors before attempting translation-unit removal. |
| `src/terminal/parser/stateMachine.cpp` | **SPLIT** | Rust owns portable state-machine semantics, but C++ consumers still depend on the native class/API shape. Replace ownership through product seams before removing the implementation. |
| `src/terminal/parser/tracing.cpp` | **KEEP-NATIVE** | Telemetry/tracing is a platform integration concern, not a reason to duplicate parser semantics in Rust. |
| `src/terminal/parser/precomp.cpp` / `precomp.h` | **KEEP-NATIVE** | Native build infrastructure while C++ parser seams remain. |

## Mechanical promotion rule

A surface moves from `SPLIT` to `DELETE` only after all of the following are true:

1. the existing product consumer routes the relevant behavior through the Rust owner;
2. Rust and Microsoft contract tests remain green;
3. the canonical `CascadiaPackage` product build remains green;
4. the redundant C++ implementation is removed from the product path or reduced to a native/ABI adapter;
5. a repository ownership gate prevents the obsolete implementation from being reintroduced.

`tools/rust/Test-R09ParserOwnership.ps1` records the parser promotions already completed: Base64, CSI/generic/SS3 key maps, modifier translation and enhanced-key composition, Win32 key normalization, control-character classification, and SGR mouse deterministic state. It verifies that product C++ consumers still route through those Rust owners and that known legacy implementations remain absent.

## Next parser candidate

The next low-blast-radius candidate is the deterministic C0 dispatch plan in `OutputStateMachineEngine::ActionExecute`. The current C++ method maps ENQ/BEL/BS/TAB/CR/LF/FF/VT/SI/SO/SUB/DEL to semantic `ITermDispatch` operations and ignores other controls; the Rust output engine already reproduces the same mapping and has a direct contract test covering all mapped controls.

Treat this as **SPLIT** until product ownership changes. The intended boundary is a narrow Rust plan from one UTF-16 code unit to a small semantic action plus argument. C++ should retain only the actual `ITermDispatch` calls and `_ClearLastChar()`. Before routing the product, reuse one Rust decision function from both the Rust output engine and the C ABI so the migration does not create two Rust implementations of the same table; then replay the C ABI from native code, switch `ActionExecute`, remove the C++ switch, guard against reintroduction, and recertify `CascadiaPackage`.
