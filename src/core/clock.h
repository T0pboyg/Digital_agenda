#pragma once

#include "date_time.h"

enum class ClockError
{
    None,
    Unavailable,
    ReadFailed,
    WriteFailed,
    InvalidDateTime
};

struct ClockReadResult
{
    ClockError error;
    DateTime value;
};

class Clock
{
public:
    virtual ~Clock() = default;

    virtual ClockReadResult read() const = 0;

    virtual ClockError set(
        const DateTime& value
    ) = 0;
};