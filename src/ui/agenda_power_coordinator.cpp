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
    WakeDelayClock& wakeDelayClock,
    AlarmService* alarms
)
    : alarms_(alarms),
      manager_(manager),
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
    if (alarms_ && !alarms_->canSleep())
    {
        return {AgendaSleepMode::None, std::nullopt, alarms_->ringing(),
            alarms_->ringing() ? AgendaPowerCoordinatorError::None : AgendaPowerCoordinatorError::AlarmScheduleFailed};
    }

    std::optional<std::uint64_t> policyWake =
        policyTimer(policyWakeAfterMilliseconds);
    // Even a failed clock/schedule read must retry while alarms are enabled.
    // Otherwise an unsaved editor can enter light sleep with no wake timer.
    if (alarms_ && std::any_of(alarms_->alarms().begin(), alarms_->alarms().end(),
            [](const Alarm& alarm) { return alarm.enabled; }) &&
        (!policyWake || *policyWake > 60000000ULL))
        policyWake = 60000000ULL;

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

    std::optional<DateTime> target = schedule.scheduledAt;
    if (alarms_)
    {
        const auto alarmTarget = alarms_->nextWake(current.value);
        if (alarmTarget && (!target || serialMinute(*alarmTarget) < serialMinute(*target))) target = alarmTarget;
        // Re-evaluate civil-time schedules across DST, including a skipped hour.
        if (alarmTarget && (!wake || *wake > 60000000ULL)) wake = 60000000ULL;
    }

    if (target.has_value())
    {
        const WakeDelayResult delay =
            wakeDelayClock_.delayUntil(
                *target);

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
