#include "agenda_runtime.h"

namespace
{
    constexpr std::uint32_t TouchPollIntervalMilliseconds = 10;
    constexpr std::uint32_t UiTickIntervalMilliseconds = 60000;

    bool intervalElapsed(
        std::uint32_t now,
        std::uint32_t previous,
        std::uint32_t interval)
    {
        return static_cast<std::uint32_t>(now - previous) >=
               interval;
    }
}

AgendaRuntime::AgendaRuntime(
    TouchInput &touchInput,
    TouchReleaseDetector &releaseDetector,
    AgendaTouchMapper &touchMapper,
    AgendaUiStateMachine &stateMachine,
    AgendaRenderer &renderer,
    DrawSurface &surface)
    : touchInput_(touchInput),
      releaseDetector_(releaseDetector),
      touchMapper_(touchMapper),
      stateMachine_(stateMachine),
      renderer_(renderer),
      surface_(surface)
{
}

void AgendaRuntime::begin(
    std::uint32_t nowMilliseconds)
{
    lastTouchPollMilliseconds_ = nowMilliseconds;
    lastTickMilliseconds_ = nowMilliseconds;
    started_ = true;
}

void AgendaRuntime::suppressActionsUntilRelease()
{
    releaseDetector_.reset();
    suppressUntilRelease_ = true;
}

AgendaRuntimeUpdateResult AgendaRuntime::update(
    std::uint32_t nowMilliseconds)
{
    AgendaRuntimeUpdateResult result;

    if (!started_)
    {
        return result;
    }

    bool renderRequired = false;

    if (
        intervalElapsed(
            nowMilliseconds,
            lastTouchPollMilliseconds_,
            TouchPollIntervalMilliseconds))
    {
        lastTouchPollMilliseconds_ = nowMilliseconds;
        result.touchPolled = true;

        const TouchInputReadResult input =
            touchInput_.read();

        result.touchError = input.error;

        if (input.error == TouchInputError::None)
        {
            result.touchActivity =
                input.sample.touched &&
                input.sample.x < AGENDA_UI_DISPLAY_WIDTH &&
                input.sample.y < AGENDA_UI_DISPLAY_HEIGHT;

            if (suppressUntilRelease_)
            {
                if (!input.sample.touched)
                {
                    suppressUntilRelease_ = false;
                    releaseDetector_.reset();
                }
            }
            else
            {
                const std::optional<UiPoint> released =
                    releaseDetector_.update(input.sample);

                if (released.has_value())
                {
                    const std::optional<AgendaUiAction> action =
                        touchMapper_.map(
                            stateMachine_.view(),
                            *released,
                            nowMilliseconds);

                    if (action.has_value())
                    {
                        stateMachine_.handle(*action);
                        result.actionHandled = true;
                        renderRequired = true;
                    }
                }
            }
        }
    }

    if (
        intervalElapsed(
            nowMilliseconds,
            lastTickMilliseconds_,
            stateMachine_.hasAlarms() ? 1000 : UiTickIntervalMilliseconds))
    {
        lastTickMilliseconds_ = nowMilliseconds;
        const auto before = stateMachine_.view();
        stateMachine_.tick();
        const auto after = stateMachine_.view();
        result.ticked = true;
        const bool timeChanged = before.currentTime.has_value() != after.currentTime.has_value() ||
            (before.currentTime && after.currentTime && serialMinute(*before.currentTime) != serialMinute(*after.currentTime));
        renderRequired = renderRequired || !stateMachine_.hasAlarms() || timeChanged ||
            before.overlay != after.overlay || before.alarmError != after.alarmError;
    }

    if (renderRequired)
    {
        renderer_.render(
            stateMachine_.view(),
            surface_);

        result.rendered = true;
    }

    return result;
}
