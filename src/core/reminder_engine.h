#pragma once

#include <cstdint>
#include <optional>

#include "agenda_manager.h"

enum class ReminderEngineError
{
    None,
    InvalidDateTime
};

struct ReminderCheckResult
{
    ReminderEngineError error;
    std::optional<std::uint32_t> eventId;
};

ReminderCheckResult findNextDueReminder(
    const AgendaManager& agenda,
    const DateTime& now
);