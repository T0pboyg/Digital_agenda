#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <string_view>

#include "core/event_codec.h"
#include "core/crc32.h"

void rewriteChecksum(
    std::vector<std::uint8_t>& bytes
)
{
    bytes.resize(bytes.size() - 4);

    const std::string_view data(
        reinterpret_cast<const char*>(
            bytes.data()
        ),
        bytes.size()
    );

    const std::uint32_t checksum =
        crc32(data);

    bytes.push_back(
        static_cast<std::uint8_t>(checksum)
    );

    bytes.push_back(
        static_cast<std::uint8_t>(
            checksum >> 8
        )
    );

    bytes.push_back(
        static_cast<std::uint8_t>(
            checksum >> 16
        )
    );

    bytes.push_back(
        static_cast<std::uint8_t>(
            checksum >> 24
        )
    );
}

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

void test_rejects_wrong_magic()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[0] = 'X';

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::InvalidMagic
    );
}

void test_rejects_unsupported_version()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[4] = 2;

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::UnsupportedVersion
    );
}

void test_rejects_checksum_mismatch()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[38] ^= 1U;

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::ChecksumMismatch
    );
}

void test_rejects_trailing_data()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes.insert(
        bytes.end() - 4,
        0xAA
    );

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::TrailingData
    );
}

void test_rejects_missing_checksum_byte()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes.pop_back();

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::ChecksumMismatch
    );
}

void test_rejects_truncated_payload()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes.erase(
        bytes.begin() + 38
    );

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::TruncatedData
    );
}

void test_rejects_invalid_event_date()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[20] = 13;

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_invalid_has_time_value()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[22] = 2;

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_invalid_reminder_seen_value()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[25] = 2;

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_oversized_text_length()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[34] = 0xD1;
    bytes[35] = 0x07;
    bytes[36] = 0;
    bytes[37] = 0;

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_more_than_500_events()
{
    auto bytes =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    bytes[6] = 0xF5;
    bytes[7] = 0x01;

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_invalid_decoded_ids()
{
    std::vector<std::uint8_t> zeroId =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    zeroId[12] = 0;
    zeroId[13] = 0;
    zeroId[14] = 0;
    zeroId[15] = 0;

    rewriteChecksum(zeroId);

    assert(
        decodeAgendaState(zeroId).error ==
        EventCodecError::InvalidData
    );

    std::vector<std::uint8_t> invalidNextId =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    invalidNextId[8] = 0x04;
    invalidNextId[9] = 0x03;
    invalidNextId[10] = 0x02;
    invalidNextId[11] = 0x01;

    rewriteChecksum(invalidNextId);

    assert(
        decodeAgendaState(invalidNextId).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_invalid_decoded_text()
{
    std::vector<std::uint8_t> invalidUtf8 =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    invalidUtf8[38] = 0xFF;

    rewriteChecksum(invalidUtf8);

    assert(
        decodeAgendaState(invalidUtf8).error ==
        EventCodecError::InvalidData
    );

    std::vector<std::uint8_t> emptyText =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    emptyText.erase(emptyText.begin() + 38);

    emptyText[34] = 0;
    emptyText[35] = 0;
    emptyText[36] = 0;
    emptyText[37] = 0;

    rewriteChecksum(emptyText);

    assert(
        decodeAgendaState(emptyText).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_invalid_decoded_date_and_time()
{
    std::vector<std::uint8_t> invalidEventTime =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    invalidEventTime[23] = 24;

    rewriteChecksum(invalidEventTime);

    assert(
        decodeAgendaState(invalidEventTime).error ==
        EventCodecError::InvalidData
    );

    std::vector<std::uint8_t> invalidCreationDate =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    invalidCreationDate[30] = 13;

    rewriteChecksum(invalidCreationDate);

    assert(
        decodeAgendaState(invalidCreationDate).error ==
        EventCodecError::InvalidData
    );

    std::vector<std::uint8_t> invalidCreationTime =
        encodeAgendaState(
            AgendaState{
                {codecEvent()},
                0x01020305U
            }
        ).bytes;

    invalidCreationTime[33] = 60;

    rewriteChecksum(invalidCreationTime);

    assert(
        decodeAgendaState(invalidCreationTime).error ==
        EventCodecError::InvalidData
    );
}

void test_rejects_duplicate_decoded_ids()
{
    Event first = codecEvent();
    first.id = 1;

    Event second = codecEvent();
    second.id = 2;
    second.text = "B";

    std::vector<std::uint8_t> bytes =
        encodeAgendaState(
            AgendaState{
                {first, second},
                3
            }
        ).bytes;

    const std::size_t secondIdOffset = 12 + 27;

    bytes[secondIdOffset] = 1;
    bytes[secondIdOffset + 1] = 0;
    bytes[secondIdOffset + 2] = 0;
    bytes[secondIdOffset + 3] = 0;

    rewriteChecksum(bytes);

    assert(
        decodeAgendaState(bytes).error ==
        EventCodecError::InvalidData
    );
}

void test_encode_rejects_invalid_next_id()
{
    Event event = codecEvent();

    const AgendaState invalid{
        {event},
        event.id
    };

    const auto result =
        encodeAgendaState(invalid);

    assert(
        result.error ==
        EventCodecError::InvalidData
    );

    assert(result.bytes.empty());
}

void test_encode_rejects_text_above_character_limit()
{
    Event event = codecEvent();
    event.text = std::string(501, 'x');

    const AgendaState invalid{
        {event},
        event.id + 1
    };

    const auto result =
        encodeAgendaState(invalid);

    assert(
        result.error ==
        EventCodecError::InvalidData
    );

    assert(result.bytes.empty());
}

void test_encode_rejects_more_than_500_events()
{
    std::vector<Event> events;

    for (
        std::size_t index = 0;
        index <= MAX_AGENDA_EVENTS;
        ++index
    )
    {
        Event event = codecEvent();

        event.id =
            static_cast<std::uint32_t>(
                index + 1
            );

        events.push_back(event);
    }

    const AgendaState invalid{
        events,
        static_cast<std::uint32_t>(
            MAX_AGENDA_EVENTS + 2
        )
    };

    const auto result =
        encodeAgendaState(invalid);

    assert(
        result.error ==
        EventCodecError::InvalidData
    );

    assert(result.bytes.empty());
}

int main()
{
    test_exact_version_one_layout();
    test_populated_state_round_trips();
    test_empty_state_round_trips();
    test_rejects_wrong_magic();
    test_rejects_unsupported_version();
    test_rejects_checksum_mismatch();
    test_rejects_trailing_data();
    test_rejects_missing_checksum_byte();
    test_rejects_truncated_payload();
    test_rejects_invalid_event_date();
    test_rejects_invalid_has_time_value();
    test_rejects_invalid_reminder_seen_value();
    test_rejects_oversized_text_length();
    test_rejects_more_than_500_events();
    test_rejects_invalid_decoded_ids();
    test_rejects_invalid_decoded_text();
    test_rejects_invalid_decoded_date_and_time();
    test_rejects_duplicate_decoded_ids();
    test_encode_rejects_invalid_next_id();
    test_encode_rejects_text_above_character_limit();
    test_encode_rejects_more_than_500_events();

    std::cout
        << "All event codec tests passed.\n";

    return 0;
}