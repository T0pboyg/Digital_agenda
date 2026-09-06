#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/event_codec.h"

Event codecEvent()
{
    return Event{
        0x01020304U,
        Date{2026, 9, 5},
        std::optional<TimeOfDay>{
            TimeOfDay{12, 30}
        },
        "A",
        false,
        DateTime{
            Date{2026, 9, 4},
            TimeOfDay{10, 0}
        }
    };
}

void assertSameEvent(
    const Event& left,
    const Event& right
)
{
    assert(left.id == right.id);

    assert(
        serialDay(left.eventDate) ==
        serialDay(right.eventDate)
    );

    assert(
        left.eventTime.has_value() ==
        right.eventTime.has_value()
    );

    if (left.eventTime.has_value())
    {
        assert(
            left.eventTime->hour ==
            right.eventTime->hour
        );

        assert(
            left.eventTime->minute ==
            right.eventTime->minute
        );
    }

    assert(left.text == right.text);
    assert(left.reminderSeen == right.reminderSeen);

    assert(
        serialMinute(left.createdAt) ==
        serialMinute(right.createdAt)
    );
}

void test_exact_version_one_layout()
{
    const AgendaState state{
        {codecEvent()},
        0x01020305U
    };

    const auto encoded =
        encodeAgendaState(state);

    const std::vector<std::uint8_t> expected{
        68, 68, 65, 71,
        1, 0,
        1, 0,
        5, 3, 2, 1,
        4, 3, 2, 1,
        234, 7, 0, 0,
        9, 5,
        1, 12, 30,
        0,
        234, 7, 0, 0,
        9, 4, 10, 0,
        1, 0, 0, 0,
        65,
        193, 217, 245, 89
    };

    assert(
        encoded.error ==
        EventCodecError::None
    );

    assert(encoded.bytes == expected);
}

void test_populated_state_round_trips()
{
    Event allDay = codecEvent();

    allDay.id = 1;
    allDay.eventTime.reset();
    allDay.text = u8"Γενέθλια 🙂";
    allDay.reminderSeen = true;

    Event timed = codecEvent();

    timed.id = 7;
    timed.text = "Dentist";

    const AgendaState original{
        {allDay, timed},
        19
    };

    const auto encoded =
        encodeAgendaState(original);

    assert(
        encoded.error ==
        EventCodecError::None
    );

    const auto decoded =
        decodeAgendaState(encoded.bytes);

    assert(
        decoded.error ==
        EventCodecError::None
    );

    assert(decoded.state.nextId == 19);
    assert(decoded.state.events.size() == 2);

    assertSameEvent(
        decoded.state.events[0],
        allDay
    );

    assertSameEvent(
        decoded.state.events[1],
        timed
    );
}

void test_empty_state_round_trips()
{
    const AgendaState original{
        {},
        0x01020304U
    };

    const auto encoded =
        encodeAgendaState(original);

    const std::vector<std::uint8_t> expected{
        68, 68, 65, 71,
        1, 0,
        0, 0,
        4, 3, 2, 1,
        89, 89, 21, 225
    };

    assert(encoded.bytes == expected);

    const auto decoded =
        decodeAgendaState(encoded.bytes);

    assert(
        decoded.error ==
        EventCodecError::None
    );

    assert(decoded.state.events.empty());

    assert(
        decoded.state.nextId ==
        0x01020304U
    );
}

int main()
{
    test_exact_version_one_layout();
    test_populated_state_round_trips();
    test_empty_state_round_trips();

    std::cout
        << "All event codec tests passed.\n";

    return 0;
}