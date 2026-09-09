#include <algorithm>
#include <vector>

#include "agenda_ui_state_machine.h"

namespace
{
bool validDateTime(const DateTime& value)
{
    return
        isValidDate(value.date) &&
        isValidTime(value.time);
}

bool sameDate(
    const Date& left,
    const Date& right
)
{
    return
        left.year == right.year &&
        left.month == right.month &&
        left.day == right.day;
}

std::vector<const Event*> eventsForDate(
    const AgendaManager& agenda,
    const Date& date
)
{
    std::vector<const Event*> result;

    for (const Event& event : agenda.events())
    {
        if (sameDate(event.eventDate, date))
        {
            result.push_back(&event);
        }
    }

    return result;
}

std::string firstUiCodePoints(
    const std::string& text
)
{
    constexpr std::size_t limit = 32;

    std::size_t position = 0;
    std::size_t count = 0;

    while (
        position < text.size() &&
        count < limit
    )
    {
        ++position;

        while (
            position < text.size() &&
            (
                static_cast<unsigned char>(
                    text[position]
                ) &
                0xC0
            ) == 0x80
        )
        {
            ++position;
        }

        ++count;
    }

    return text.substr(0, position);
}

void moveToNextDate(Date& value)
{
    const std::optional<Date> moved =
        nextDate(value);

    if (moved.has_value())
    {
        value = *moved;
    }
}

void moveToPreviousDate(Date& value)
{
    const std::optional<Date> moved =
        previousDate(value);

    if (moved.has_value())
    {
        value = *moved;
    }
}

void adjustEditorMinutes(
    std::optional<TimeOfDay>& value,
    int delta
)
{
    if (!value.has_value())
    {
        return;
    }

    const int current =
        value->hour * 60 +
        value->minute;

    const int wrapped =
        (
            current +
            delta +
            1440
        ) % 1440;

    value = TimeOfDay{
        wrapped / 60,
        wrapped % 60
    };
}
}

AgendaUiStateMachine::AgendaUiStateMachine(
    AgendaController& controller,
    Clock& clock,
    const DateTime& clockSetupDefault
)
    : controller_(controller),
      clock_(clock),
      screen_(AgendaUiScreen::StartupError),
      overlay_(AgendaUiOverlay::None),
      controllerError_(
          AgendaControllerError::None
      ),
      eventError_(EventError::None),
      clockError_(ClockError::None),
      clockSetupDefault_(clockSetupDefault),
      clockDraft_(clockSetupDefault),
      lastNow_(clockSetupDefault),
      displayedMonth_{
          clockSetupDefault.date.year,
          clockSetupDefault.date.month
      },
      selectedDate_(),
      selectedEventId_(),
      dayPageIndex_(0),
      editor_(),
      editorReturnScreen_(
          AgendaUiScreen::Day
      ),
      t9_(),
      pendingOperation_(
          AgendaUiPendingOperation::None
      ),
      reminder_()
{
}

void AgendaUiStateMachine::start()
{
    startOrRetry();
}

void AgendaUiStateMachine::startOrRetry()
{
    resetErrors();
    overlay_ = AgendaUiOverlay::None;

    const AgendaStartResult started =
        controller_.start();

    if (
        started.error !=
        AgendaControllerError::None
    )
    {
        controllerError_ = started.error;
        screen_ = AgendaUiScreen::StartupError;
        return;
    }

    const ClockReadResult current =
        clock_.read();

    if (
        current.error != ClockError::None ||
        !validDateTime(current.value)
    )
    {
        clockError_ =
            current.error == ClockError::None
                ? ClockError::InvalidDateTime
                : current.error;

        clockDraft_ = clockSetupDefault_;
        screen_ = AgendaUiScreen::ClockSetup;
        return;
    }

    lastNow_ = current.value;

    displayedMonth_ = YearMonth{
        current.value.date.year,
        current.value.date.month
    };

    screen_ = AgendaUiScreen::Month;
}

void AgendaUiStateMachine::tick()
{
}

void AgendaUiStateMachine::handle(
    const AgendaUiAction& action
)
{
    if (
        action.type ==
            AgendaUiActionType::RetryStartup &&
        screen_ ==
            AgendaUiScreen::StartupError
    )
    {
        startOrRetry();
        return;
    }

    if (
        overlay_ ==
        AgendaUiOverlay::ConfirmDiscard
    )
    {
        if (
            action.type ==
            AgendaUiActionType::CancelDiscard
        )
        {
            overlay_ = AgendaUiOverlay::None;
        }
        else if (
            action.type ==
            AgendaUiActionType::ConfirmDiscard
        )
        {
            leaveEditorAfterDiscard();
        }

        return;
    }

    if (
        overlay_ ==
        AgendaUiOverlay::ConfirmDelete
    )
    {
        if (
            action.type ==
            AgendaUiActionType::CancelDelete
        )
        {
            overlay_ = AgendaUiOverlay::None;
        }
        else if (
            action.type ==
            AgendaUiActionType::ConfirmDelete
        )
        {
            overlay_ = AgendaUiOverlay::None;
            confirmDelete();
        }

        return;
    }

    if (
        overlay_ ==
            AgendaUiOverlay::OperationError ||
        overlay_ ==
            AgendaUiOverlay::ClockError
    )
    {
        if (
            action.type ==
            AgendaUiActionType::RetryOperation
        )
        {
            overlay_ = AgendaUiOverlay::None;
            retryPendingOperation();
        }
        else if (
            action.type ==
            AgendaUiActionType::CancelOperation
        )
        {
            overlay_ = AgendaUiOverlay::None;

            pendingOperation_ =
                AgendaUiPendingOperation::None;

            resetErrors();
        }

        return;
    }

    if (overlay_ != AgendaUiOverlay::None)
    {
        return;
    }

    if (
        screen_ ==
        AgendaUiScreen::EventDetails
    )
    {
        if (
            !selectedEventId_.has_value() ||
            controller_.agenda().findEvent(
                *selectedEventId_
            ) == nullptr
        )
        {
            selectedEventId_.reset();
            screen_ = AgendaUiScreen::Day;
            return;
        }
    }

    if (
        screen_ ==
            AgendaUiScreen::EventDetails &&
        action.type ==
            AgendaUiActionType::RequestDelete
    )
    {
        if (
            !selectedEventId_.has_value() ||
            controller_.agenda().findEvent(
                *selectedEventId_
            ) == nullptr
        )
        {
            selectedEventId_.reset();
            screen_ = AgendaUiScreen::Day;
            return;
        }

        overlay_ =
            AgendaUiOverlay::ConfirmDelete;

        return;
    }

    if (
        screen_ ==
            AgendaUiScreen::EventDetails &&
        action.type ==
            AgendaUiActionType::StartEdit
    )
    {
        beginEdit();
        return;
    }

    if (
        action.type ==
        AgendaUiActionType::Back
    )
    {
        if (
            screen_ ==
            AgendaUiScreen::EventDetails
        )
        {
            screen_ = AgendaUiScreen::Day;
            return;
        }

        if (
            screen_ ==
            AgendaUiScreen::Day
        )
        {
            screen_ = AgendaUiScreen::Month;
            return;
        }
    }

    if (
        screen_ == AgendaUiScreen::EditorDate ||
        screen_ == AgendaUiScreen::EditorTime ||
        screen_ == AgendaUiScreen::EditorText ||
        screen_ == AgendaUiScreen::EditorReview
    )
    {
        if (
            screen_ ==
                AgendaUiScreen::EditorReview &&
            action.type ==
                AgendaUiActionType::SaveEditor
        )
        {
            saveEditor();
            return;
        }

        if (
            action.type ==
                AgendaUiActionType::EditorCancel ||
            (
                action.type ==
                    AgendaUiActionType::EditorBack &&
                screen_ ==
                    AgendaUiScreen::EditorDate
            )
        )
        {
            if (editor_.dirty)
            {
                overlay_ =
                    AgendaUiOverlay::ConfirmDiscard;
            }
            else
            {
                leaveEditorAfterDiscard();
            }

            return;
        }

        if (
            screen_ ==
                AgendaUiScreen::EditorDate &&
            (
                action.type ==
                    AgendaUiActionType::
                        EditorPreviousDay ||
                action.type ==
                    AgendaUiActionType::
                        EditorNextDay
            )
        )
        {
            const std::optional<Date> moved =
                action.type ==
                    AgendaUiActionType::
                        EditorPreviousDay
                    ? previousDate(editor_.date)
                    : nextDate(editor_.date);

            if (moved.has_value())
            {
                editor_.date = *moved;
                editor_.dirty = true;
            }

            return;
        }

        if (
            screen_ ==
            AgendaUiScreen::EditorTime
        )
        {
            if (
                action.type ==
                AgendaUiActionType::
                    ToggleEventTime
            )
            {
                if (editor_.time.has_value())
                {
                    editor_.time.reset();
                }
                else
                {
                    editor_.time =
                        TimeOfDay{12, 0};
                }

                editor_.dirty = true;
                return;
            }

            int delta = 0;

            if (
                action.type ==
                AgendaUiActionType::
                    EditorPreviousHour
            )
            {
                delta = -60;
            }
            else if (
                action.type ==
                AgendaUiActionType::
                    EditorNextHour
            )
            {
                delta = 60;
            }
            else if (
                action.type ==
                AgendaUiActionType::
                    EditorPreviousMinute
            )
            {
                delta = -5;
            }
            else if (
                action.type ==
                AgendaUiActionType::
                    EditorNextMinute
            )
            {
                delta = 5;
            }

            if (
                delta != 0 &&
                editor_.time.has_value()
            )
            {
                adjustEditorMinutes(
                    editor_.time,
                    delta
                );

                editor_.dirty = true;
                return;
            }
        }

        if (
            screen_ ==
            AgendaUiScreen::EditorText
        )
        {
            switch (action.type)
            {
                case AgendaUiActionType::T9Digit:
                    if (
                        t9_.pressDigit(
                            action.digit,
                            action.timestampMilliseconds
                        )
                    )
                    {
                        editor_.text = t9_.text();
                        editor_.dirty = true;
                    }

                    return;

                case AgendaUiActionType::
                    ToggleT9Language:
                    t9_.toggleLanguage();
                    return;

                case AgendaUiActionType::
                    ToggleT9Mode:
                    t9_.toggleMode();
                    return;

                case AgendaUiActionType::
                    ToggleT9Case:
                    t9_.toggleCase();
                    return;

                case AgendaUiActionType::
                    T9Backspace:
                    if (t9_.backspace())
                    {
                        editor_.text = t9_.text();
                        editor_.dirty = true;
                    }

                    return;

                default:
                    break;
            }
        }

        if (
            action.type ==
            AgendaUiActionType::EditorNext
        )
        {
            switch (screen_)
            {
                case AgendaUiScreen::EditorDate:
                    screen_ =
                        AgendaUiScreen::EditorTime;
                    return;

                case AgendaUiScreen::EditorTime:
                    screen_ =
                        AgendaUiScreen::EditorText;
                    return;

                case AgendaUiScreen::EditorText:
                    t9_.commitPending();
                    editor_.text = t9_.text();

                    screen_ =
                        AgendaUiScreen::EditorReview;
                    return;

                default:
                    return;
            }
        }

        if (
            action.type ==
            AgendaUiActionType::EditorBack
        )
        {
            switch (screen_)
            {
                case AgendaUiScreen::EditorReview:
                    screen_ =
                        AgendaUiScreen::EditorText;
                    return;

                case AgendaUiScreen::EditorText:
                    screen_ =
                        AgendaUiScreen::EditorTime;
                    return;

                case AgendaUiScreen::EditorTime:
                    screen_ =
                        AgendaUiScreen::EditorDate;
                    return;

                default:
                    return;
            }
        }

        return;
    }

    if (
        screen_ ==
        AgendaUiScreen::ClockSetup
    )
    {
        switch (action.type)
        {
            case AgendaUiActionType::
                ClockPreviousDay:
                moveToPreviousDate(
                    clockDraft_.date
                );
                return;

            case AgendaUiActionType::
                ClockNextDay:
                moveToNextDate(
                    clockDraft_.date
                );
                return;

            case AgendaUiActionType::
                ClockPreviousHour:
                clockDraft_.time.hour =
                    (
                        clockDraft_.time.hour +
                        23
                    ) % 24;
                return;

            case AgendaUiActionType::
                ClockNextHour:
                clockDraft_.time.hour =
                    (
                        clockDraft_.time.hour +
                        1
                    ) % 24;
                return;

            case AgendaUiActionType::
                ClockPreviousMinute:
                clockDraft_.time.minute =
                    (
                        clockDraft_.time.minute +
                        59
                    ) % 60;
                return;

            case AgendaUiActionType::
                ClockNextMinute:
                clockDraft_.time.minute =
                    (
                        clockDraft_.time.minute +
                        1
                    ) % 60;
                return;

            case AgendaUiActionType::
                ConfirmClock:
                break;

            default:
                return;
        }

        if (
            !isValidDate(clockDraft_.date) ||
            !isValidTime(clockDraft_.time)
        )
        {
            clockError_ =
                ClockError::InvalidDateTime;
            return;
        }

        const ClockError setError =
            clock_.set(clockDraft_);

        if (setError != ClockError::None)
        {
            clockError_ = setError;
            return;
        }

        lastNow_ = clockDraft_;

        displayedMonth_ = YearMonth{
            clockDraft_.date.year,
            clockDraft_.date.month
        };

        clockError_ = ClockError::None;
        screen_ = AgendaUiScreen::Month;
        return;
    }

    if (screen_ == AgendaUiScreen::Month)
    {
        switch (action.type)
        {
            case AgendaUiActionType::
                PreviousMonth:
            {
                const std::optional<YearMonth>
                    previous =
                        previousMonth(
                            displayedMonth_
                        );

                if (previous.has_value())
                {
                    displayedMonth_ = *previous;
                }

                return;
            }

            case AgendaUiActionType::
                NextMonth:
            {
                const std::optional<YearMonth>
                    next =
                        nextMonth(
                            displayedMonth_
                        );

                if (next.has_value())
                {
                    displayedMonth_ = *next;
                }

                return;
            }

            case AgendaUiActionType::
                SelectDate:
                if (
                    !action.date.has_value() ||
                    !isValidDate(*action.date)
                )
                {
                    return;
                }

                selectedDate_ = *action.date;

                displayedMonth_ = YearMonth{
                    action.date->year,
                    action.date->month
                };

                selectedEventId_.reset();
                dayPageIndex_ = 0;
                screen_ = AgendaUiScreen::Day;
                return;

            default:
                return;
        }
    }

    if (screen_ == AgendaUiScreen::Day)
    {
        if (
            action.type ==
            AgendaUiActionType::StartCreate
        )
        {
            beginCreate();
            return;
        }

        if (
            action.type ==
            AgendaUiActionType::
                PreviousDayPage
        )
        {
            clampDayPage();

            if (dayPageIndex_ > 0)
            {
                --dayPageIndex_;
            }

            return;
        }

        if (
            action.type ==
            AgendaUiActionType::NextDayPage
        )
        {
            clampDayPage();

            if (!selectedDate_.has_value())
            {
                return;
            }

            const std::size_t count =
                eventsForDate(
                    controller_.agenda(),
                    *selectedDate_
                ).size();

            const std::size_t pageCount =
                count == 0
                    ? 1
                    : (
                        count +
                        AGENDA_UI_EVENTS_PER_PAGE -
                        1
                    ) /
                        AGENDA_UI_EVENTS_PER_PAGE;

            if (
                dayPageIndex_ + 1 <
                pageCount
            )
            {
                ++dayPageIndex_;
            }

            return;
        }

        if (
            action.type !=
            AgendaUiActionType::SelectEvent
        )
        {
            return;
        }

        if (!action.eventId.has_value())
        {
            return;
        }

        const Event* selected =
            controller_.agenda().findEvent(
                *action.eventId
            );

        if (
            selected == nullptr ||
            !selectedDate_.has_value() ||
            !sameDate(
                selected->eventDate,
                *selectedDate_
            )
        )
        {
            return;
        }

        selectedEventId_ = selected->id;
        screen_ = AgendaUiScreen::EventDetails;
        return;
    }
}

AgendaUiView AgendaUiStateMachine::view() const
{
    AgendaUiView result;

    result.screen = screen_;
    result.overlay = overlay_;
    result.controllerError = controllerError_;
    result.eventError = eventError_;
    result.clockError = clockError_;
    result.displayedMonth = displayedMonth_;
    result.clockDraft = clockDraft_;
    result.selectedDate = selectedDate_;
    result.dayPageIndex = dayPageIndex_;
    result.editor = editor_;
    result.t9Language = t9_.language();
    result.t9Mode = t9_.mode();
    result.t9Uppercase = t9_.uppercase();
    result.reminder = reminder_;

    if (!controller_.isReady())
    {
        return result;
    }

    rebuildMonthCells(result);
    rebuildDayRows(result);

    if (selectedEventId_.has_value())
    {
        const Event* selected =
            controller_.agenda().findEvent(
                *selectedEventId_
            );

        if (selected != nullptr)
        {
            result.selectedEvent = *selected;
        }
    }

    return result;
}

void AgendaUiStateMachine::rebuildMonthCells(
    AgendaUiView& result
) const
{
    result.monthPreviousEnabled =
        previousMonth(
            displayedMonth_
        ).has_value();

    result.monthNextEnabled =
        nextMonth(
            displayedMonth_
        ).has_value();

    const MonthGrid grid =
        buildMonthGrid(displayedMonth_);

    for (
        std::size_t index = 0;
        index < grid.size();
        ++index
    )
    {
        AgendaUiMonthCell& target =
            result.monthCells[index];

        target.date = grid[index].date;

        target.inDisplayedMonth =
            grid[index].inDisplayedMonth;

        if (!target.date.has_value())
        {
            continue;
        }

        target.isToday =
            sameDate(
                *target.date,
                lastNow_.date
            );

        target.isSelected =
            selectedDate_.has_value() &&
            sameDate(
                *target.date,
                *selectedDate_
            );

        for (
            const Event& event :
            controller_.agenda().events()
        )
        {
            if (
                sameDate(
                    event.eventDate,
                    *target.date
                )
            )
            {
                target.hasEvents = true;
                break;
            }
        }
    }
}

void AgendaUiStateMachine::rebuildDayRows(
    AgendaUiView& result
) const
{
    result.dayEvents.clear();

    if (!selectedDate_.has_value())
    {
        result.dayPageCount = 1;
        return;
    }

    const std::vector<const Event*> events =
        eventsForDate(
            controller_.agenda(),
            *selectedDate_
        );

    result.dayPageCount =
        events.empty()
            ? 1
            : (
                events.size() +
                AGENDA_UI_EVENTS_PER_PAGE -
                1
            ) /
                AGENDA_UI_EVENTS_PER_PAGE;

    result.dayPreviousEnabled =
        dayPageIndex_ > 0;

    result.dayNextEnabled =
        dayPageIndex_ + 1 <
        result.dayPageCount;

    const std::size_t first =
        dayPageIndex_ *
        AGENDA_UI_EVENTS_PER_PAGE;

    const std::size_t last =
        std::min(
            first +
                AGENDA_UI_EVENTS_PER_PAGE,
            events.size()
        );

    for (
        std::size_t index = first;
        index < last;
        ++index
    )
    {
        result.dayEvents.push_back(
            AgendaUiEventRow{
                events[index]->id,
                events[index]->eventTime,
                firstUiCodePoints(
                    events[index]->text
                )
            }
        );
    }
}

void AgendaUiStateMachine::clampDayPage()
{
    if (!selectedDate_.has_value())
    {
        dayPageIndex_ = 0;
        return;
    }

    const std::size_t count =
        eventsForDate(
            controller_.agenda(),
            *selectedDate_
        ).size();

    const std::size_t pageCount =
        count == 0
            ? 1
            : (
                count +
                AGENDA_UI_EVENTS_PER_PAGE -
                1
            ) /
                AGENDA_UI_EVENTS_PER_PAGE;

    if (dayPageIndex_ >= pageCount)
    {
        dayPageIndex_ = pageCount - 1;
    }
}

void AgendaUiStateMachine::beginCreate()
{
    if (!selectedDate_.has_value())
    {
        return;
    }

    editor_ = AgendaUiEditorDraft{};

    editor_.mode =
        AgendaUiEditorMode::Create;

    editor_.eventId.reset();
    editor_.date = *selectedDate_;
    editor_.time.reset();
    editor_.text.clear();
    editor_.dirty = false;
    t9_ = T9Engine{};
    t9_.toggleLanguage();
    t9_.setText(editor_.text);

    editorReturnScreen_ =
        AgendaUiScreen::Day;

    screen_ =
        AgendaUiScreen::EditorDate;
}

void AgendaUiStateMachine::beginEdit()
{
    if (!selectedEventId_.has_value())
    {
        return;
    }

    const Event* event =
        controller_.agenda().findEvent(
            *selectedEventId_
        );

    if (event == nullptr)
    {
        selectedEventId_.reset();
        screen_ = AgendaUiScreen::Day;
        return;
    }

    editor_ = AgendaUiEditorDraft{};

    editor_.mode =
        AgendaUiEditorMode::Update;

    editor_.eventId = event->id;
    editor_.date = event->eventDate;
    editor_.time = event->eventTime;
    editor_.text = event->text;
    editor_.dirty = false;
    t9_ = T9Engine{};
    t9_.toggleLanguage();
    t9_.setText(editor_.text);

    editorReturnScreen_ =
        AgendaUiScreen::EventDetails;

    screen_ =
        AgendaUiScreen::EditorDate;
}

void AgendaUiStateMachine::
    leaveEditorAfterDiscard()
{
    screen_ = editorReturnScreen_;
    overlay_ = AgendaUiOverlay::None;
    editor_ = AgendaUiEditorDraft{};
    t9_ = T9Engine{};
}

void AgendaUiStateMachine::saveEditor()
{
    if (
        screen_ !=
        AgendaUiScreen::EditorReview
    )
    {
        return;
    }

    resetErrors();

    t9_.commitPending();
    editor_.text = t9_.text();

    const ClockReadResult current =
        clock_.read();

    pendingOperation_ =
        editor_.mode ==
            AgendaUiEditorMode::Create
            ? AgendaUiPendingOperation::
                CreateEvent
            : AgendaUiPendingOperation::
                UpdateEvent;

    if (current.error != ClockError::None)
    {
        clockError_ = current.error;
        overlay_ = AgendaUiOverlay::ClockError;
        return;
    }

    if (!validDateTime(current.value))
    {
        clockError_ =
            ClockError::InvalidDateTime;

        overlay_ =
            AgendaUiOverlay::ClockError;

        return;
    }

    lastNow_ = current.value;

    const EventDraft draft{
        editor_.date,
        editor_.time,
        editor_.text
    };

    AgendaControllerError controllerError =
        AgendaControllerError::None;

    EventError eventError =
        EventError::None;

    if (
        editor_.mode ==
        AgendaUiEditorMode::Create
    )
    {
        const AgendaControllerCreateResult
            created =
                controller_.createEvent(
                    draft,
                    current.value
                );

        controllerError = created.error;
        eventError = created.eventError;

        pendingOperation_ =
            AgendaUiPendingOperation::
                CreateEvent;
    }
    else
    {
        if (!editor_.eventId.has_value())
        {
            eventError_ =
                EventError::EventNotFound;

            pendingOperation_ =
                AgendaUiPendingOperation::None;

            return;
        }

        const AgendaControllerChangeResult
            updated =
                controller_.updateEvent(
                    *editor_.eventId,
                    draft,
                    current.value
                );

        controllerError = updated.error;
        eventError = updated.eventError;

        pendingOperation_ =
            AgendaUiPendingOperation::
                UpdateEvent;
    }

    if (eventError != EventError::None)
    {
        eventError_ = eventError;

        pendingOperation_ =
            AgendaUiPendingOperation::None;

        return;
    }

    if (
        controllerError !=
        AgendaControllerError::None
    )
    {
        controllerError_ = controllerError;

        overlay_ =
            AgendaUiOverlay::OperationError;

        return;
    }

    const Date savedDate = editor_.date;

    editor_ = AgendaUiEditorDraft{};

    pendingOperation_ =
        AgendaUiPendingOperation::None;

    selectedDate_ = savedDate;
    selectedEventId_.reset();

    displayedMonth_ = YearMonth{
        savedDate.year,
        savedDate.month
    };

    screen_ = AgendaUiScreen::Month;
    overlay_ = AgendaUiOverlay::None;

    resetErrors();
}

void AgendaUiStateMachine::confirmDelete()
{
    if (!selectedEventId_.has_value())
    {
        screen_ = AgendaUiScreen::Day;
        return;
    }

    resetErrors();

    const AgendaControllerChangeResult result =
        controller_.deleteEvent(
            *selectedEventId_
        );

    if (
        result.eventError ==
        EventError::EventNotFound
    )
    {
        selectedEventId_.reset();

        pendingOperation_ =
            AgendaUiPendingOperation::None;

        screen_ = AgendaUiScreen::Day;

        clampDayPage();
        return;
    }

    if (
        result.eventError !=
        EventError::None
    )
    {
        eventError_ = result.eventError;
        return;
    }

    if (
        result.error !=
        AgendaControllerError::None
    )
    {
        controllerError_ = result.error;

        pendingOperation_ =
            AgendaUiPendingOperation::
                DeleteEvent;

        overlay_ =
            AgendaUiOverlay::OperationError;

        return;
    }

    selectedEventId_.reset();

    pendingOperation_ =
        AgendaUiPendingOperation::None;

    overlay_ = AgendaUiOverlay::None;
    screen_ = AgendaUiScreen::Day;

    clampDayPage();
}

void AgendaUiStateMachine::
    retryPendingOperation()
{
    if (
        pendingOperation_ ==
        AgendaUiPendingOperation::DeleteEvent
    )
    {
        confirmDelete();
        return;
    }

    if (
        pendingOperation_ ==
            AgendaUiPendingOperation::
                CreateEvent ||
        pendingOperation_ ==
            AgendaUiPendingOperation::
                UpdateEvent
    )
    {
        saveEditor();
    }
}

void AgendaUiStateMachine::resetErrors()
{
    controllerError_ =
        AgendaControllerError::None;

    eventError_ = EventError::None;
    clockError_ = ClockError::None;
}