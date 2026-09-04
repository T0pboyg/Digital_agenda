#include "event.h"

#include "utf8.h"

EventError validateDraft(
    const EventDraft& draft,
    const DateTime& now
)
{
    if (!isValidDate(draft.eventDate))
    {
        return EventError::InvalidDate;
    }

    if (
        draft.eventTime.has_value() &&
        !isValidTime(draft.eventTime.value())
    )
    {
        return EventError::InvalidTime;
    }

    if (draft.text.empty())
    {
        return EventError::EmptyText;
    }

    if (utf8CodePointCount(draft.text) > 500)
    {
        return EventError::TextTooLong;
    }

    if (draft.eventTime.has_value())
    {
        DateTime eventDateTime{
            draft.eventDate,
            draft.eventTime.value()
        };

        if (
            serialMinute(eventDateTime) <=
            serialMinute(now)
        )
        {
            return EventError::PastEvent;
        }
    }
    else
    {
        if (
            serialDay(draft.eventDate) <
            serialDay(now.date)
        )
        {
            return EventError::PastEvent;
        }
    }

    return EventError::None;
}

bool isPast(
    const Event& event,
    const DateTime& now
)
{
    if (event.eventTime.has_value())
    {
        DateTime eventDateTime{
            event.eventDate,
            event.eventTime.value()
        };

        return serialMinute(eventDateTime) <=
               serialMinute(now);
    }

    return serialDay(event.eventDate) <
           serialDay(now.date);
}