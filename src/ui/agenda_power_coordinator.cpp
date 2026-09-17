#include "agenda_power_coordinator.h"

#include <algorithm>

namespace
{
    AgendaSleepMode modeFor(
        AgendaPowerAction action
    )
    {
        switch (action)
        {
        case AgendaPowerAction::EnterLightSleep:
            return AgendaSleepMode::Light;

        case AgendaPowerAction::EnterDeepSleep:
            return AgendaSleepMode::Deep;

        case AgendaPowerAction::None:
            break;
        }

        return AgendaSleepMode::None;
    }

    std::optional<std::uint64_t> policyTimer(
        const std::optional<std::uint32_t>& milliseconds
    )
    {
        if (!milliseconds.has_value())
        {
            return std::nullopt;
        }

        return static_cast<std::uint64_t>(*milliseconds) *
               1000ULL;
    }

    AgendaPowerPlan failedPlan(
        AgendaSleepMode requestedMode,
        const std::optional<std::uint64_t>& timer,
        AgendaPowerCoordinatorError error
    )
    {
        return AgendaPowerPlan{
            requestedMode == AgendaSleepMode::Light
                ? AgendaSleepMode::Light
                : AgendaSleepMode::None,
            requestedMode == AgendaSleepMode::Light
                ? timer
                : std::nullopt,
            false,
            error
        };
    }
}

AgendaPowerCoordinator::AgendaPowerCoordinator(
    AgendaPowerManager& manager,
    AgendaController& controller,
    Clock& clock,
    WakeDelayClock& wakeDelayClock
)
    : manager_(manager),
      controller_(controller),
      clock_(clock),
      wakeDelayClock_(wakeDelayClock)
{
}

void AgendaPowerCoordinator::begin(
    std::uint32_t nowMilliseconds
)
{
    manager_.begin(nowMilliseconds);
}

void AgendaPowerCoordinator::recordActivity(
    std::uint32_t nowMilliseconds
)
{
    manager_.recordActivity(nowMilliseconds);
}

AgendaPowerPlan AgendaPowerCoordinator::update(
    std::uint32_t nowMilliseconds,
    bool touchActivity,
    const AgendaUiView& view
)
{
    if (touchActivity)
    {
        manager_.recordActivity(nowMilliseconds);
    }

    const AgendaPowerDecision decision =
        manager_.decide(nowMilliseconds, view);

    if (decision.action == AgendaPowerAction::None)
    {
        return AgendaPowerPlan{
            AgendaSleepMode::None,
            std::nullopt,
            false,
            AgendaPowerCoordinatorError::None
        };
    }

    return buildSleepPlan(
        modeFor(decision.action),
        decision.policyWakeAfterMilliseconds);
}

AgendaPowerPlan AgendaPowerCoordinator::planImmediateDeepSleep(
    const AgendaUiView& view
) const
{
    if (!agendaViewAllowsDeepSleep(view))
    {
        return AgendaPowerPlan{
            AgendaSleepMode::None,
            std::nullopt,
            false,
            AgendaPowerCoordinatorError::None
        };
    }

    return buildSleepPlan(
        AgendaSleepMode::Deep,
        std::nullopt);
}

AgendaPowerPlan AgendaPowerCoordinator::buildSleepPlan(
    AgendaSleepMode mode,
    const std::optional<std::uint32_t>&
        policyWakeAfterMilliseconds
) const
{
    const std::optional<std::uint64_t> policyWake =
        policyTimer(policyWakeAfterMilliseconds);

    const ClockReadResult current = clock_.read();

    if (current.error != ClockError::None)
    {
        return failedPlan(
            mode,
            policyWake,
            AgendaPowerCoordinatorError::ClockReadFailed);
    }

    const AgendaControllerReminderScheduleResult schedule =
        controller_.reminderSchedule(current.value);

    if (
        schedule.error != AgendaControllerError::None ||
        schedule.reminderError != ReminderEngineError::None
    )
    {
        return failedPlan(
            mode,
            policyWake,
            AgendaPowerCoordinatorError::ReminderScheduleFailed);
    }

    if (schedule.dueNow)
    {
        return AgendaPowerPlan{
            AgendaSleepMode::None,
            std::nullopt,
            true,
            AgendaPowerCoordinatorError::None
        };
    }

    std::optional<std::uint64_t> wake = policyWake;

    if (schedule.scheduledAt.has_value())
    {
        const WakeDelayResult delay =
            wakeDelayClock_.delayUntil(
                *schedule.scheduledAt);

        if (delay.error != ClockError::None)
        {
            return failedPlan(
                mode,
                policyWake,
                AgendaPowerCoordinatorError::WakeDelayFailed);
        }

        if (delay.dueNow)
        {
            return AgendaPowerPlan{
                AgendaSleepMode::None,
                std::nullopt,
                true,
                AgendaPowerCoordinatorError::None
            };
        }

        if (
            !wake.has_value() ||
            delay.microseconds < *wake
        )
        {
            wake = delay.microseconds;
        }
    }

    return AgendaPowerPlan{
        mode,
        wake,
        false,
        AgendaPowerCoordinatorError::None
    };
}
