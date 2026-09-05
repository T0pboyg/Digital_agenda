#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <cstddef>
#include "core/agenda_state.h"

std::string repeatedGreekLetter(
    std::size_t count
)
{
    const std::string letter("\xCE\xB1", 2);
    std::string result;

    for (
        std::size_t index = 0;
        index < count;
        ++index
    )
    {
        result += letter;
    }

    return result;
}

Event storedEvent(std::uint32_t id)
{
    return Event{
        id,
        Date{2026, 9, 5},
        std::optional<TimeOfDay>{
            TimeOfDay{12, 30}
        },
        "Meeting",
        false,
        DateTime{
            Date{2026, 9, 4},
            TimeOfDay{10, 0}
        }
    };
}

void test_accepts_valid_agenda_state()
{
    AgendaState state{
        {storedEvent(1)},
        2
    };

    assert(
        validateAgendaState(state) ==
        AgendaStateError::None
    );
}

void test_rejects_more_than_500_events()
{
    AgendaState state;

    for (
        std::uint32_t id = 1;
        id <= 501;
        ++id
    )
    {
        state.events.push_back(storedEvent(id));
    }

    state.nextId = 502;

    assert(
        validateAgendaState(state) ==
        AgendaStateError::TooManyEvents
    );
}

void test_rejects_event_with_invalid_date()
{
    AgendaState state{
        {storedEvent(1)},
        2
    };

    state.events[0].eventDate =
        Date{2026, 2, 30};

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidEvent
    );
}

void test_rejects_event_with_invalid_time()
{
    AgendaState state{
        {storedEvent(1)},
        2
    };

    state.events[0].eventTime =
        std::optional<TimeOfDay>{
            TimeOfDay{24, 0}
        };

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidEvent
    );
}

void test_rejects_event_with_empty_text()
{
    AgendaState state{
        {storedEvent(1)},
        2
    };

    state.events[0].text = "";

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidEvent
    );
}

void test_rejects_event_with_invalid_utf8_text()
{
    AgendaState state{
        {storedEvent(1)},
        2
    };

    state.events[0].text =
        std::string("\x80", 1);

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidEvent
    );
}

void test_applies_500_character_text_limit()
{
    AgendaState accepted{
        {storedEvent(1)},
        2
    };

    accepted.events[0].text =
        repeatedGreekLetter(500);

    assert(
        validateAgendaState(accepted) ==
        AgendaStateError::None
    );

    AgendaState rejected{
        {storedEvent(1)},
        2
    };

    rejected.events[0].text =
        repeatedGreekLetter(501);

    assert(
        validateAgendaState(rejected) ==
        AgendaStateError::InvalidEvent
    );
}

void test_rejects_event_with_zero_id()
{
    AgendaState state{
        {storedEvent(0)},
        1
    };

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidEvent
    );
}

void test_rejects_duplicate_event_ids()
{
    AgendaState state{
        {
            storedEvent(7),
            storedEvent(7)
        },
        8
    };

    state.events[1].text = "Second event";

    assert(
        validateAgendaState(state) ==
        AgendaStateError::DuplicateId
    );
}

void test_rejects_zero_next_id()
{
    AgendaState state{
        {},
        0
    };

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidNextId
    );
}

void test_rejects_next_id_equal_to_existing_id()
{
    AgendaState state{
        {storedEvent(7)},
        7
    };

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidNextId
    );
}

void test_rejects_invalid_creation_date()
{
    AgendaState state{
        {storedEvent(1)},
        2
    };

    state.events[0].createdAt =
        DateTime{
            Date{2026, 2, 30},
            TimeOfDay{10, 0}
        };

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidEvent
    );
}

void test_rejects_invalid_creation_time()
{
    AgendaState state{
        {storedEvent(1)},
        2
    };

    state.events[0].createdAt =
        DateTime{
            Date{2026, 9, 4},
            TimeOfDay{24, 0}
        };

    assert(
        validateAgendaState(state) ==
        AgendaStateError::InvalidEvent
    );
}

int main()
{
    test_accepts_valid_agenda_state();
    test_rejects_more_than_500_events();
    test_rejects_event_with_invalid_date();
    test_rejects_event_with_invalid_time();
    test_rejects_event_with_empty_text();
    test_rejects_event_with_invalid_utf8_text();
    test_applies_500_character_text_limit();
    test_rejects_event_with_zero_id();
    test_rejects_duplicate_event_ids();
    test_rejects_zero_next_id();
    test_rejects_next_id_equal_to_existing_id();
    test_rejects_invalid_creation_date();
    test_rejects_invalid_creation_time();

    std::cout
        << "All agenda state tests passed.\n";

    return 0;
}