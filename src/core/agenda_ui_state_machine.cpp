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

void moveToNextDate(Date& value)
{
    if (!isValidDate(value))
    {
        return;
    }

    if (
        value.year == 2099 &&
        value.month == 12 &&
        value.day == 31
    )
    {
        return;
    }

    const int finalDay =
        daysInMonth(value.year, value.month);

    if (value.day < finalDay)
    {
        ++value.day;
        return;
    }

    value.day = 1;

    if (value.month == 12)
    {
        value.month = 1;
        ++value.year;
        return;
    }

    ++value.month;
}

void moveToPreviousDate(Date& value)
{
    if (!isValidDate(value))
    {
        return;
    }

    if (
        value.year == 2000 &&
        value.month == 1 &&
        value.day == 1
    )
    {
        return;
    }

    if (value.day > 1)
    {
        --value.day;
        return;
    }

    if (value.month == 1)
    {
        --value.year;
        value.month = 12;
    }
    else
    {
        --value.month;
    }

    value.day =
        daysInMonth(value.year, value.month);
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
      controllerError_(AgendaControllerError::None),
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
      editorReturnScreen_(AgendaUiScreen::Day),
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

    if (screen_ == AgendaUiScreen::Month)
    {
        if (
            action.type ==
            AgendaUiActionType::PreviousMonth
        )
        {
            const std::optional<YearMonth> previous =
                previousMonth(displayedMonth_);

            if (previous.has_value())
            {
                displayedMonth_ =
                    previous.value();
            }

            return;
        }

        if (
            action.type ==
            AgendaUiActionType::NextMonth
        )
        {
            const std::optional<YearMonth> next =
                nextMonth(displayedMonth_);

            if (next.has_value())
            {
                displayedMonth_ =
                    next.value();
            }

            return;
        }

        if (
            action.type ==
                AgendaUiActionType::SelectDate &&
            action.date.has_value() &&
            isValidDate(action.date.value())
        )
        {
            const Date selected =
                action.date.value();

            selectedDate_ = selected;

            displayedMonth_ = YearMonth{
                selected.year,
                selected.month
            };

            selectedEventId_.reset();
            dayPageIndex_ = 0;
            screen_ = AgendaUiScreen::Day;

            return;
        }

        return;
    }

    if (
        screen_ !=
        AgendaUiScreen::ClockSetup
    )
    {
        return;
    }

    if (
        action.type ==
        AgendaUiActionType::ClockPreviousDay
    )
    {
        moveToPreviousDate(
            clockDraft_.date
        );

        return;
    }

    if (
        action.type ==
        AgendaUiActionType::ClockNextDay
    )
    {
        moveToNextDate(
            clockDraft_.date
        );

        return;
    }

    if (
        action.type ==
        AgendaUiActionType::ClockPreviousHour
    )
    {
        clockDraft_.time.hour =
            (clockDraft_.time.hour + 23) % 24;

        return;
    }

    if (
        action.type ==
        AgendaUiActionType::ClockNextHour
    )
    {
        clockDraft_.time.hour =
            (clockDraft_.time.hour + 1) % 24;

        return;
    }

    if (
        action.type ==
        AgendaUiActionType::ClockPreviousMinute
    )
    {
        clockDraft_.time.minute =
            (clockDraft_.time.minute + 59) % 60;

        return;
    }

    if (
        action.type ==
        AgendaUiActionType::ClockNextMinute
    )
    {
        clockDraft_.time.minute =
            (clockDraft_.time.minute + 1) % 60;

        return;
    }

    if (
        action.type !=
        AgendaUiActionType::ConfirmClock
    )
    {
        return;
    }

    if (!validDateTime(clockDraft_))
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
}

AgendaUiView AgendaUiStateMachine::view() const
{
    AgendaUiView result;

    result.screen = screen_;
    result.overlay = overlay_;

    result.controllerError =
        controllerError_;

    result.eventError = eventError_;
    result.clockError = clockError_;

    result.displayedMonth =
        displayedMonth_;

    result.monthPreviousEnabled =
        previousMonth(
            displayedMonth_
        ).has_value();

    result.monthNextEnabled =
        nextMonth(
            displayedMonth_
        ).has_value();

    result.clockDraft = clockDraft_;
    result.selectedDate = selectedDate_;

    result.dayPageIndex =
        dayPageIndex_;

    result.editor = editor_;

    result.t9Language =
        t9_.language();

    result.t9Mode =
        t9_.mode();

    result.reminder = reminder_;

    if (controller_.isReady())
    {
        rebuildMonthCells(result);
    }

    return result;
}

void AgendaUiStateMachine::rebuildMonthCells(
    AgendaUiView& result
) const
{
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

        target.date =
            grid[index].date;

        target.inDisplayedMonth =
            grid[index].inDisplayedMonth;

        target.isToday =
            target.date.has_value() &&
            sameDate(
                target.date.value(),
                lastNow_.date
            );

        target.isSelected =
            target.date.has_value() &&
            selectedDate_.has_value() &&
            sameDate(
                target.date.value(),
                selectedDate_.value()
            );

        if (!target.date.has_value())
        {
            continue;
        }

        for (
            const Event& event :
            controller_.agenda().events()
        )
        {
            if (
                sameDate(
                    event.eventDate,
                    target.date.value()
                )
            )
            {
                target.hasEvents = true;
                break;
            }
        }
    }
}

void AgendaUiStateMachine::resetErrors()
{
    controllerError_ =
        AgendaControllerError::None;

    eventError_ = EventError::None;
    clockError_ = ClockError::None;
}