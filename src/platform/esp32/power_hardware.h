#pragma once

#include <cstdint>

#include "ui/power_hardware.h"

enum class Esp32WakeSource
{
    Undefined,
    ExternalPin,
    Timer,
    Other
};

struct Esp32PowerApi
{
    using ReadWakeSourceFunction =
        Esp32WakeSource (*)();

    using ReleaseWakePinFunction =
        void (*)(std::int8_t pin);

    using ReadPinHighFunction =
        bool (*)(std::int8_t pin, bool& high);

    using DisableWakeSourcesFunction =
        bool (*)();

    using EnableExternalWakeFunction =
        bool (*)(std::int8_t pin, bool wakeOnLow);

    using EnableTimerWakeFunction =
        bool (*)(std::uint64_t microseconds);

    using EnterLightSleepFunction =
        bool (*)();

    using EnterDeepSleepFunction =
        void (*)();

    ReadWakeSourceFunction readWakeSource = nullptr;
    ReleaseWakePinFunction releaseWakePin = nullptr;
    ReadPinHighFunction readPinHigh = nullptr;
    DisableWakeSourcesFunction disableWakeSources = nullptr;
    EnableExternalWakeFunction enableExternalWake = nullptr;
    EnableTimerWakeFunction enableTimerWake = nullptr;
    EnterLightSleepFunction enterLightSleep = nullptr;
    EnterDeepSleepFunction enterDeepSleep = nullptr;
};

class Esp32PowerHardware final : public PowerHardware
{
public:
    Esp32PowerHardware();

    explicit Esp32PowerHardware(
        Esp32PowerApi api);

    PowerWakeCause wakeCause() const override;
    void releaseWakePin() override;

    PowerHardwareError prepareWake(
        const std::optional<std::uint64_t>&
            timerMicroseconds
    ) override;

    PowerHardwareError enterLightSleep() override;
    PowerHardwareError enterDeepSleep() override;

private:
    Esp32PowerApi api_;
    bool wakePrepared_ = false;
};
