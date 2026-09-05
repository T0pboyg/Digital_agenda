#include "agenda_manager.h"

#include <algorithm>

CreateEventResult AgendaManager::createEvent(
    const EventDraft& draft,
    const DateTime& now
)
{
    const EventError error =
        validateDraft(draft, now);

    if (error != EventError::None)
    {
        return CreateEventResult{error, 0};
    }

    const std::uint32_t assignedId = nextId_;

    events_.push_back(
        Event{
            assignedId,
            draft.eventDate,
            draft.eventTime,
            draft.text,
            false,
            now
        }
    );

    ++nextId_;

    return CreateEventResult{
        EventError::None,
        assignedId
    };
}

const Event* AgendaManager::findEvent(
    std::uint32_t id
) const
{
    const auto iterator = std::find_if(
        events_.begin(),
        events_.end(),
        [id](const Event& event)
        {
            return event.id == id;
        }
    );

    if (iterator == events_.end())
    {
        return nullptr;
    }

    return &(*iterator);
}

const std::vector<Event>&
AgendaManager::events() const
{
    return events_;
}

std::size_t AgendaManager::size() const
{
    return events_.size();
}

bool AgendaManager::empty() const
{
    return events_.empty();
}