#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "event.h"

constexpr std::size_t MAX_AGENDA_EVENTS = 500;

enum class AgendaStateError
{
    None,
    TooManyEvents,
    InvalidEvent,
    DuplicateId,
    InvalidNextId
};

struct AgendaState
{
    std::vector<Event> events;
    std::uint32_t nextId = 1;
};

AgendaStateError validateAgendaState(
    const AgendaState& state
);