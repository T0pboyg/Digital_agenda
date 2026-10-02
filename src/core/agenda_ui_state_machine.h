#pragma once

#include "agenda_ui_types.h"
#include "alarm_ui_controller.h"

class AgendaUiStateMachine
{
public:
    AgendaUiStateMachine(
        AgendaController& controller,
        Clock& clock,
        const DateTime& clockSetupDefault,
        AlarmService* alarms = nullptr
    );

    void start();
    void tick();

    void handle(
        const AgendaUiAction& action
    );

    AgendaUiView view() const;
    bool hasAlarms() const { return alarms_ != nullptr; }
    void setBatteryPercent(std::optional<int> value) { batteryPercent_ = value; }


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

    AlarmService* alarms_;
    AlarmUiController alarmUi_;
    std::optional<int> batteryPercent_;
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