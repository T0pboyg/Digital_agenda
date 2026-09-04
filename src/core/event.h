#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "date_time.h"

enum class EventError
{
    None,
    InvalidDate,
    InvalidTime,
    EmptyText,
    TextTooLong,
    PastEvent
};

struct EventDraft
{
    Date eventDate;
    std::optional<TimeOfDay> eventTime;
    std::string text;
};

struct Event
{
    std::uint32_t id;
    Date eventDate;
    std::optional<TimeOfDay> eventTime;
    std::string text;
    bool reminderSeen;
    DateTime createdAt;
};

EventError validateDraft(
    const EventDraft& draft,
    const DateTime& now
);

bool isPast(
    const Event& event,
    const DateTime& now
);