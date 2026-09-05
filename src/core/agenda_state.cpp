#include "agenda_state.h"

#include <unordered_set>

#include "utf8.h"

AgendaStateError validateAgendaState(
    const AgendaState& state
)
{
    if (state.events.size() > MAX_AGENDA_EVENTS)
    {
        return AgendaStateError::TooManyEvents;
    }

    if (state.nextId == 0)
    {
        return AgendaStateError::InvalidNextId;
    }

    std::unordered_set<std::uint32_t> seenIds;

    for (const Event& event : state.events)
    {
        if (event.id == 0)
        {
            return AgendaStateError::InvalidEvent;
        }

        if (!seenIds.insert(event.id).second)
        {
            return AgendaStateError::DuplicateId;
        }

        if (event.id >= state.nextId)
        {
            return AgendaStateError::InvalidNextId;
        }

        if (!isValidDate(event.eventDate))
        {
            return AgendaStateError::InvalidEvent;
        }

        if (
            event.eventTime.has_value() &&
            !isValidTime(event.eventTime.value())
        )
        {
            return AgendaStateError::InvalidEvent;
        }

        if (
            !isValidDate(event.createdAt.date) ||
            !isValidTime(event.createdAt.time)
        )
        {
            return AgendaStateError::InvalidEvent;
        }

        if (
            event.text.empty() ||
            !isValidUtf8(event.text) ||
            utf8CodePointCount(event.text) > 500
        )
        {
            return AgendaStateError::InvalidEvent;
        }
    }

    return AgendaStateError::None;
}