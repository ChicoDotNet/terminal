// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "OutputStateMachineEngine.hpp"

#include <conattrs.hpp>

#include "ascii.hpp"
#include "base64.hpp"
#include "stateMachine.hpp"
#include "terminal_parser_ffi.h"
#include "terminal_parser_ffi_output_esc.h"
#include "terminal_parser_ffi_output_vt52.h"
#include "terminal_parser_ffi_output_csi_cursor.h"
#include "terminal_parser_ffi_output_csi_margins.h"
#include "terminal_parser_ffi_output_csi_edit.h"
#include "terminal_parser_ffi_output_csi_line_edit.h"
#include "terminal_parser_ffi_output_csi_erase_characters.h"
#include "terminal_parser_ffi_output_csi_scroll.h"
#include "terminal_parser_ffi_output_csi_page.h"
#include "terminal_parser_ffi_output_csi_page_position.h"
#include "terminal_parser_ffi_output_csi_tab.h"
#include "terminal_parser_ffi_output_csi_terminal_parameters.h"
#include "terminal_parser_ffi_output_csi_device_attributes.h"
#include "terminal_parser_ffi_output_csi_cursor_restore.h"
#include "terminal_parser_ffi_output_csi_soft_reset.h"
#include "terminal_parser_ffi_output_csi_displayed_extent.h"
#include "terminal_parser_ffi_output_csi_cursor_style.h"
#include "terminal_parser_ffi_output_csi_request_mode.h"
#include "terminal_parser_ffi_output_csi_device_status_report.h"
#include "terminal_parser_ffi_output_csi_mode.h"
#include "terminal_parser_ffi_output_csi_erase.h"
#include "terminal_parser_ffi_output_csi_tab_control.h"
#include "../../types/inc/utils.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;

constexpr COLORREF COLOR_INQUIRY_COLOR = 0xfeffffff; // It's like INVALID_COLOR but special

// takes ownership of pDispatch
OutputStateMachineEngine::OutputStateMachineEngine(std::unique_ptr<ITermDispatch> pDispatch) :
    _dispatch(std::move(pDispatch)),
    _lastPrintedChar(AsciiChars::NUL)
{
    THROW_HR_IF_NULL(E_INVALIDARG, _dispatch.get());
}

void OutputStateMachineEngine::UnknownSequence() noexcept
{
    _dispatch->UnknownSequence();
}

bool OutputStateMachineEngine::EncounteredWin32InputModeSequence() const noexcept
{
    return false;
}

const ITermDispatch& OutputStateMachineEngine::Dispatch() const noexcept
{
    return *_dispatch;
}

ITermDispatch& OutputStateMachineEngine::Dispatch() noexcept
{
    return *_dispatch;
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionExecute(const wchar_t wch)
{
    terminal_parser_ffi_output_execute_result plan{};
    const auto status = terminal_parser_ffi_output_execute_plan(gsl::narrow_cast<uint16_t>(wch), &plan);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_PARSER_FFI_OK);

    switch (plan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_ENQUIRE_ANSWERBACK:
        _dispatch->EnquireAnswerback();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_WARNING_BELL:
        _dispatch->WarningBell();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_CURSOR_BACKWARD:
        _dispatch->CursorBackward(plan.argument);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_FORWARD_TAB:
        _dispatch->ForwardTab(plan.argument);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_CARRIAGE_RETURN:
        _dispatch->CarriageReturn();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_LINE_FEED_DEPENDS_ON_MODE:
        _dispatch->LineFeed(DispatchTypes::LineFeedType::DependsOnMode);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_LOCKING_SHIFT:
        _dispatch->LockingShift(plan.argument);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_PRINT:
        _dispatch->Print(gsl::narrow_cast<wchar_t>(plan.argument));
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_EXECUTE_NONE:
    default:
        break;
    }

    _ClearLastChar();

    return true;
}
// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// This is called from the Escape state in the state machine, indicating the
//      immediately previous character was an 0x1b. The output state machine
//      does not treat this any differently than a normal ActionExecute.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionExecuteFromEscape(const wchar_t wch)
{
    return ActionExecute(wch);
}

// Routine Description:
// - Triggers the Print action to indicate that the listener should render the
//      character given.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPrint(const wchar_t wch)
{
    // Stash the last character of the string, if it's a graphical character
    if (wch >= AsciiChars::SPC)
    {
        _lastPrintedChar = wch;
    }

    _dispatch->Print(wch); // call print

    return true;
}

// Routine Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - string - string to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPrintString(const std::wstring_view string)
{
    if (string.empty())
    {
        return true;
    }

    // Stash the last character of the string, if it's a graphical character
    const auto wch = string.back();
    if (wch >= AsciiChars::SPC)
    {
        _lastPrintedChar = wch;
    }

    _dispatch->PrintString(string); // call print

    return true;
}

// Routine Description:
// This is called when we have determined that we don't understand a particular
//      sequence, or the adapter has determined that the string is intended for
//      the actual terminal (when we're acting as a pty).
// - Pass the string through to the target terminal application. If we're a pty,
//      then we'll have a TerminalConnection that we'll write the string to.
//      Otherwise, we're the terminal device, and we'll eat the string (because
//      we don't know what to do with it)
// Arguments:
// - string - string to dispatch.
// - flush - set to true if the string should be flushed immediately.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPassThroughString(const std::wstring_view /*string*/) noexcept
{
    return true;
}

// Routine Description:
// - Triggers the EscDispatch action to indicate that the listener should handle
//      a simple escape sequence. These sequences traditionally start with ESC
//      and a simple letter. No complicated parameters.
// Arguments:
// - id - Identifier of the escape sequence to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionEscDispatch(const VTID id)
{
    terminal_parser_ffi_output_esc_result plan{};
    const auto status = terminal_parser_ffi_output_esc_plan(static_cast<uint64_t>(id), &plan);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_PARSER_FFI_OK);

    switch (plan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_BACK_INDEX:
        _dispatch->BackIndex();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_CURSOR_SAVE_STATE:
        _dispatch->CursorSaveState();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_CURSOR_RESTORE_STATE:
        _dispatch->CursorRestoreState();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_FORWARD_INDEX:
        _dispatch->ForwardIndex();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_KEYPAD_MODE:
        _dispatch->SetKeypadMode(plan.argument != 0);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_LINE_FEED_WITH_RETURN:
        _dispatch->LineFeed(DispatchTypes::LineFeedType::WithReturn);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_LINE_FEED_WITHOUT_RETURN:
        _dispatch->LineFeed(DispatchTypes::LineFeedType::WithoutReturn);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_REVERSE_LINE_FEED:
        _dispatch->ReverseLineFeed();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_HORIZONTAL_TAB_SET:
        _dispatch->HorizontalTabSet();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_DEVICE_ATTRIBUTES_PRIMARY:
        _dispatch->DeviceAttributes();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_HARD_RESET:
        _dispatch->HardReset(true);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_SINGLE_SHIFT:
        _dispatch->SingleShift(plan.argument);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT:
        _dispatch->LockingShift(plan.argument);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_LOCKING_SHIFT_RIGHT:
        _dispatch->LockingShiftRight(plan.argument);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_ACCEPT_C1_CONTROLS:
        _dispatch->AcceptC1Controls(plan.argument != 0);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_SEND_C1_CONTROLS:
        _dispatch->SendC1Controls(plan.argument != 0);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_ANNOUNCE_CODE_STRUCTURE:
        _dispatch->AnnounceCodeStructure(plan.argument);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_SET_LINE_RENDITION:
        switch (plan.argument)
        {
        case TERMINAL_PARSER_FFI_OUTPUT_ESC_SINGLE_WIDTH:
            _dispatch->SetLineRendition(LineRendition::SingleWidth);
            break;
        case TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_WIDTH:
            _dispatch->SetLineRendition(LineRendition::DoubleWidth);
            break;
        case TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_HEIGHT_TOP:
            _dispatch->SetLineRendition(LineRendition::DoubleHeightTop);
            break;
        case TERMINAL_PARSER_FFI_OUTPUT_ESC_DOUBLE_HEIGHT_BOTTOM:
            _dispatch->SetLineRendition(LineRendition::DoubleHeightBottom);
            break;
        default:
            THROW_HR(E_UNEXPECTED);
        }
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_SCREEN_ALIGNMENT_PATTERN:
        _dispatch->ScreenAlignmentPattern();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_CODING_SYSTEM:
        _dispatch->DesignateCodingSystem(VTID{ plan.payload });
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_94_CHARSET:
        _dispatch->Designate94Charset(plan.argument, VTID{ plan.payload });
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_DESIGNATE_96_CHARSET:
        _dispatch->Designate96Charset(plan.argument, VTID{ plan.payload });
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_ESC_NONE:
    default:
        break;
    }

    _ClearLastChar();

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
bool OutputStateMachineEngine::ActionVt52EscDispatch(const VTID id, const VTParameters parameters)
{
    terminal_parser_ffi_output_vt52_result plan{};
    const auto status = terminal_parser_ffi_output_vt52_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        static_cast<int32_t>(parameters.at(1).value_or(0)),
        &plan);
    THROW_HR_IF(E_UNEXPECTED, status != TERMINAL_PARSER_FFI_OK);

    switch (plan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_UP:
        _dispatch->CursorUp(plan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_DOWN:
        _dispatch->CursorDown(plan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_FORWARD:
        _dispatch->CursorForward(plan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_BACKWARD:
        _dispatch->CursorBackward(plan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_DESIGNATE_94_CHARSET:
        _dispatch->Designate94Charset(plan.argument1, VTID{ plan.payload });
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_CURSOR_POSITION:
        _dispatch->CursorPosition(plan.argument1, plan.argument2);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_REVERSE_LINE_FEED:
        _dispatch->ReverseLineFeed();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_ERASE_IN_DISPLAY:
        _dispatch->EraseInDisplay(DispatchTypes::EraseType::ToEnd);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_ERASE_IN_LINE:
        _dispatch->EraseInLine(DispatchTypes::EraseType::ToEnd);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_DEVICE_ATTRIBUTES:
        _dispatch->Vt52DeviceAttributes();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_SET_KEYPAD_MODE:
        _dispatch->SetKeypadMode(plan.argument1 != 0);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_SET_ANSI_MODE:
        _dispatch->SetMode(DispatchTypes::ModeParams::DECANM_AnsiMode);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_VT52_NONE:
    default:
        break;
    }

    _ClearLastChar();

    return true;
}

// Routine Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - id - Identifier of the control sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)
{
    // Bail out if we receive subparameters, but we don't accept them in the sequence.
    if (parameters.hasSubParams() && !_CanSeqAcceptSubParam(id, parameters)) [[unlikely]]
    {
        return true;
    }

    terminal_parser_ffi_output_csi_cursor_result cursorPlan{};
    const auto cursorStatus = terminal_parser_ffi_output_csi_cursor_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        static_cast<int32_t>(parameters.at(1).value_or(0)),
        &cursorPlan);
    THROW_HR_IF(E_UNEXPECTED, cursorStatus != TERMINAL_PARSER_FFI_OK);

    switch (cursorPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_UP:
        _dispatch->CursorUp(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_DOWN:
        _dispatch->CursorDown(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_FORWARD:
        _dispatch->CursorForward(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_BACKWARD:
        _dispatch->CursorBackward(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NEXT_LINE:
        _dispatch->CursorNextLine(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_PREVIOUS_LINE:
        _dispatch->CursorPrevLine(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_HORIZONTAL_ABSOLUTE:
        _dispatch->CursorHorizontalPositionAbsolute(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_VERTICAL_ABSOLUTE:
        _dispatch->VerticalLinePositionAbsolute(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_HORIZONTAL_RELATIVE:
        _dispatch->HorizontalPositionRelative(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_VERTICAL_RELATIVE:
        _dispatch->VerticalPositionRelative(cursorPlan.argument1);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_POSITION:
        _dispatch->CursorPosition(cursorPlan.argument1, cursorPlan.argument2);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (cursorPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_margins_result marginsPlan{};
    const auto marginsStatus = terminal_parser_ffi_output_csi_margins_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        static_cast<int32_t>(parameters.at(1).value_or(0)),
        &marginsPlan);
    THROW_HR_IF(E_UNEXPECTED, marginsStatus != TERMINAL_PARSER_FFI_OK);

    switch (marginsPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_TOP_BOTTOM:
        _dispatch->SetTopBottomScrollingMargins(marginsPlan.first, marginsPlan.second);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_LEFT_RIGHT:
        _dispatch->SetLeftRightScrollingMargins(marginsPlan.first, marginsPlan.second);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (marginsPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_MARGINS_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_edit_result editPlan{};
    const auto editStatus = terminal_parser_ffi_output_csi_edit_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &editPlan);
    THROW_HR_IF(E_UNEXPECTED, editStatus != TERMINAL_PARSER_FFI_OK);

    switch (editPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_EDIT_INSERT_CHARACTER:
        _dispatch->InsertCharacter(editPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_EDIT_DELETE_CHARACTER:
        _dispatch->DeleteCharacter(editPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_EDIT_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (editPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_EDIT_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_line_edit_result lineEditPlan{};
    const auto lineEditStatus = terminal_parser_ffi_output_csi_line_edit_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &lineEditPlan);
    THROW_HR_IF(E_UNEXPECTED, lineEditStatus != TERMINAL_PARSER_FFI_OK);

    switch (lineEditPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_LINE_EDIT_INSERT_LINE:
        _dispatch->InsertLine(lineEditPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_LINE_EDIT_DELETE_LINE:
        _dispatch->DeleteLine(lineEditPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_LINE_EDIT_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (lineEditPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_LINE_EDIT_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_erase_characters_result eraseCharactersPlan{};
    const auto eraseCharactersStatus = terminal_parser_ffi_output_csi_erase_characters_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &eraseCharactersPlan);
    THROW_HR_IF(E_UNEXPECTED, eraseCharactersStatus != TERMINAL_PARSER_FFI_OK);

    switch (eraseCharactersPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_CHARACTERS_ERASE_CHARACTERS:
        _dispatch->EraseCharacters(eraseCharactersPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_CHARACTERS_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (eraseCharactersPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_CHARACTERS_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_scroll_result scrollPlan{};
    const auto scrollStatus = terminal_parser_ffi_output_csi_scroll_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &scrollPlan);
    THROW_HR_IF(E_UNEXPECTED, scrollStatus != TERMINAL_PARSER_FFI_OK);

    switch (scrollPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_UP:
        _dispatch->ScrollUp(scrollPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_DOWN:
        _dispatch->ScrollDown(scrollPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (scrollPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_SCROLL_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_page_result pagePlan{};
    const auto pageStatus = terminal_parser_ffi_output_csi_page_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &pagePlan);
    THROW_HR_IF(E_UNEXPECTED, pageStatus != TERMINAL_PARSER_FFI_OK);

    switch (pagePlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_NEXT:
        _dispatch->NextPage(pagePlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_PRECEDING:
        _dispatch->PrecedingPage(pagePlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (pagePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_page_position_result pagePositionPlan{};
    const auto pagePositionStatus = terminal_parser_ffi_output_csi_page_position_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &pagePositionPlan);
    THROW_HR_IF(E_UNEXPECTED, pagePositionStatus != TERMINAL_PARSER_FFI_OK);

    switch (pagePositionPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_ABSOLUTE:
        _dispatch->PagePositionAbsolute(pagePositionPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_RELATIVE:
        _dispatch->PagePositionRelative(pagePositionPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_BACK:
        _dispatch->PagePositionBack(pagePositionPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (pagePositionPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_PAGE_POSITION_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_tab_result tabPlan{};
    const auto tabStatus = terminal_parser_ffi_output_csi_tab_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &tabPlan);
    THROW_HR_IF(E_UNEXPECTED, tabStatus != TERMINAL_PARSER_FFI_OK);

    switch (tabPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_FORWARD:
        _dispatch->ForwardTab(tabPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_BACKWARD:
        _dispatch->BackwardsTab(tabPlan.count);
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (tabPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_terminal_parameters_result terminalParametersPlan{};
    const auto terminalParametersStatus = terminal_parser_ffi_output_csi_terminal_parameters_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &terminalParametersPlan);
    THROW_HR_IF(E_UNEXPECTED, terminalParametersStatus != TERMINAL_PARSER_FFI_OK);

    switch (terminalParametersPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_REQUEST:
        _dispatch->RequestTerminalParameters(static_cast<DispatchTypes::ReportingPermission>(terminalParametersPlan.parameter));
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (terminalParametersPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_TERMINAL_PARAMETERS_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_device_attributes_result deviceAttributesPlan{};
    const auto deviceAttributesStatus = terminal_parser_ffi_output_csi_device_attributes_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &deviceAttributesPlan);
    THROW_HR_IF(E_UNEXPECTED, deviceAttributesStatus != TERMINAL_PARSER_FFI_OK);

    switch (deviceAttributesPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_PRIMARY:
        _dispatch->DeviceAttributes();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_SECONDARY:
        _dispatch->SecondaryDeviceAttributes();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_TERTIARY:
        _dispatch->TertiaryDeviceAttributes();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (deviceAttributesPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_ATTRIBUTES_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_cursor_restore_result cursorRestorePlan{};
    const auto cursorRestoreStatus = terminal_parser_ffi_output_csi_cursor_restore_plan(
        static_cast<uint64_t>(id),
        &cursorRestorePlan);
    THROW_HR_IF(E_UNEXPECTED, cursorRestoreStatus != TERMINAL_PARSER_FFI_OK);

    switch (cursorRestorePlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_RESTORE:
        _dispatch->CursorRestoreState();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (cursorRestorePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_RESTORE_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_soft_reset_result softResetPlan{};
    const auto softResetStatus = terminal_parser_ffi_output_csi_soft_reset_plan(
        static_cast<uint64_t>(id),
        &softResetPlan);
    THROW_HR_IF(E_UNEXPECTED, softResetStatus != TERMINAL_PARSER_FFI_OK);

    switch (softResetPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_SOFT_RESET:
        _dispatch->SoftReset();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (softResetPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_SOFT_RESET_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_displayed_extent_result displayedExtentPlan{};
    const auto displayedExtentStatus = terminal_parser_ffi_output_csi_displayed_extent_plan(
        static_cast<uint64_t>(id),
        &displayedExtentPlan);
    THROW_HR_IF(E_UNEXPECTED, displayedExtentStatus != TERMINAL_PARSER_FFI_OK);

    switch (displayedExtentPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_REQUEST:
        _dispatch->RequestDisplayedExtent();
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (displayedExtentPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DISPLAYED_EXTENT_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_cursor_style_result cursorStylePlan{};
    const auto cursorStyleStatus = terminal_parser_ffi_output_csi_cursor_style_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &cursorStylePlan);
    THROW_HR_IF(E_UNEXPECTED, cursorStyleStatus != TERMINAL_PARSER_FFI_OK);

    switch (cursorStylePlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_SET_CURSOR_STYLE:
        _dispatch->SetCursorStyle(static_cast<DispatchTypes::CursorStyle>(cursorStylePlan.style));
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (cursorStylePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_CURSOR_STYLE_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_request_mode_result requestModePlan{};
    const auto requestModeStatus = terminal_parser_ffi_output_csi_request_mode_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        &requestModePlan);
    THROW_HR_IF(E_UNEXPECTED, requestModeStatus != TERMINAL_PARSER_FFI_OK);

    switch (requestModePlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_REQUEST_MODE:
        if (requestModePlan.private_mode != 0)
        {
            _dispatch->RequestMode(static_cast<DispatchTypes::DECPrivateMode>(requestModePlan.mode));
        }
        else
        {
            _dispatch->RequestMode(static_cast<DispatchTypes::ANSIStandardMode>(requestModePlan.mode));
        }
        break;
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (requestModePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_REQUEST_MODE_NONE)
    {
        _ClearLastChar();
        return true;
    }

    terminal_parser_ffi_output_csi_device_status_report_result deviceStatusReportPlan{};
    const auto deviceStatusReportStatus = terminal_parser_ffi_output_csi_device_status_report_plan(
        static_cast<uint64_t>(id),
        static_cast<int32_t>(parameters.at(0).value_or(0)),
        parameters.at(1).has_value() ? 1u : 0u,
        static_cast<int32_t>(parameters.at(1).value_or(0)),
        &deviceStatusReportPlan);
    THROW_HR_IF(E_UNEXPECTED, deviceStatusReportStatus != TERMINAL_PARSER_FFI_OK);

    switch (deviceStatusReportPlan.kind)
    {
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_REPORT:
    {
        const auto reportId = deviceStatusReportPlan.has_id != 0 ? VTParameter{ deviceStatusReportPlan.id } : VTParameter{};
        if (deviceStatusReportPlan.private_mode != 0)
        {
            _dispatch->DeviceStatusReport(DispatchTypes::DECPrivateStatus(deviceStatusReportPlan.status), reportId);
        }
        else
        {
            _dispatch->DeviceStatusReport(DispatchTypes::ANSIStandardStatus(deviceStatusReportPlan.status), reportId);
        }
        break;
    }
    case TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_NONE:
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    if (deviceStatusReportPlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_DEVICE_STATUS_REPORT_NONE)
    {
        _ClearLastChar();
        return true;
    }

    constexpr size_t modePlanCapacity = 32;
    int32_t modeParameters[modePlanCapacity]{};
    size_t modeParameterCount = 0;
    parameters.for_each([&](const auto mode) {
        THROW_HR_IF(E_UNEXPECTED, modeParameterCount >= modePlanCapacity);
        modeParameters[modeParameterCount++] = static_cast<int32_t>(mode);
    });

    terminal_parser_ffi_output_csi_mode_result modePlans[modePlanCapacity]{};
    size_t modePlanCount = 0;
    const auto modeStatus = terminal_parser_ffi_output_csi_mode_plans(
        static_cast<uint64_t>(id),
        modeParameters,
        modeParameterCount,
        modePlans,
        modePlanCapacity,
        &modePlanCount);
    THROW_HR_IF(E_UNEXPECTED, modeStatus != TERMINAL_PARSER_FFI_OK);

    for (size_t index = 0; index < modePlanCount; ++index)
    {
        const auto& modePlan = modePlans[index];
        THROW_HR_IF(E_UNEXPECTED, modePlan.kind != TERMINAL_PARSER_FFI_OUTPUT_CSI_MODE_MODE);

        if (modePlan.private_mode != 0)
        {
            if (modePlan.enabled != 0)
            {
                _dispatch->SetMode(static_cast<DispatchTypes::DECPrivateMode>(modePlan.mode));
            }
            else
            {
                _dispatch->ResetMode(static_cast<DispatchTypes::DECPrivateMode>(modePlan.mode));
            }
        }
        else if (modePlan.enabled != 0)
        {
            _dispatch->SetMode(static_cast<DispatchTypes::ANSIStandardMode>(modePlan.mode));
        }
        else
        {
            _dispatch->ResetMode(static_cast<DispatchTypes::ANSIStandardMode>(modePlan.mode));
        }
    }

    if (modePlanCount != 0)
    {
        _ClearLastChar();
        return true;
    }

    constexpr size_t erasePlanCapacity = 32;
    int32_t eraseParameters[erasePlanCapacity]{};
    size_t eraseParameterCount = 0;
    parameters.for_each([&](const auto eraseType) {
        THROW_HR_IF(E_UNEXPECTED, eraseParameterCount >= erasePlanCapacity);
        eraseParameters[eraseParameterCount++] = static_cast<int32_t>(eraseType);
    });

    terminal_parser_ffi_output_csi_erase_result erasePlans[erasePlanCapacity]{};
    size_t erasePlanCount = 0;
    const auto eraseStatus = terminal_parser_ffi_output_csi_erase_plans(
        static_cast<uint64_t>(id),
        eraseParameters,
        eraseParameterCount,
        erasePlans,
        erasePlanCapacity,
        &erasePlanCount);
    THROW_HR_IF(E_UNEXPECTED, eraseStatus != TERMINAL_PARSER_FFI_OK);

    for (size_t index = 0; index < erasePlanCount; ++index)
    {
        const auto& erasePlan = erasePlans[index];
        const auto eraseType = static_cast<DispatchTypes::EraseType>(erasePlan.value);

        switch (erasePlan.kind)
        {
        case TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_DISPLAY:
            _dispatch->EraseInDisplay(eraseType);
            break;
        case TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_SELECTIVE_DISPLAY:
            _dispatch->SelectiveEraseInDisplay(eraseType);
            break;
        case TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_LINE:
            _dispatch->EraseInLine(eraseType);
            break;
        case TERMINAL_PARSER_FFI_OUTPUT_CSI_ERASE_SELECTIVE_LINE:
            _dispatch->SelectiveEraseInLine(eraseType);
            break;
        default:
            THROW_HR(E_UNEXPECTED);
        }
    }

    if (erasePlanCount != 0)
    {
        _ClearLastChar();
        return true;
    }

    constexpr size_t tabControlPlanCapacity = 32;
    int32_t tabControlParameters[tabControlPlanCapacity]{};
    size_t tabControlParameterCount = 0;
    parameters.for_each([&](const auto tabControlType) {
        THROW_HR_IF(E_UNEXPECTED, tabControlParameterCount >= tabControlPlanCapacity);
        tabControlParameters[tabControlParameterCount++] = static_cast<int32_t>(tabControlType);
    });

    terminal_parser_ffi_output_csi_tab_control_result tabControlPlans[tabControlPlanCapacity]{};
    size_t tabControlPlanCount = 0;
    const auto tabControlStatus = terminal_parser_ffi_output_csi_tab_control_plans(
        static_cast<uint64_t>(id),
        tabControlParameters,
        tabControlParameterCount,
        tabControlPlans,
        tabControlPlanCapacity,
        &tabControlPlanCount);
    THROW_HR_IF(E_UNEXPECTED, tabControlStatus != TERMINAL_PARSER_FFI_OK);

    for (size_t index = 0; index < tabControlPlanCount; ++index)
    {
        const auto& tabControlPlan = tabControlPlans[index];

        switch (tabControlPlan.kind)
        {
        case TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_CLEAR:
            _dispatch->TabClear(static_cast<DispatchTypes::TabClearType>(tabControlPlan.value));
            break;
        case TERMINAL_PARSER_FFI_OUTPUT_CSI_TAB_CONTROL_SET:
            _dispatch->TabSet(tabControlPlan.value);
            break;
        default:
            THROW_HR(E_UNEXPECTED);
        }
    }

    if (tabControlPlanCount != 0)
    {
        _ClearLastChar();
        return true;
    }

    switch (id)
    {


    case CsiActionCodes::SGR_SetGraphicsRendition:
        _dispatch->SetGraphicsRendition(parameters);
        break;











    case CsiActionCodes::DTTERM_WindowManipulation:
        _dispatch->WindowManipulation(parameters.at(0), parameters.at(1), parameters.at(2));
        break;
    case CsiActionCodes::REP_RepeatCharacter:
        // Handled w/o the dispatch. This function is unique in that way
        // If this were in the ITerminalDispatch, then each
        // implementation would effectively be the same, calling only
        // functions that are already part of the interface.
        // Print the last graphical character a number of times.
        if (_lastPrintedChar != AsciiChars::NUL)
        {
            const size_t repeatCount = parameters.at(0);
            std::wstring wstr(repeatCount, _lastPrintedChar);
            _dispatch->PrintString(wstr);
        }
        break;



    case CsiActionCodes::DECSCA_SetCharacterProtectionAttribute:
        _dispatch->SetCharacterProtectionAttribute(parameters);
        break;

    case CsiActionCodes::XT_PushSgr:
    case CsiActionCodes::XT_PushSgrAlias:
        _dispatch->PushGraphicsRendition(parameters);
        break;
    case CsiActionCodes::XT_PopSgr:
    case CsiActionCodes::XT_PopSgrAlias:
        _dispatch->PopGraphicsRendition();
        break;

    case CsiActionCodes::DECCARA_ChangeAttributesRectangularArea:
        _dispatch->ChangeAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
        break;
    case CsiActionCodes::DECRARA_ReverseAttributesRectangularArea:
        _dispatch->ReverseAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
        break;
    case CsiActionCodes::DECCRA_CopyRectangularArea:
        _dispatch->CopyRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.at(4), parameters.at(5), parameters.at(6), parameters.at(7));
        break;
    case CsiActionCodes::DECRQTSR_RequestTerminalStateReport:
        _dispatch->RequestTerminalStateReport(parameters.at(0), parameters.at(1));
        break;
    case CsiActionCodes::DECRQPSR_RequestPresentationStateReport:
        _dispatch->RequestPresentationStateReport(parameters.at(0));
        break;
    case CsiActionCodes::DECFRA_FillRectangularArea:
        _dispatch->FillRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2), parameters.at(3).value_or(0), parameters.at(4).value_or(0));
        break;
    case CsiActionCodes::DECERA_EraseRectangularArea:
        _dispatch->EraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
        break;
    case CsiActionCodes::DECSERA_SelectiveEraseRectangularArea:
        _dispatch->SelectiveEraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
        break;
    case CsiActionCodes::DECRQUPSS_RequestUserPreferenceSupplementalSet:
        _dispatch->RequestUserPreferenceCharset();
        break;
    case CsiActionCodes::DECIC_InsertColumn:
        _dispatch->InsertColumn(parameters.at(0));
        break;
    case CsiActionCodes::DECDC_DeleteColumn:
        _dispatch->DeleteColumn(parameters.at(0));
        break;
    case CsiActionCodes::DECSACE_SelectAttributeChangeExtent:
        _dispatch->SelectAttributeChangeExtent(parameters.at(0));
        break;
    case CsiActionCodes::DECRQCRA_RequestChecksumRectangularArea:
        _dispatch->RequestChecksumRectangularArea(parameters.at(0).value_or(0), parameters.at(1).value_or(0), parameters.at(2), parameters.at(3), parameters.at(4).value_or(0), parameters.at(5).value_or(0));
        break;
    case CsiActionCodes::DECINVM_InvokeMacro:
        _dispatch->InvokeMacro(parameters.at(0).value_or(0));
        break;
    case CsiActionCodes::DECAC_AssignColor:
        _dispatch->AssignColor(parameters.at(0), parameters.at(1).value_or(0), parameters.at(2).value_or(0));
        break;
    case CsiActionCodes::DECPS_PlaySound:
        _dispatch->PlaySounds(parameters);
        break;
    case CsiActionCodes::KKP_KittyKeyboardSet:
        _dispatch->SetKittyKeyboardProtocol(parameters.at(0), parameters.at(1));
        break;
    case CsiActionCodes::KKP_KittyKeyboardQuery:
        _dispatch->QueryKittyKeyboardProtocol();
        break;
    case CsiActionCodes::KKP_KittyKeyboardPush:
        _dispatch->PushKittyKeyboardProtocol(parameters.at(0));
        break;
    case CsiActionCodes::KKP_KittyKeyboardPop:
        _dispatch->PopKittyKeyboardProtocol(parameters.at(0));
        break;
    default:
        _dispatch->UnknownSequence();
        break;
    }

    _ClearLastChar();

    return true;
}

// Routine Description:
// - Triggers the DcsDispatch action to indicate that the listener should handle
//      a control sequence. Returns the handler function that is to be used to
//      process the subsequent data string characters in the sequence.
// Arguments:
// - id - Identifier of the control sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - the data string handler function or nullptr if the sequence is not supported
IStateMachineEngine::StringHandler OutputStateMachineEngine::ActionDcsDispatch(const VTID id, const VTParameters parameters)
{
    StringHandler handler = nullptr;

    switch (id)
    {
    case DcsActionCodes::SIXEL_DefineImage:
        handler = _dispatch->DefineSixelImage(parameters.at(0),
                                              parameters.at(1),
                                              parameters.at(2));
        break;
    case DcsActionCodes::DECDLD_DownloadDRCS:
        handler = _dispatch->DownloadDRCS(parameters.at(0),
                                          parameters.at(1),
                                          parameters.at(2),
                                          parameters.at(3),
                                          parameters.at(4),
                                          parameters.at(5),
                                          parameters.at(6),
                                          parameters.at(7));
        break;
    case DcsActionCodes::DECAUPSS_AssignUserPreferenceSupplementalSet:
        handler = _dispatch->AssignUserPreferenceCharset(parameters.at(0));
        break;
    case DcsActionCodes::DECDMAC_DefineMacro:
        handler = _dispatch->DefineMacro(parameters.at(0).value_or(0), parameters.at(1), parameters.at(2));
        break;
    case DcsActionCodes::DECRSTS_RestoreTerminalState:
        handler = _dispatch->RestoreTerminalState(parameters.at(0));
        break;
    case DcsActionCodes::DECRQSS_RequestSetting:
        handler = _dispatch->RequestSetting();
        break;
    case DcsActionCodes::DECRSPS_RestorePresentationState:
        handler = _dispatch->RestorePresentationState(parameters.at(0));
        break;
    default:
        _dispatch->UnknownSequence();
        break;
    }

    _ClearLastChar();

    return handler;
}

// Routine Description:
// - Triggers the OscDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - parameter - identifier of the OSC action to perform
// - string - OSC string we've collected. NOT null terminated.
// Return Value:
// - true if we handled the dispatch.
bool OutputStateMachineEngine::ActionOscDispatch(const size_t parameter, const std::wstring_view string)
{
    switch (parameter)
    {
    case OscActionCodes::SetIconAndWindowTitle:
    case OscActionCodes::SetWindowIcon:
    case OscActionCodes::SetWindowTitle:
    case OscActionCodes::DECSWT_SetWindowTitle:
    {
        _dispatch->SetWindowTitle(string);
        break;
    }
    case OscActionCodes::SetColor:
    {
        std::vector<size_t> tableIndexes;
        std::vector<DWORD> colors;
        if (_GetOscSetColorTable(string, tableIndexes, colors))
        {
            for (size_t i = 0; i < tableIndexes.size(); i++)
            {
                const auto tableIndex = til::at(tableIndexes, i);
                const auto rgb = til::at(colors, i);
                if (rgb == COLOR_INQUIRY_COLOR)
                {
                    _dispatch->RequestColorTableEntry(tableIndex);
                }
                else
                {
                    _dispatch->SetColorTableEntry(tableIndex, rgb);
                }
            }
        }
        break;
    }
    case OscActionCodes::SetForegroundColor:
    case OscActionCodes::SetBackgroundColor:
    case OscActionCodes::SetCursorColor:
    case OscActionCodes::SetHighlightColor:
    {
        std::vector<DWORD> colors;
        if (_GetOscSetColor(string, colors))
        {
            auto resource = parameter;
            for (auto&& color : colors)
            {
                if (color == COLOR_INQUIRY_COLOR)
                {
                    _dispatch->RequestXtermColorResource(resource);
                }
                else if (color != INVALID_COLOR)
                {
                    _dispatch->SetXtermColorResource(resource, color);
                }
                resource++;
            }
        }
        break;
    }
    case OscActionCodes::SetClipboard:
    {
        std::wstring setClipboardContent;
        auto queryClipboard = false;
        if (_GetOscSetClipboard(string, setClipboardContent, queryClipboard) && !queryClipboard)
        {
            _dispatch->SetClipboard(setClipboardContent);
        }
        break;
    }
    case OscActionCodes::ResetColor:
    {
        if (string.empty())
        {
            _dispatch->ResetColorTable();
        }
        else
        {
            for (auto&& c : til::split_iterator{ string, L';' })
            {
                if (const auto index{ til::parse_unsigned<size_t>(c, 10) }; index)
                {
                    _dispatch->ResetColorTableEntry(*index);
                }
                else
                {
                    // NOTE: xterm stops at the first unparseable index whereas VTE keeps going.
                    break;
                }
            }
        }
        break;
    }
    case OscActionCodes::ResetForegroundColor:
    case OscActionCodes::ResetBackgroundColor:
    case OscActionCodes::ResetCursorColor:
    case OscActionCodes::ResetHighlightColor:
    {
        // NOTE: xterm ignores the request if there's any parameters whereas VTE resets the provided index and ignores the rest
        if (string.empty())
        {
            // The reset codes for xterm dynamic resources are the set codes + 100
            _dispatch->ResetXtermColorResource(parameter - 100u);
        }
        break;
    }
    case OscActionCodes::CurrentWorkingDirectory:
        _dispatch->SetCurrentWorkingDirectory(string);
        break;
    case OscActionCodes::Hyperlink:
    {
        std::wstring params;
        std::wstring uri;
        if (_ParseHyperlink(string, params, uri))
        {
            if (uri.empty())
            {
                _dispatch->EndHyperlink();
            }
            else
            {
                _dispatch->AddHyperlink(uri, params);
            }
        }
        break;
    }
    case OscActionCodes::ConEmuAction:
    {
        _dispatch->DoConEmuAction(string);
        break;
    }
    case OscActionCodes::ITerm2Action:
    {
        _dispatch->DoITerm2Action(string);
        break;
    }
    case OscActionCodes::FinalTermAction:
    {
        _dispatch->DoFinalTermAction(string);
        break;
    }
    case OscActionCodes::VsCodeAction:
    {
        _dispatch->DoVsCodeAction(string);
        break;
    }
    case OscActionCodes::WTAction:
    {
        _dispatch->DoWTAction(string);
        break;
    }
    case OscActionCodes::UrxvtAction:
        _dispatch->DoUrxvtAction(string);
        break;
    default:
        _dispatch->UnknownSequence();
        break;
    }

    _ClearLastChar();

    return true;
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
bool OutputStateMachineEngine::ActionSs3Dispatch(const wchar_t /*wch*/, const VTParameters /*parameters*/) noexcept
{
    // The output engine doesn't handle any SS3 sequences.
    _dispatch->UnknownSequence();
    _ClearLastChar();
    return true;
}

// Routine Description:
// - OSC 4 ; c ; spec ST
//      c: the index of the ansi color table
//      spec: The colors are specified by name or RGB specification as per XParseColor
//
//   It's possible to have multiple "c ; spec" pairs, which will set the index "c" of the color table
//   with color parsed from "spec" for each pair respectively.
// Arguments:
// - string - the Osc String to parse
// - tableIndexes - receives the table indexes
// - rgbs - receives the colors that we parsed in the format: 0x00BBGGRR
// Return Value:
// - True if at least one table index and color was parsed successfully. False otherwise.
bool OutputStateMachineEngine::_GetOscSetColorTable(const std::wstring_view string,
                                                    std::vector<size_t>& tableIndexes,
                                                    std::vector<DWORD>& rgbs) const
{
    using namespace std::string_view_literals;

    const auto parts = Utils::SplitString(string, L';');
    if (parts.size() < 2)
    {
        return false;
    }

    std::vector<size_t> newTableIndexes;
    std::vector<DWORD> newRgbs;

    for (size_t i = 0, j = 1; j < parts.size(); i += 2, j += 2)
    {
        auto&& index = til::at(parts, i);
        auto&& color = til::at(parts, j);
        unsigned int tableIndex = 0;
        const auto indexSuccess = Utils::StringToUint(index, tableIndex);

        if (indexSuccess)
        {
            if (color == L"?"sv) [[unlikely]]
            {
                newTableIndexes.push_back(tableIndex);
                newRgbs.push_back(COLOR_INQUIRY_COLOR);
            }
            else if (const auto colorOptional = Utils::ColorFromXTermColor(color))
            {
                newTableIndexes.push_back(tableIndex);
                newRgbs.push_back(colorOptional.value());
            }
        }
    }

    tableIndexes.swap(newTableIndexes);
    rgbs.swap(newRgbs);

    return tableIndexes.size() > 0 && rgbs.size() > 0;
}

#pragma warning(push)
#pragma warning(disable : 26445) // Suppress lifetime check for a reference to std::span or std::string_view

// Routine Description:
// - Given a hyperlink string, attempts to parse the URI encoded. An 'id' parameter
//   may be provided.
//   If there is a URI, the well formatted string looks like:
//          "<params>;<URI>"
//   To be specific, params is an optional list of key=value assignments, separated by the ':'. Example:
//          "id=xyz123:foo=bar:baz=value"
//   If there is no URI, we need to close the hyperlink and the string looks like:
//          ";"
// Arguments:
// - string - the string containing the parameters and URI
// - params - where to store the parameters
// - uri - where to store the uri
// Return Value:
// - True if a URI was successfully parsed or if we are meant to close a hyperlink
bool OutputStateMachineEngine::_ParseHyperlink(const std::wstring_view string,
                                               std::wstring& params,
                                               std::wstring& uri) const
{
    params.clear();
    uri.clear();

    if (string == L";")
    {
        return true;
    }

    const auto midPos = string.find(';');
    if (midPos != std::wstring::npos)
    {
        uri = string.substr(midPos + 1, MAX_URL_LENGTH);
        const auto paramStr = string.substr(0, midPos);
        const auto paramParts = Utils::SplitString(paramStr, ':');
        for (const auto& part : paramParts)
        {
            const auto idPos = part.find(hyperlinkIDParameter);
            if (idPos != std::wstring::npos)
            {
                params = part.substr(idPos + hyperlinkIDParameter.size());
            }
        }
        return true;
    }
    return false;
}

#pragma warning(pop)

// Routine Description:
// - OSC 10, 11, 12 ; spec ST
//      spec: The colors are specified by name or RGB specification as per XParseColor
//
//   It's possible to have multiple "spec", which by design equals a series of OSC command
//   with accumulated Ps. For example "OSC 10;color1;color2" is effectively an "OSC 10;color1"
//   and an "OSC 11;color2".
//
// Arguments:
// - string - the Osc String to parse
// - rgbs - receives the colors that we parsed in the format: 0x00BBGGRR
// Return Value:
// - True if at least one color was parsed successfully. False otherwise.
bool OutputStateMachineEngine::_GetOscSetColor(const std::wstring_view string,
                                               std::vector<DWORD>& rgbs) const
{
    using namespace std::string_view_literals;

    const auto parts = Utils::SplitString(string, L';');
    if (parts.size() < 1)
    {
        return false;
    }

    std::vector<DWORD> newRgbs;
    for (const auto& part : parts)
    {
        if (part == L"?"sv) [[unlikely]]
        {
            newRgbs.push_back(COLOR_INQUIRY_COLOR);
            continue;
        }
        else if (const auto colorOptional = Utils::ColorFromXTermColor(part))
        {
            newRgbs.push_back(colorOptional.value());
        }
        else
        {
            newRgbs.push_back(INVALID_COLOR);
        }
    }

    rgbs.swap(newRgbs);

    return rgbs.size() > 0;
}

// Routine Description:
// - Parse OscSetClipboard parameters with the format `Pc;Pd`. Currently the first parameter `Pc` is
// ignored. The second parameter `Pd` should be a valid base64 string or character `?`.
// Arguments:
// - string - Osc String input.
// - content - Content to set to clipboard.
// - queryClipboard - Whether to get clipboard content and return it to terminal with base64 encoded.
// Return Value:
// - True if there was a valid base64 string or the passed parameter was `?`.
bool OutputStateMachineEngine::_GetOscSetClipboard(const std::wstring_view string,
                                                   std::wstring& content,
                                                   bool& queryClipboard) const noexcept
{
    const auto pos = string.find(L';');
    if (pos == std::wstring_view::npos)
    {
        return false;
    }

    const auto substr = string.substr(pos + 1);
    if (substr == L"?")
    {
        queryClipboard = true;
        return true;
    }

// Log_IfFailed has the following description: "Should be decorated WI_NOEXCEPT, but conflicts with forceinline."
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'Log_IfFailed()' which may throw exceptions (f.6).
    return SUCCEEDED_LOG(Base64::Decode(substr, content));
}

// Routine Description:
// - Takes a sequence id ("final byte") and determines if it accepts sub parameters.
// Arguments:
// - id - The sequence id to check for.
// Return Value:
// - True, if it accepts sub parameters or else False.
bool OutputStateMachineEngine::_CanSeqAcceptSubParam(const VTID id, const VTParameters& parameters) noexcept
{
    switch (id)
    {
    case SGR_SetGraphicsRendition:
        return true;
    case DECCARA_ChangeAttributesRectangularArea:
    case DECRARA_ReverseAttributesRectangularArea:
        return !parameters.hasSubParamsFor(0) && !parameters.hasSubParamsFor(1) && !parameters.hasSubParamsFor(2) && !parameters.hasSubParamsFor(3);
    default:
        return false;
    }
}

// Method Description:
// - Clears our last stored character. The last stored character is the last
//      graphical character we printed, which is reset if any other action is
//      dispatched.
// Arguments:
// - <none>
// Return Value:
// - <none>
void OutputStateMachineEngine::_ClearLastChar() noexcept
{
    _lastPrintedChar = AsciiChars::NUL;
}
