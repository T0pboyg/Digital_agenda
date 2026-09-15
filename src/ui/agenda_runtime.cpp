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

    if (
        intervalElapsed(
            nowMilliseconds,
            lastTickMilliseconds_,
            UiTickIntervalMilliseconds))
    {
        lastTickMilliseconds_ = nowMilliseconds;
        stateMachine_.tick();
        result.ticked = true;
        renderRequired = true;
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