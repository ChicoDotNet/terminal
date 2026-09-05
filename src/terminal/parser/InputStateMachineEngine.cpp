// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "stateMachine.hpp"
#include "InputStateMachineEngine.hpp"
#include "terminal_parser_ffi.h"

#include <til/atomic.h>

#include "../../inc/unicode.hpp"
#include "../../interactivity/inc/VtApiRedirection.hpp"

using namespace Microsoft::Console::VirtualTerminal;

InputStateMachineEngine::InputStateMachineEngine(std::unique_ptr<IInteractDispatch> pDispatch) :
    _pDispatch(std::move(pDispatch)),
    _doubleClickTime(std::chrono::milliseconds(GetDoubleClickTime()))
{
    THROW_HR_IF_NULL(E_INVALIDARG, _pDispatch.get());
}

void InputStateMachineEngine::CaptureNextCursorPositionReport() noexcept
{
    _captureNextCursorPositionReport.store(true, std::memory_order_relaxed);
}

til::enumset<DeviceAttribute, uint64_t> InputStateMachineEngine::WaitUntilDA1(DWORD timeout) noexcept
{
    uint64_t val = 0;

    // atomic_wait() returns false when the timeout expires.
    // Technically we should decrement the timeout with each iteration,
    // but I suspect infinite spurious wake-ups are a theoretical problem.
    for (;;)
    {
        val = _deviceAttributes.load(std::memory_order_relaxed);
        if (val)
        {
            break;
        }

        if (!til::atomic_wait(_deviceAttributes, val, timeout))
        {
            break;
        }
    }

    // VtIo first sends a DSR CPR and then a DA1 request.
    // If we encountered a DA1 response here, the DSR request is definitely done now.
    _captureNextCursorPositionReport.store(false, std::memory_order_relaxed);

    return til::enumset<DeviceAttribute, uint64_t>::from_bits(val);
}

void InputStateMachineEngine::UnknownSequence() noexcept
{
}

bool InputStateMachineEngine::EncounteredWin32InputModeSequence() const noexcept
{
    return _encounteredWin32InputModeSequence;
}

// Method Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionExecute(const wchar_t wch)
{
    return _DoControlCharacter(wch, false);
}

// Routine Description:
// - Writes a control character into the buffer. Think characters like tab, backspace, etc.
// Arguments:
// - wch - the character to write
// - writeAlt - Pass in the alt-state information here as it's not embedded
// Return Value:
// - True if successfully generated and written. False otherwise.
bool InputStateMachineEngine::_DoControlCharacter(const wchar_t wch, const bool writeAlt)
{
    terminal_parser_ffi_control_character_plan plan{};
    const auto status = terminal_parser_ffi_input_control_character_plan(
        gsl::narrow_cast<uint16_t>(wch),
        writeAlt ? 1u : 0u,
        &plan);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_PARSER_FFI_OK);

    switch (plan.kind)
    {
    case TERMINAL_PARSER_FFI_CONTROL_CHARACTER_CTRL_C:
    {
        const auto virtualKey = gsl::narrow_cast<short>(plan.forced_virtual_key);
        const auto character = static_cast<wchar_t>(plan.character);
        const auto modifierState = plan.write_ctrl != 0 ? LEFT_CTRL_PRESSED : 0;
        const auto keyDown = SynthesizeKeyEvent(true, 1, virtualKey, 0, character, modifierState);
        const auto keyUp = SynthesizeKeyEvent(false, 1, virtualKey, 0, character, modifierState);
        _pDispatch->WriteCtrlKey(keyDown);
        _pDispatch->WriteCtrlKey(keyUp);
        break;
    }
    case TERMINAL_PARSER_FFI_CONTROL_CHARACTER_MAPPED_C0:
    {
        const auto actualChar = static_cast<wchar_t>(plan.character);
        short vkey = gsl::narrow_cast<short>(plan.forced_virtual_key);
        DWORD modifierState = 0;
        auto success = plan.forced_virtual_key != 0;
        if (!success)
        {
            success = _GenerateKeyFromChar(actualChar, vkey, modifierState);
        }
        if (plan.clear_layout_modifiers != 0)
        {
            modifierState = 0;
        }

        if (success)
        {
            if (plan.write_ctrl != 0)
            {
                WI_SetFlag(modifierState, LEFT_CTRL_PRESSED);
            }
            if (writeAlt)
            {
                WI_SetFlag(modifierState, LEFT_ALT_PRESSED);
            }

            _WriteSingleKey(actualChar, vkey, modifierState);
        }
        break;
    }
    case TERMINAL_PARSER_FFI_CONTROL_CHARACTER_DELETE_AS_BACKSPACE:
        _WriteSingleKey(
            static_cast<wchar_t>(plan.character),
            gsl::narrow_cast<short>(plan.forced_virtual_key),
            writeAlt ? LEFT_ALT_PRESSED : 0);
        break;
    case TERMINAL_PARSER_FFI_CONTROL_CHARACTER_PRINT:
        ActionPrint(static_cast<wchar_t>(plan.character));
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    return true;
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// This is called from the Escape state in the state machine, indicating the
//      immediately previous character was an 0x1b.
// We need to override this method to properly treat 0x1b + C0 strings as
//      Ctrl+Alt+<char> input sequences.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionExecuteFromEscape(const wchar_t wch)
{
    if (_pDispatch->IsVtInputEnabled())
    {
        return false;
    }

    return _DoControlCharacter(wch, true);
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      character given.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPrint(const wchar_t wch)
{
    short vkey = 0;
    DWORD modifierState = 0;
    if (_GenerateKeyFromChar(wch, vkey, modifierState))
    {
        _WriteSingleKey(wch, vkey, modifierState);
    }
    return true;
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - string - string to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPrintString(const std::wstring_view string)
{
    if (!string.empty())
    {
        _pDispatch->WriteString(string);
    }
    return true;
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - string - string to dispatch.
// - flush - not applicable to the input state machine.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPassThroughString(const std::wstring_view string)
{
    if (!string.empty())
    {
        _pDispatch->WriteStringRaw(string);
    }
    return true;
}

// Method Description:
// - Triggers the EscDispatch action to indicate that the listener should handle
//      a simple escape sequence. These sequences traditionally start with ESC
//      and a simple letter. No complicated parameters.
// Arguments:
// - id - Identifier of the escape sequence to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionEscDispatch(const VTID id)
{
    // If the _expectingStringTerminator flag is set, that means we've been
    // processing a DCS sequence and are waiting for the string terminator.
    // Once we receive the ST sequence here, we can return false to force the
    // buffered DCS content to be flushed.
    if (_expectingStringTerminator && id == VTID("\\"))
    {
        _expectingStringTerminator = false;
        return false;
    }

    if (_pDispatch->IsVtInputEnabled())
    {
        return false;
    }

    // There are no intermediates, so the id is effectively the final char.
    const auto wch = gsl::narrow_cast<wchar_t>(id);

    // 0x7f is DEL, which we treat effectively the same as a ctrl character.
    if (wch == 0x7f)
    {
        _DoControlCharacter(wch, true);
    }
    else
    {
        DWORD modifierState = 0;
        short vk = 0;
        if (_GenerateKeyFromChar(wch, vk, modifierState))
        {
            // Alt is definitely pressed in the esc+key case.
            modifierState = WI_SetFlag(modifierState, LEFT_ALT_PRESSED);
            _WriteSingleKey(wch, vk, modifierState);
        }
    }

    return true;
}

// Method Description:
// - Triggers the Vt52EscDispatch action to indicate that the listener should handle
//      a VT52 escape sequence. These sequences start with ESC and a single letter,
//      sometimes followed by parameters.
// Arguments:
// - id - Identifier of the VT52 sequence to dispatch.
// - parameters - Set of parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionVt52EscDispatch(const VTID /*id*/, const VTParameters /*parameters*/) noexcept
{
    // VT52 escape sequences are not used in the input state machine.
    return false;
}

// Method Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - id - Identifier of the escape sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)
{
    // GH#4999 - If the client was in VT input mode, but we received a
    // win32-input-mode sequence, then _don't_ passthrough the sequence to the
    // client. It's impossibly unlikely that the client actually wanted
    // win32-input-mode, and if they did, then we'll just translate the
    // INPUT_RECORD back to the same sequence we say here later on, when the
    // client reads it.
    //
    // Focus events in conpty are special, so don't flush those through either.
    // See GH#12799, GH#12900 for details
    const auto vtInputEnabled = _pDispatch->IsVtInputEnabled();

    switch (id)
    {
    case CsiActionCodes::MouseDown:
    case CsiActionCodes::MouseUp:
    {
        if (vtInputEnabled)
        {
            return false;
        }

        DWORD buttonState = 0;
        DWORD eventFlags = 0;
        const auto firstParameter = parameters.at(0).value_or(0);
        const til::point uiPos{ parameters.at(1) - 1, parameters.at(2) - 1 };

        if (_UpdateSGRMouseButtonState(id, firstParameter, buttonState, eventFlags, uiPos))
        {
            const auto modifierState = _GetSGRMouseModifierState(firstParameter);
            _WriteMouseEvent(uiPos, buttonState, modifierState, eventFlags);
        }
        return true;
    }
    // case CsiActionCodes::DSR_DeviceStatusReportResponse:
    case CsiActionCodes::CSI_F3:
        // The F3 case is special - it shares a code with the DeviceStatusResponse.
        // If we're looking for that response, then do that, and break out.
        // Else, fall though to the _GetCursorKeysModifierState handler.
        if (_captureNextCursorPositionReport.exchange(false, std::memory_order_relaxed))
        {
            _pDispatch->MoveCursor(parameters.at(0), parameters.at(1));
            return true;
        }
        // Heuristic: If the hosting terminal used the win32 input mode, chances are high
        // that this is a CPR requested by the terminal application as opposed to a F3 key.
        if (_encounteredWin32InputModeSequence)
        {
            return false;
        }
        if (vtInputEnabled)
        {
            return false;
        }
        [[fallthrough]];
    case CsiActionCodes::ArrowUp:
    case CsiActionCodes::ArrowDown:
    case CsiActionCodes::ArrowRight:
    case CsiActionCodes::ArrowLeft:
    case CsiActionCodes::Home:
    case CsiActionCodes::End:
    case CsiActionCodes::CSI_F1:
    case CsiActionCodes::CSI_F2:
    case CsiActionCodes::CSI_F4:
    {
        if (vtInputEnabled)
        {
            return false;
        }
        short vkey = 0;
        if (_GetCursorKeysVkey(id, vkey))
        {
            const auto modifierState = _GetCursorKeysModifierState(parameters, id);
            _WriteSingleKey(vkey, modifierState);
        }
        return true;
    }
    case CsiActionCodes::Generic:
    {
        if (vtInputEnabled)
        {
            return false;
        }
        short vkey = 0;
        if (_GetGenericVkey(parameters.at(0), vkey))
        {
            const auto modifierState = _GetGenericKeysModifierState(parameters);
            _WriteSingleKey(vkey, modifierState);
        }
        return true;
    }
    case CsiActionCodes::CursorBackTab:
        if (vtInputEnabled)
        {
            return false;
        }
        _WriteSingleKey(VK_TAB, SHIFT_PRESSED);
        return true;
    case CsiActionCodes::FocusIn:
        _pDispatch->FocusChanged(true);
        return true;
    case CsiActionCodes::FocusOut:
        _pDispatch->FocusChanged(false);
        return true;
    case CsiActionCodes::DA_DeviceAttributes:
        // This assumes that InputStateMachineEngine is tightly coupled with VtInputThread and the rest of the ConPTY system (VtIo).
        // On startup, ConPTY will send a DA1 request to get more information about the hosting terminal.
        // We catch it here and store the information for later retrieval.
        if (_deviceAttributes.load(std::memory_order_relaxed) == 0)
        {
            til::enumset<DeviceAttribute, uint64_t> attributes{ DeviceAttribute::__some__ };

            // The first parameter denotes the conformance level.
            const auto len = parameters.size();
            if (len >= 2 && parameters.at(0).value() >= 61)
            {
                // NOTE: VTParameters::for_each will replace empty spans with a single default value.
                // This means we could not distinguish between no parameters and a single default parameter.
                for (size_t i = 1; i < len; i++)
                {
                    const auto value = parameters.at(i).value();
                    if (value > 0 && value < 64)
                    {
                        attributes.set(static_cast<DeviceAttribute>(value));
                    }
                }
            }

            _deviceAttributes.fetch_or(attributes.bits(), std::memory_order_relaxed);
            til::atomic_notify_all(_deviceAttributes);
            return true;
        }
        return false;
    case CsiActionCodes::Win32KeyboardInput:
    {
        // Use WriteCtrlKey here, even for keys that _aren't_ control keys,
        // because that will take extra steps to make sure things like
        // Ctrl+C, Ctrl+Break are handled correctly.
        const auto key = _GenerateWin32Key(parameters);
        _pDispatch->WriteCtrlKey(key);
        _encounteredWin32InputModeSequence = true;
        return true;
    }
    default:
        return false;
    }
}

// Routine Description:
// - Triggers the DcsDispatch action to indicate that the listener should handle
//      a control sequence. Returns the handler function that is to be used to
//      process the subsequent data string characters in the sequence.
// Arguments:
// - id - Identifier of the escape sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - the data string handler function or nullptr if the sequence is not supported
IStateMachineEngine::StringHandler InputStateMachineEngine::ActionDcsDispatch(const VTID /*id*/, const VTParameters /*parameters*/) noexcept
{
    // Returning a nullptr here will cause the content of the DCS sequence to be
    // ignored, but it'll still be buffered by the state machine, so we can flush
    // the whole thing once we receive the string terminator.
    _expectingStringTerminator = true;
    return nullptr;
}

// Routine Description:
// - Triggers the Ss3Dispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionSs3Dispatch(const wchar_t wch, const VTParameters /*parameters*/)
{
    if (_pDispatch->IsVtInputEnabled())
    {
        return false;
    }

    // Ss3 sequence keys aren't modified.
    // When F1-F4 *are* modified, they're sent as CSI sequences, not SS3's.
    short vkey = 0;
    if (_GetSs3KeysVkey(wch, vkey))
    {
        _WriteSingleKey(vkey, 0);
    }

    return true;
}

// Method Description:
// - Triggers the OscDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands.
// Arguments:
// - parameter - identifier of the OSC action to perform
// - string - OSC string we've collected. NOT null terminated.
// Return Value:
// - true if we handled the dispatch.
bool InputStateMachineEngine::ActionOscDispatch(const size_t /*parameter*/, const std::wstring_view /*string*/) noexcept
{
    // Unlike ActionCsiDispatch, we are not checking whether the application has requested
    // VT input.
    // Our documentation states that VT reports generated by application requests will be
    // sent regardless of the state of `ENABLE_VIRTUAL_TERMINAL_INPUT`. We can't easily do
    // that for CSI reports because we may incidentally pass through non-response VT input;
    // however, there should be no OSC on the input stream *except* for responses.
    // It should be safe to pass all OSCs from the input stream through to the application.
    return false;
}

// Method Description:
// - Writes a sequence of keypresses to the buffer based on the wch,
//      vkey and modifiers passed in. Will create both the appropriate key downs
//      and ups for that key for writing to the input. Will also generate
//      keypresses for pressing the modifier keys while typing that character.
//  If rgInput isn't big enough, then it will stop writing when it's filled.
// Arguments:
// - wch - the character to write to the input callback.
// - vkey - the VKEY of the key to write to the input callback.
// - modifierState - the modifier state to write with the key.
// - input - the buffer of characters to write the keypresses to. Can write
//      up to 8 records to this buffer.
// Return Value:
// - the number of records written, or 0 if the buffer wasn't big enough.
void InputStateMachineEngine::_GenerateWrappedSequence(const wchar_t wch,
                                                       const short vkey,
                                                       const DWORD modifierState,
                                                       InputEventQueue& input)
{
    input.reserve(input.size() + 8);

    // TODO: Reuse the clipboard functions for generating input for characters
    //       that aren't on the current keyboard.
    // MSFT:13994942

    const auto shift = WI_IsFlagSet(modifierState, SHIFT_PRESSED);
    const auto ctrl = WI_IsFlagSet(modifierState, LEFT_CTRL_PRESSED);
    const auto alt = WI_IsFlagSet(modifierState, LEFT_ALT_PRESSED);

    auto next = SynthesizeKeyEvent(true, 1, 0, 0, 0, 0);
    DWORD currentModifiers = 0;

    if (shift)
    {
        WI_SetFlag(currentModifiers, SHIFT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (alt)
    {
        WI_SetFlag(currentModifiers, LEFT_ALT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_MENU, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (ctrl)
    {
        WI_SetFlag(currentModifiers, LEFT_CTRL_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }

    // Use modifierState instead of currentModifiers here.
    // This allows other modifiers like ENHANCED_KEY to get
    //    through on the KeyPress.
    _GetSingleKeypress(wch, vkey, modifierState, input);

    next.Event.KeyEvent.bKeyDown = FALSE;

    if (ctrl)
    {
        WI_ClearFlag(currentModifiers, LEFT_CTRL_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (alt)
    {
        WI_ClearFlag(currentModifiers, LEFT_ALT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_MENU, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (shift)
    {
        WI_ClearFlag(currentModifiers, SHIFT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
}

// Method Description:
// - Writes a single character keypress to the input buffer. This writes both
//      the keydown and keyup events.
// Arguments:
// - wch - the character to write to the buffer.
// - vkey - the VKEY of the key to write to the buffer.
// - modifierState - the modifier state to write with the key.
// - input - the buffer of characters to write the keypress to. Will always
//      write to the first two positions in the buffer.
// Return Value:
// - the number of input records written.
void InputStateMachineEngine::_GetSingleKeypress(const wchar_t wch,
                                                 const short vkey,
                                                 const DWORD modifierState,
                                                 InputEventQueue& input)
{
    input.reserve(input.size() + 2);

    const auto sc = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC));
    auto rec = SynthesizeKeyEvent(true, 1, vkey, sc, wch, modifierState);

    input.push_back(rec);
    rec.Event.KeyEvent.bKeyDown = FALSE;
    input.push_back(rec);
}

// Method Description:
// - Writes a sequence of keypresses to the input callback based on the wch,
//      vkey and modifiers passed in. Will create both the appropriate key downs
//      and ups for that key for writing to the input. Will also generate
//      keypresses for pressing the modifier keys while typing that character.
// Arguments:
// - wch - the character to write to the input callback.
// - vkey - the VKEY of the key to write to the input callback.
// - modifierState - the modifier state to write with the key.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
void InputStateMachineEngine::_WriteSingleKey(const wchar_t wch, const short vkey, const DWORD modifierState)
{
    // At most 8 records - 2 for each of shift,ctrl,alt up and down, and 2 for the actual key up and down.
    InputEventQueue inputEvents;
    _GenerateWrappedSequence(wch, vkey, modifierState, inputEvents);
    _pDispatch->WriteInput(inputEvents);
}

// Method Description:
// - Helper for writing a single key to the input when you only know the vkey.
//      Will automatically get the wchar_t associated with that vkey.
// Arguments:
// - vkey - the VKEY of the key to write to the input callback.
// - modifierState - the modifier state to write with the key.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
void InputStateMachineEngine::_WriteSingleKey(const short vkey, const DWORD modifierState)
{
    const auto wch = gsl::narrow_cast<wchar_t>(OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR));
    _WriteSingleKey(wch, vkey, modifierState);
}

// Method Description:
// - Writes a Mouse Event Record to the input callback based on the state of the mouse.
// Arguments:
// - column - the X/Column position on the viewport (0 = left-most)
// - line - the Y/Line/Row position on the viewport (0 = top-most)
// - buttonState - the mouse buttons that are being modified
// - modifierState - the modifier state to write mouse record.
// - eventFlags - the type of mouse event to write to the mouse record.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
void InputStateMachineEngine::_WriteMouseEvent(const til::point uiPos, const DWORD buttonState, const DWORD controlKeyState, const DWORD eventFlags)
{
    const auto rgInput = SynthesizeMouseEvent(uiPos, buttonState, controlKeyState, eventFlags);
    _pDispatch->WriteInput({ &rgInput, 1 });
}

// Method Description:
// - Retrieves the modifier state from a set of parameters for a cursor keys
//      sequence. This is for Arrow keys, Home, End, etc.
// Arguments:
// - parameters - the set of parameters to get the modifier state from.
// - id - the identifier for the sequence we're operating on.
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetCursorKeysModifierState(const VTParameters parameters, const VTID id) noexcept
{
    return terminal_parser_ffi_input_cursor_modifier_state(
        gsl::narrow_cast<uint16_t>(id),
        gsl::narrow_cast<uint32_t>(parameters.at(1)));
}

// Method Description:
// - Retrieves the modifier state from a set of parameters for a "Generic"
//      keypress - one who's sequence is terminated with a '~'.
// Arguments:
// - parameters - the set of parameters to get the modifier state from.
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetGenericKeysModifierState(const VTParameters parameters) noexcept
{
    return terminal_parser_ffi_input_generic_modifier_state(
        static_cast<int32_t>(parameters.at(0)),
        gsl::narrow_cast<uint32_t>(parameters.at(1)));
}

// Method Description:
// - Retrieves the modifier state from the first parameter of an SGR
//      Mouse Sequence - one who's sequence is terminated with an 'M' or 'm'.
// Arguments:
// - modifierParam - the first parameter to get the modifier state from.
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetSGRMouseModifierState(const size_t modifierParam) noexcept
{
    return terminal_parser_ffi_input_sgr_mouse_modifier_state(gsl::narrow_cast<uint32_t>(modifierParam));
}

// Method Description:
// - Converts a VT encoded modifier param into a INPUT_RECORD compatible one.
// Arguments:
// - modifierParam - the VT modifier value to convert
// Return Value:
// - The equivalent INPUT_RECORD modifier value.
DWORD InputStateMachineEngine::_GetModifier(const size_t modifierParam) noexcept
{
    return terminal_parser_ffi_input_vt_modifier_state(gsl::narrow_cast<uint32_t>(modifierParam));
}

// Method Description:
// - Synthesize the button state for the Mouse Input Record from an SGR VT Sequence
// - Rust owns deterministic SGR decoding and persistent button state.
// - Native code retains only double-click tracking, which depends on time and position.
// Arguments:
// - id: the sequence identifier representing whether the button was pressed or released
// - sgrEncoding: the first parameter, encoding the button and drag state
// - buttonState: Receives the button state for the record
// - eventFlags: Receives the special mouse events for the record
// Return Value:
// true iff we were able to synthesize buttonState
bool InputStateMachineEngine::_UpdateSGRMouseButtonState(const VTID id,
                                                         const size_t sgrEncoding,
                                                         DWORD& buttonState,
                                                         DWORD& eventFlags,
                                                         const til::point uiPos)
{
    const auto buttonDown = id == CsiActionCodes::MouseDown;
    if (!buttonDown && id != CsiActionCodes::MouseUp)
    {
        return false;
    }

    terminal_parser_ffi_sgr_mouse_plan plan{};
    const auto status = terminal_parser_ffi_input_sgr_mouse_plan(
        _mouseButtonState,
        gsl::narrow_cast<uint32_t>(sgrEncoding),
        buttonDown ? 1u : 0u,
        &plan);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_PARSER_FFI_OK);
    if (plan.valid == 0)
    {
        return false;
    }

    buttonState = plan.button_state;
    eventFlags = plan.event_flags;

    if (plan.track_click != 0)
    {
        const auto currentTime = std::chrono::steady_clock::now();
        if (_lastMouseClickPos && _lastMouseClickTime && _lastMouseClickButton &&
            uiPos == _lastMouseClickPos &&
            (currentTime - _lastMouseClickTime.value()) < _doubleClickTime &&
            plan.button_id == _lastMouseClickButton)
        {
            eventFlags |= DOUBLE_CLICK;
            _lastMouseClickPos.reset();
            _lastMouseClickTime.reset();
            _lastMouseClickButton.reset();
        }
        else
        {
            _lastMouseClickPos = uiPos;
            _lastMouseClickTime = currentTime;
            _lastMouseClickButton = plan.button_id;
        }
    }

    _mouseButtonState = plan.persistent_button_state;
    return true;
}

// Method Description:
// - Gets the Vkey form the generic keys table associated with a particular
//   identifier code.
// Arguments:
// - identifier: the identifier of the key we're looking for.
// - vkey: Receives the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetGenericVkey(const GenericKeyIdentifiers identifier, short& vkey) const
{
    const auto mapped = terminal_parser_ffi_input_generic_vkey(static_cast<int32_t>(identifier));
    vkey = gsl::narrow_cast<short>(mapped);
    return mapped != 0;
}

// Method Description:
// - Gets the Vkey from the CSI codes table associated with a particular character.
// Arguments:
// - id: the sequence identifier to get the mapped vkey of.
// - vkey: Receives the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetCursorKeysVkey(const VTID id, short& vkey) const
{
    const auto mapped = terminal_parser_ffi_input_cursor_vkey(gsl::narrow_cast<uint16_t>(id));
    vkey = gsl::narrow_cast<short>(mapped);
    return mapped != 0;
}

// Method Description:
// - Gets the Vkey from the SS3 codes table associated with a particular character.
// Arguments:
// - wch: the wchar_t to get the vkey and modifier state of.
// - pVkey: Receives the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetSs3KeysVkey(const wchar_t wch, short& vkey) const
{
    const auto mapped = terminal_parser_ffi_input_ss3_vkey(gsl::narrow_cast<uint16_t>(wch));
    vkey = gsl::narrow_cast<short>(mapped);
    return mapped != 0;
}

// Method Description:
// - Gets the Vkey and modifier state that's associated with a particular char.
// Arguments:
// - wch: the wchar_t to get the vkey and modifier state of.
// - vkey: Receives the vkey
// - modifierState: Receives the modifier state
// Return Value:
// <none>
bool InputStateMachineEngine::_GenerateKeyFromChar(const wchar_t wch,
                                                   short& vkey,
                                                   DWORD& modifierState) noexcept
{
    // Low order byte is key, high order is modifiers
    const auto keyscan = OneCoreSafeVkKeyScanW(wch);

    short key = LOBYTE(keyscan);

    const short keyscanModifiers = HIBYTE(keyscan);

    if (key == -1 && keyscanModifiers == -1)
    {
        return false;
    }

    // Because of course, these are not the same flags.
    short modifierFlags = 0 |
                          (WI_IsFlagSet(keyscanModifiers, KEYSCAN_SHIFT) ? SHIFT_PRESSED : 0) |
                          (WI_IsFlagSet(keyscanModifiers, KEYSCAN_CTRL) ? LEFT_CTRL_PRESSED : 0) |
                          (WI_IsFlagSet(keyscanModifiers, KEYSCAN_ALT) ? LEFT_ALT_PRESSED : 0);

    vkey = key;
    modifierState = modifierFlags;

    return true;
}

// Method Description:
// - Retrieves the type of window manipulation operation from the parameter pool
//      stored during Param actions.
//  This is kept separate from the output version, as there may be
//      codes that are supported in one direction but not the other.
// Arguments:
// - parameters - Array of parameters collected
// - function - Receives the function type
// Return Value:
// - True iff we successfully pulled the function type from the parameters
bool InputStateMachineEngine::_GetWindowManipulationType(const std::span<const size_t> parameters,
                                                         unsigned int& function) const noexcept
{
    auto success = false;
    function = DispatchTypes::WindowManipulationType::Invalid;

    if (!parameters.empty())
    {
        switch (til::at(parameters, 0))
        {
        case DispatchTypes::WindowManipulationType::RefreshWindow:
            function = DispatchTypes::WindowManipulationType::RefreshWindow;
            success = true;
            break;
        case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
            function = DispatchTypes::WindowManipulationType::ResizeWindowInCharacters;
            success = true;
            break;
        default:
            success = false;
        }
    }

    return success;
}

// Method Description:
// - Attempt to parse our parameters into a win32-input-mode serialized KeyEvent.
// Arguments:
// - parameters: the list of numbers to parse into values for the KeyEvent.
// Return Value:
// - The deserialized KeyEvent.
INPUT_RECORD InputStateMachineEngine::_GenerateWin32Key(const VTParameters& parameters)
{
    // Sequences are formatted as follows:
    //
    // ^[ [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    //
    //      Vk: the value of wVirtualKeyCode - any number. If omitted, defaults to '0'.
    //      Sc: the value of wVirtualScanCode - any number. If omitted, defaults to '0'.
    //      Uc: the decimal value of UnicodeChar - for example, NUL is "0", LF is
    //          "10", the character 'A' is "65". If omitted, defaults to '0'.
    //      Kd: the value of bKeyDown - either a '0' or '1'. If omitted, defaults to '0'.
    //      Cs: the value of dwControlKeyState - any number. If omitted, defaults to '0'.
    //      Rc: the value of wRepeatCount - any number. If omitted, defaults to '1'.
    uint32_t presentMask = 0;
    int32_t values[6]{};
    for (size_t index = 0; index < 6; ++index)
    {
        const auto parameter = parameters.at(index);
        if (parameter.has_value())
        {
            presentMask |= 1u << index;
            values[index] = parameter.value();
        }
    }

    terminal_parser_ffi_key_event key{};
    const auto status = terminal_parser_ffi_input_win32_key_fields(
        presentMask,
        values[0],
        values[1],
        values[2],
        values[3],
        values[4],
        values[5],
        &key);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_PARSER_FFI_OK);

    return SynthesizeKeyEvent(
        key.key_down != 0,
        key.repeat_count,
        key.virtual_key,
        key.scan_code,
        static_cast<wchar_t>(key.unicode_char),
        key.control_key_state);
}