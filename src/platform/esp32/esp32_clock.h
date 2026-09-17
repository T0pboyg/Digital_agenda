#pragma once

#include <cstdint>
#include <ctime>

#include "core/clock.h"
#include "ui/power_hardware.h"

struct Esp32ClockApi
{
    using ReadLocalTimeFunction =
        bool (*)(std::tm &);

    using WriteLocalTimeFunction =
        bool (*)(const std::tm &);

    using ReadEpochSecondsFunction =
        bool (*)(std::int64_t&);

    using LocalDateTimeToEpochSecondsFunction =
        bool (*)(const DateTime&, std::int64_t&);

    ReadLocalTimeFunction readLocalTime =
        nullptr;

    WriteLocalTimeFunction writeLocalTime =
        nullptr;

    ReadEpochSecondsFunction readEpochSeconds =
        nullptr;

    LocalDateTimeToEpochSecondsFunction
        localDateTimeToEpochSeconds = nullptr;
};

class Esp32Clock final :
    public Clock,
    public WakeDelayClock
{
public:
    Esp32Clock();

    explicit Esp32Clock(
        Esp32ClockApi api);

    ClockReadResult read() const override;

    ClockError set(
        const DateTime &value) override;

    WakeDelayResult delayUntil(
        const DateTime& target
    ) const override;

private:
    Esp32ClockApi api_;
};
