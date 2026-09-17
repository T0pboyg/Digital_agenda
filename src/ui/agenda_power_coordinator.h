#pragma once

#include <cstdint>
#include <optional>

#include "agenda_power_manager.h"
#include "power_hardware.h"
#include "core/agenda_controller.h"

enum class AgendaSleepMode
{
    None,
    Light,
    Deep
};

enum class AgendaPowerCoordinatorError
{
    None,
    ClockReadFailed,
    ReminderScheduleFailed,
    WakeDelayFailed
};

struct AgendaPowerPlan
{
    AgendaSleepMode mode;
    std::optional<std::uint64_t> timerMicroseconds;
    bool reminderDueNow;
    AgendaPowerCoordinatorError error;
};

class AgendaPowerCoordinator
{
public:
    AgendaPowerCoordinator(
        AgendaPowerManager& manager,
        AgendaController& controller,
        Clock& clock,
        WakeDelayClock& wakeDelayClock
    );

    void begin(
        std::uint32_t nowMilliseconds
    );

    void recordActivity(
        std::uint32_t nowMilliseconds
    );

    AgendaPowerPlan update(
        std::uint32_t nowMilliseconds,
        bool touchActivity,
        const AgendaUiView& view
    );

    AgendaPowerPlan planImmediateDeepSleep(
        const AgendaUiView& view
    ) const;

private:
    AgendaPowerPlan buildSleepPlan(
        AgendaSleepMode mode,
        const std::optional<std::uint32_t>&
            policyWakeAfterMilliseconds
    ) const;

    AgendaPowerManager& manager_;
    AgendaController& controller_;
    Clock& clock_;
    WakeDelayClock& wakeDelayClock_;
};
