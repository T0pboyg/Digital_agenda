#pragma once

#include <cstdint>
#include <optional>

#include "core/agenda_ui_types.h"

constexpr std::uint32_t AgendaLightSleepTimeoutMilliseconds =
    45000;

constexpr std::uint32_t AgendaDeepSleepTimeoutMilliseconds =
    300000;

enum class AgendaPowerAction
{
    None,
    EnterLightSleep,
    EnterDeepSleep
};

struct AgendaPowerDecision
{
    AgendaPowerAction action;

    std::optional<std::uint32_t>
        policyWakeAfterMilliseconds;
};

bool agendaViewAllowsDeepSleep(
    const AgendaUiView& view
);

class AgendaPowerManager
{
public:
    void begin(
        std::uint32_t nowMilliseconds
    );

    void recordActivity(
        std::uint32_t nowMilliseconds
    );

    AgendaPowerDecision decide(
        std::uint32_t nowMilliseconds,
        const AgendaUiView& view
    ) const;

private:
    std::uint32_t lastActivityMilliseconds_ = 0;
    bool started_ = false;
};
