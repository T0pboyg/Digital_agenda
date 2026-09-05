#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "event.h"
#include "agenda_state.h"

struct CreateEventResult
{
    EventError error;
    std::uint32_t eventId;
};

class AgendaManager
{
public:
    CreateEventResult createEvent(
        const EventDraft& draft,
        const DateTime& now
    );

    EventError updateEvent(
        std::uint32_t id,
        const EventDraft& draft,
        const DateTime& now
    );

    bool deleteEvent(std::uint32_t id);
    AgendaState snapshot() const;

    bool restoreState(
        const AgendaState& state
    );

    const Event* findEvent(std::uint32_t id) const;
    const std::vector<Event>& events() const;

    std::size_t size() const;
    bool empty() const;

private:
    void sortEvents();

    std::vector<Event> events_;
    std::uint32_t nextId_ = 1;
};