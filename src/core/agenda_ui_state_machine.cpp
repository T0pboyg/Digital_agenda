#include "agenda_ui_state_machine.h"

namespace
{
bool validDateTime(
    const DateTime& value
)
{
    return
        isValidDate(value.date) &&
        isValidTime(value.time);
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
    result.reminder = reminder_;

    return result;
}

void AgendaUiStateMachine::resetErrors()
{
    controllerError_ =
        AgendaControllerError::None;

    eventError_ = EventError::None;
    clockError_ = ClockError::None;
}