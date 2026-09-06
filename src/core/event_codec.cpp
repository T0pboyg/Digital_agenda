#include "event_codec.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "crc32.h"

namespace
{
void appendU8(
    std::vector<std::uint8_t>& bytes,
    std::uint8_t value
)
{
    bytes.push_back(value);
}

void appendU16(
    std::vector<std::uint8_t>& bytes,
    std::uint16_t value
)
{
    appendU8(
        bytes,
        static_cast<std::uint8_t>(value)
    );

    appendU8(
        bytes,
        static_cast<std::uint8_t>(value >> 8)
    );
}

void appendU32(
    std::vector<std::uint8_t>& bytes,
    std::uint32_t value
)
{
    appendU8(
        bytes,
        static_cast<std::uint8_t>(value)
    );

    appendU8(
        bytes,
        static_cast<std::uint8_t>(value >> 8)
    );

    appendU8(
        bytes,
        static_cast<std::uint8_t>(value >> 16)
    );

    appendU8(
        bytes,
        static_cast<std::uint8_t>(value >> 24)
    );
}

void appendI32(
    std::vector<std::uint8_t>& bytes,
    std::int32_t value
)
{
    appendU32(
        bytes,
        static_cast<std::uint32_t>(value)
    );
}

std::uint32_t crc32OfBytes(
    const std::vector<std::uint8_t>& bytes
)
{
    const std::string_view view(
        reinterpret_cast<const char*>(
            bytes.data()
        ),
        bytes.size()
    );

    return crc32(view);
}

class Reader
{
public:
    Reader(
        const std::vector<std::uint8_t>& bytes,
        std::size_t limit
    )
        : bytes_(bytes),
          limit_(limit)
    {
    }

    bool skip(std::size_t count)
    {
        if (count > limit_ - position_)
        {
            return false;
        }

        position_ += count;

        return true;
    }

    bool readU8(std::uint8_t& value)
    {
        if (position_ >= limit_)
        {
            return false;
        }

        value = bytes_[position_];
        ++position_;

        return true;
    }

    bool readU16(std::uint16_t& value)
    {
        std::uint8_t low = 0;
        std::uint8_t high = 0;

        if (
            !readU8(low) ||
            !readU8(high)
        )
        {
            return false;
        }

        value =
            static_cast<std::uint16_t>(low) |
            (
                static_cast<std::uint16_t>(high)
                << 8
            );

        return true;
    }

    bool readU32(std::uint32_t& value)
    {
        std::uint8_t bytes[4]{};

        for (std::uint8_t& byte : bytes)
        {
            if (!readU8(byte))
            {
                return false;
            }
        }

        value =
            static_cast<std::uint32_t>(bytes[0]) |
            (
                static_cast<std::uint32_t>(bytes[1])
                << 8
            ) |
            (
                static_cast<std::uint32_t>(bytes[2])
                << 16
            ) |
            (
                static_cast<std::uint32_t>(bytes[3])
                << 24
            );

        return true;
    }

    bool readI32(std::int32_t& value)
    {
        std::uint32_t raw = 0;

        if (!readU32(raw))
        {
            return false;
        }

        const std::int64_t signedValue =
            raw <= 0x7FFFFFFFU
                ? static_cast<std::int64_t>(raw)
                : static_cast<std::int64_t>(raw) -
                      0x100000000LL;

        value =
            static_cast<std::int32_t>(signedValue);

        return true;
    }

    bool readText(
        std::size_t size,
        std::string& text
    )
    {
        if (size > limit_ - position_)
        {
            return false;
        }

        text.assign(
            reinterpret_cast<const char*>(
                bytes_.data() + position_
            ),
            size
        );

        position_ += size;

        return true;
    }

private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t limit_;
    std::size_t position_ = 0;
};
}

EventEncodeResult encodeAgendaState(
    const AgendaState& state
)
{
    if (
        validateAgendaState(state) !=
        AgendaStateError::None
    )
    {
        return EventEncodeResult{
            EventCodecError::InvalidData,
            {}
        };
    }

    std::vector<std::uint8_t> bytes;

    bytes.insert(
        bytes.end(),
        {'D', 'D', 'A', 'G'}
    );

    appendU16(bytes, 1);

    appendU16(
        bytes,
        static_cast<std::uint16_t>(
            state.events.size()
        )
    );

    appendU32(bytes, state.nextId);

    for (const Event& event : state.events)
    {
        appendU32(bytes, event.id);
        appendI32(bytes, event.eventDate.year);
        appendU8(bytes, event.eventDate.month);
        appendU8(bytes, event.eventDate.day);

        appendU8(
            bytes,
            event.eventTime.has_value() ? 1 : 0
        );

        if (event.eventTime.has_value())
        {
            appendU8(
                bytes,
                event.eventTime->hour
            );

            appendU8(
                bytes,
                event.eventTime->minute
            );
        }

        appendU8(
            bytes,
            event.reminderSeen ? 1 : 0
        );

        appendI32(
            bytes,
            event.createdAt.date.year
        );

        appendU8(
            bytes,
            event.createdAt.date.month
        );

        appendU8(
            bytes,
            event.createdAt.date.day
        );

        appendU8(
            bytes,
            event.createdAt.time.hour
        );

        appendU8(
            bytes,
            event.createdAt.time.minute
        );

        appendU32(
            bytes,
            static_cast<std::uint32_t>(
                event.text.size()
            )
        );

        bytes.insert(
            bytes.end(),
            event.text.begin(),
            event.text.end()
        );
    }

    appendU32(
        bytes,
        crc32OfBytes(bytes)
    );

    return EventEncodeResult{
        EventCodecError::None,
        bytes
    };
}

EventDecodeResult decodeAgendaState(
    const std::vector<std::uint8_t>& bytes
)
{
    if (bytes.size() < 16)
    {
        return EventDecodeResult{
            EventCodecError::TruncatedData,
            {}
        };
    }

    Reader reader(
        bytes,
        bytes.size() - 4
    );

    if (!reader.skip(6))
    {
        return EventDecodeResult{
            EventCodecError::TruncatedData,
            {}
        };
    }

    std::uint16_t eventCount = 0;
    std::uint32_t nextId = 0;

    if (
        !reader.readU16(eventCount) ||
        !reader.readU32(nextId)
    )
    {
        return EventDecodeResult{
            EventCodecError::TruncatedData,
            {}
        };
    }

    AgendaState state;
    state.nextId = nextId;
    state.events.reserve(eventCount);

    for (
        std::uint16_t index = 0;
        index < eventCount;
        ++index
    )
    {
        std::uint32_t id = 0;
        std::int32_t eventYear = 0;
        std::uint8_t eventMonth = 0;
        std::uint8_t eventDay = 0;
        std::uint8_t hasTime = 0;

        if (
            !reader.readU32(id) ||
            !reader.readI32(eventYear) ||
            !reader.readU8(eventMonth) ||
            !reader.readU8(eventDay) ||
            !reader.readU8(hasTime)
        )
        {
            return EventDecodeResult{
                EventCodecError::TruncatedData,
                {}
            };
        }

        std::optional<TimeOfDay> eventTime;

        if (hasTime == 1)
        {
            std::uint8_t hour = 0;
            std::uint8_t minute = 0;

            if (
                !reader.readU8(hour) ||
                !reader.readU8(minute)
            )
            {
                return EventDecodeResult{
                    EventCodecError::TruncatedData,
                    {}
                };
            }

            eventTime =
                TimeOfDay{hour, minute};
        }

        std::uint8_t reminderSeen = 0;
        std::int32_t createdYear = 0;
        std::uint8_t createdMonth = 0;
        std::uint8_t createdDay = 0;
        std::uint8_t createdHour = 0;
        std::uint8_t createdMinute = 0;
        std::uint32_t textSize = 0;

        if (
            !reader.readU8(reminderSeen) ||
            !reader.readI32(createdYear) ||
            !reader.readU8(createdMonth) ||
            !reader.readU8(createdDay) ||
            !reader.readU8(createdHour) ||
            !reader.readU8(createdMinute) ||
            !reader.readU32(textSize)
        )
        {
            return EventDecodeResult{
                EventCodecError::TruncatedData,
                {}
            };
        }

        std::string text;

        if (!reader.readText(textSize, text))
        {
            return EventDecodeResult{
                EventCodecError::TruncatedData,
                {}
            };
        }

        state.events.push_back(
            Event{
                id,
                Date{
                    eventYear,
                    eventMonth,
                    eventDay
                },
                eventTime,
                text,
                reminderSeen == 1,
                DateTime{
                    Date{
                        createdYear,
                        createdMonth,
                        createdDay
                    },
                    TimeOfDay{
                        createdHour,
                        createdMinute
                    }
                }
            }
        );
    }

    return EventDecodeResult{
        EventCodecError::None,
        state
    };
}