#include <cassert>
#include <iostream>
#include <string>

#include "core/event.h"

const DateTime NOW{
    {2026, 9, 3},
    {12, 0}
};

std::string repeatedGreekCharacter(std::size_t count)
{
    std::string result;

    for (std::size_t index = 0; index < count; ++index)
    {
        result += u8"ά";
    }

    return result;
}

void test_accepts_future_timed_event()
{
    EventDraft draft{
        {2026, 9, 4},
        TimeOfDay{18, 30},
        "Training"
    };

    assert(validateDraft(draft, NOW) == EventError::None);
}

void test_accepts_all_day_event_for_today()
{
    EventDraft draft{
        {2026, 9, 3},
        std::nullopt,
        u8"Σημερινό γεγονός"
    };

    assert(validateDraft(draft, NOW) == EventError::None);
}

void test_rejects_invalid_date_first()
{
    EventDraft draft{
        {2026, 2, 31},
        TimeOfDay{25, 0},
        ""
    };

    assert(validateDraft(draft, NOW) == EventError::InvalidDate);
}

void test_rejects_invalid_time()
{
    EventDraft invalidHour{
        {2026, 9, 4},
        TimeOfDay{24, 0},
        "Bad hour"
    };

    EventDraft invalidMinute{
        {2026, 9, 4},
        TimeOfDay{18, 60},
        "Bad minute"
    };

    assert(
        validateDraft(invalidHour, NOW) ==
        EventError::InvalidTime
    );

    assert(
        validateDraft(invalidMinute, NOW) ==
        EventError::InvalidTime
    );
}

void test_rejects_empty_text()
{
    EventDraft draft{
        {2026, 9, 4},
        std::nullopt,
        ""
    };

    assert(validateDraft(draft, NOW) == EventError::EmptyText);
}

void test_accepts_exactly_500_unicode_characters()
{
    EventDraft draft{
        {2026, 9, 4},
        std::nullopt,
        repeatedGreekCharacter(500)
    };

    assert(validateDraft(draft, NOW) == EventError::None);
}

void test_rejects_501_unicode_characters()
{
    EventDraft draft{
        {2026, 9, 4},
        std::nullopt,
        repeatedGreekCharacter(501)
    };

    assert(
        validateDraft(draft, NOW) ==
        EventError::TextTooLong
    );
}

void test_rejects_timed_event_before_now()
{
    EventDraft draft{
        {2026, 9, 3},
        TimeOfDay{11, 59},
        "Past event"
    };

    assert(validateDraft(draft, NOW) == EventError::PastEvent);
}

void test_rejects_timed_event_exactly_at_now()
{
    EventDraft draft{
        {2026, 9, 3},
        TimeOfDay{12, 0},
        "Not future"
    };

    assert(validateDraft(draft, NOW) == EventError::PastEvent);
}

void test_rejects_all_day_event_before_today()
{
    EventDraft draft{
        {2026, 9, 2},
        std::nullopt,
        "Yesterday"
    };

    assert(validateDraft(draft, NOW) == EventError::PastEvent);
}

void test_timed_event_becomes_past_at_its_time()
{
    Event event{
        1,
        {2026, 9, 3},
        TimeOfDay{18, 30},
        "Training",
        false,
        {{2026, 9, 1}, {10, 0}}
    };

    assert(!isPast(event, {{2026, 9, 3}, {18, 29}}));
    assert(isPast(event, {{2026, 9, 3}, {18, 30}}));
}

void test_all_day_event_becomes_past_next_day()
{
    Event event{
        2,
        {2026, 9, 3},
        std::nullopt,
        "All-day event",
        false,
        {{2026, 9, 1}, {10, 0}}
    };

    assert(!isPast(event, {{2026, 9, 3}, {23, 59}}));
    assert(isPast(event, {{2026, 9, 4}, {0, 0}}));
}

int main()
{
    test_accepts_future_timed_event();
    test_accepts_all_day_event_for_today();
    test_rejects_invalid_date_first();
    test_rejects_invalid_time();
    test_rejects_empty_text();
    test_accepts_exactly_500_unicode_characters();
    test_rejects_501_unicode_characters();
    test_rejects_timed_event_before_now();
    test_rejects_timed_event_exactly_at_now();
    test_rejects_all_day_event_before_today();
    test_timed_event_becomes_past_at_its_time();
    test_all_day_event_becomes_past_next_day();

    std::cout << "All event tests passed.\n";

    return 0;
}