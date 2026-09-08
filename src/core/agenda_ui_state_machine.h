#pragma once

#include "agenda_ui_types.h"

class AgendaUiStateMachine
{
public:
    AgendaUiStateMachine(
        AgendaController& controller,
        Clock& clock,
        const DateTime& clockSetupDefault
    );

    void start();
    void tick();

    void handle(
        const AgendaUiAction& action
    );

    AgendaUiView view() const;

private:
    void startOrRetry();
    void resetErrors();

    void rebuildMonthCells(
        AgendaUiView& result
    ) const;

    void rebuildDayRows(
        AgendaUiView& result
    ) const;

    void clampDayPage();
    void beginCreate();
    void beginEdit();
    void leaveEditorAfterDiscard();
    void saveEditor();
    void retryPendingOperation();
    void confirmDelete();
    void checkReminder();

    AgendaController& controller_;
    Clock& clock_;

    AgendaUiScreen screen_;
    AgendaUiOverlay overlay_;

    AgendaControllerError controllerError_;
    EventError eventError_;
    ClockError clockError_;

    DateTime clockSetupDefault_;
    DateTime clockDraft_;
    DateTime lastNow_;

    YearMonth displayedMonth_;

    std::optional<Date> selectedDate_;

    std::optional<std::uint32_t>
        selectedEventId_;

    std::size_t dayPageIndex_;

    AgendaUiEditorDraft editor_;
    AgendaUiScreen editorReturnScreen_;
    T9Engine t9_;

    AgendaUiPendingOperation pendingOperation_;

    std::optional<Event> reminder_;
};