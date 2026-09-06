#pragma once

#include <cstdint>
#include <vector>

#include "agenda_state.h"

enum class EventCodecError
{
    None,
    InvalidMagic,
    UnsupportedVersion,
    ChecksumMismatch,
    TruncatedData,
    TrailingData,
    InvalidData
};

struct EventEncodeResult
{
    EventCodecError error;
    std::vector<std::uint8_t> bytes;
};

struct EventDecodeResult
{
    EventCodecError error;
    AgendaState state;
};

EventEncodeResult encodeAgendaState(
    const AgendaState& state
);

EventDecodeResult decodeAgendaState(
    const std::vector<std::uint8_t>& bytes
);