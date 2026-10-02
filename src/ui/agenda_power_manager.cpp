#include "agenda_power_manager.h"

bool agendaViewAllowsDeepSleep(
    const AgendaUiView& view
)
{
    if (view.overlay != AgendaUiOverlay::None)
    {
        return false;
    }

    return
        view.screen == AgendaUiScreen::Month ||
        view.screen == AgendaUiScreen::Day ||
        view.screen == AgendaUiScreen::EventDetails;
}

void AgendaPowerManager::begin(
    std::uint32_t nowMilliseconds
)
{
    lastActivityMilliseconds_ = nowMilliseconds;
    started_ = true;
}

void AgendaPowerManager::recordActivity(
    std::uint32_t nowMilliseconds
)
{
    lastActivityMilliseconds_ = nowMilliseconds;
    started_ = true;
}

AgendaPowerDecision AgendaPowerManager::decide(
    std::uint32_t nowMilliseconds,
    const AgendaUiView& view
) const
{
    if (
        !started_ ||
        view.overlay == AgendaUiOverlay::Reminder ||
        view.overlay == AgendaUiOverlay::Alarm
    )
    {
        return AgendaPowerDecision{
            AgendaPowerAction::None,
            std::nullopt
        };
    }

    const std::uint32_t inactiveMilliseconds =
        static_cast<std::uint32_t>(
            nowMilliseconds -
            lastActivityMilliseconds_);

    if (
        inactiveMilliseconds <
        AgendaLightSleepTimeoutMilliseconds
    )
    {
        return AgendaPowerDecision{
            AgendaPowerAction::None,
            std::nullopt
        };
    }

    if (
        inactiveMilliseconds >=
        AgendaDeepSleepTimeoutMilliseconds
    )
    {
        return AgendaPowerDecision{
            agendaViewAllowsDeepSleep(view)
                ? AgendaPowerAction::EnterDeepSleep
                : AgendaPowerAction::EnterLightSleep,
            std::nullopt
        };
    }

    return AgendaPowerDecision{
        AgendaPowerAction::EnterLightSleep,
        AgendaDeepSleepTimeoutMilliseconds -
            inactiveMilliseconds
    };
}
