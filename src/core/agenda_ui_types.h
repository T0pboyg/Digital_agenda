#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "agenda_controller.h"
#include "alarm_service.h"
#include "calendar_engine.h"
#include "clock.h"
#include "t9_engine.h"

constexpr std::size_t AGENDA_UI_EVENTS_PER_PAGE = 3;

enum class AgendaUiScreen
{
    ClockSetup,
    Month,
    Day,
    EventDetails,
    EditorDate,
    EditorTime,
    EditorText,
    EditorReview,
    StartupError,
    AlarmList,
    AlarmEditor
};

enum class AgendaUiOverlay
{
    None,
    Reminder,
    ConfirmDelete,
    ConfirmDiscard,
    OperationError,
    ClockError,
    Alarm
};

enum class AgendaUiEditorMode
{
    Create,
    Update
};

enum class AgendaUiPendingOperation
{
    None,
    CreateEvent,
    UpdateEvent,
    DeleteEvent
};

enum class AgendaUiActionType
{
    RetryStartup,
    ClockPreviousDay,
    ClockNextDay,
    ClockPreviousHour,
    ClockNextHour,
    ClockPreviousMinute,
    ClockNextMinute,
    ConfirmClock,
    PreviousMonth,
    NextMonth,
    SelectDate,
    PreviousDayPage,
    NextDayPage,
    SelectEvent,
    Back,
    StartCreate,
    StartEdit,
    EditorPreviousDay,
    EditorNextDay,
    ToggleEventTime,
    EditorPreviousHour,
    EditorNextHour,
    EditorPreviousMinute,
    EditorNextMinute,
    EditorNext,
    EditorBack,
    EditorCancel,
    ConfirmDiscard,
    CancelDiscard,
    T9Digit,
    ToggleT9Language,
    ToggleT9Mode,
    ToggleT9Case,
    T9Backspace,
    SaveEditor,
    RetryOperation,
    CancelOperation,
    RequestDelete,
    ConfirmDelete,
    CancelDelete,
    AcknowledgeReminder,
    OpenAlarms, CreateAlarm, SelectAlarm, ToggleAlarm, SaveAlarm, DeleteAlarm,
    ConfirmAlarmDelete, CancelAlarmDelete, StopAlarm, RetryAlarms,
    AlarmPreviousPage, AlarmNextPage, AlarmPreviousDay, AlarmNextDay,
    AlarmPreviousHour, AlarmNextHour, AlarmPreviousMinute, AlarmNextMinute,
    AlarmToggleDaily, AlarmToggleEnabled
};

struct AgendaUiAction
{
    AgendaUiActionType type;
    std::optional<Date> date = std::nullopt;

    std::optional<std::uint32_t> eventId =
        std::nullopt;
    char digit = '\0';
    std::uint32_t timestampMilliseconds = 0;
};

struct AgendaUiMonthCell
{
    std::optional<Date> date;
    bool inDisplayedMonth = false;
    bool isToday = false;
    bool isSelected = false;
    bool hasEvents = false;
};

struct AgendaUiEventRow
{
    std::uint32_t eventId = 0;
    std::optional<TimeOfDay> time;
    std::string text;
};

struct AgendaUiEditorDraft
{
    AgendaUiEditorMode mode =
        AgendaUiEditorMode::Create;

    std::optional<std::uint32_t> eventId;
    Date date{2000, 1, 1};
    std::optional<TimeOfDay> time;
    std::string text;
    bool dirty = false;
};

struct AgendaUiView
{
    bool alarmFeature = false;
    std::optional<DateTime> currentTime;
    std::optional<int> batteryPercent;
    std::vector<Alarm> alarms;
    Alarm alarmDraft;
    std::size_t alarmPage = 0;
    std::size_t alarmPageCount = 0;
    bool alarmConfirmDelete = false;
    AlarmError alarmError = AlarmError::None;

    AgendaUiScreen screen =
        AgendaUiScreen::StartupError;

    AgendaUiOverlay overlay =
        AgendaUiOverlay::None;

    AgendaControllerError controllerError =
        AgendaControllerError::None;

    EventError eventError =
        EventError::None;

    ClockError clockError =
        ClockError::None;

    YearMonth displayedMonth{2000, 1};

    bool monthPreviousEnabled = false;
    bool monthNextEnabled = false;

    DateTime clockDraft{
        Date{2000, 1, 1},
        TimeOfDay{0, 0}
    };

    std::array<AgendaUiMonthCell, 42>
        monthCells{};

    std::optional<Date> selectedDate;

    std::vector<AgendaUiEventRow>
        dayEvents;

    std::size_t dayPageIndex = 0;
    std::size_t dayPageCount = 0;

    bool dayPreviousEnabled = false;
    bool dayNextEnabled = false;

    std::optional<Event> selectedEvent;

    AgendaUiEditorDraft editor;

    T9Language t9Language =
        T9Language::Greek;

    T9Mode t9Mode =
        T9Mode::Letters;

    bool t9Uppercase = false;

    std::optional<Event> reminder;
};