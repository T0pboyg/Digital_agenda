#pragma once

#include <cstdint>
#include <optional>

#include "core/clock.h"

enum class PowerWakeCause
{
    ColdBoot,
    Touch,
    Timer,
    Unknown
};

enum class PowerHardwareError
{
    None,
    Unavailable,
    TouchActive,
    TouchWakeConfigurationFailed,
    TimerWakeConfigurationFailed,
    LightSleepFailed,
    DeepSleepReturned
};

class PowerHardware
{
public:
    virtual ~PowerHardware() = default;

    virtual PowerWakeCause wakeCause() const = 0;
    virtual void releaseWakePin() = 0;

    virtual PowerHardwareError prepareWake(
        const std::optional<std::uint64_t>&
            timerMicroseconds
    ) = 0;

    virtual PowerHardwareError enterLightSleep() = 0;
    virtual PowerHardwareError enterDeepSleep() = 0;
};

class BacklightControl
{
public:
    virtual ~BacklightControl() = default;

    virtual void enableBacklight() = 0;
    virtual void disableBacklight() = 0;
};

struct WakeDelayResult
{
    ClockError error;
    bool dueNow;
    std::uint64_t microseconds;
};

class WakeDelayClock
{
public:
    virtual ~WakeDelayClock() = default;

    virtual WakeDelayResult delayUntil(
        const DateTime& target
    ) const = 0;
};
