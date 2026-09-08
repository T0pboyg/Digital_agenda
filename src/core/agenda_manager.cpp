#include "agenda_manager.h"

#include <algorithm>

namespace
{
bool eventComesBefore(
    const Event& left,
    const Event& right
)
{
    const auto leftDay =
        serialDay(left.eventDate);

    const auto rightDay =
        serialDay(right.eventDate);

    if (leftDay != rightDay)
    {
        return leftDay < rightDay;
    }

    if (
        left.eventTime.has_value() !=
        right.eventTime.has_value()
    )
    {
        return !left.eventTime.has_value();
    }

    if (left.eventTime.has_value())
    {
        const auto leftMinute =
            serialMinute(
                DateTime{
                    left.eventDate,
                    left.eventTime.value()
                }
            );

        const auto rightMinute =
            serialMinute(
                DateTime{
                    right.eventDate,
                    right.eventTime.value()
                }
            );

        if (leftMinute != rightMinute)
        {
            return leftMinute < rightMinute;
        }
    }

    return left.id < right.id;
}
}

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

    if (events_.size() >= MAX_AGENDA_EVENTS)
    {
        return CreateEventResult{
            EventError::AgendaFull,
            0
        };
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
    sortEvents();

    return CreateEventResult{
        EventError::None,
        assignedId
    };
}

EventError AgendaManager::updateEvent(
    std::uint32_t id,
    const EventDraft& draft,
    const DateTime& now
)
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
        return EventError::EventNotFound;
    }

    const EventError error =
        validateDraft(draft, now);

    if (error != EventError::None)
    {
        return error;
    }

    iterator->eventDate = draft.eventDate;
    iterator->eventTime = draft.eventTime;
    iterator->text = draft.text;
    iterator->reminderSeen = false;

    sortEvents();

    return EventError::None;
}

bool AgendaManager::deleteEvent(
    std::uint32_t id
)
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
        return false;
    }

    events_.erase(iterator);

    return true;
}

bool AgendaManager::markReminderSeen(
    std::uint32_t id
)
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
        return false;
    }

    iterator->reminderSeen = true;
    return true;
}

AgendaState AgendaManager::snapshot() const
{
    return AgendaState{
        events_,
        nextId_
    };
}

bool AgendaManager::restoreState(
    const AgendaState& state
)
{
    if (
        validateAgendaState(state) !=
        AgendaStateError::None
    )
    {
        return false;
    }

    events_ = state.events;
    nextId_ = state.nextId;

    sortEvents();

    return true;
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

void AgendaManager::sortEvents()
{
    std::sort(
        events_.begin(),
        events_.end(),
        eventComesBefore
    );
}