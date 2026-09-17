#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>

#include "platform/esp32/board_pins.h"
#include "platform/esp32/power_hardware.h"

namespace
{
    Esp32WakeSource wakeSource = Esp32WakeSource::Undefined;
    bool pinHigh = true;
    bool disableSucceeds = true;
    bool externalSucceeds = true;
    bool timerSucceeds = true;
    bool lightSucceeds = true;

    int releaseCount = 0;
    int disableCount = 0;
    int externalCount = 0;
    int timerCount = 0;
    int lightCount = 0;
    int deepCount = 0;
    std::int8_t lastPin = -1;
    bool lastWakeOnLow = false;
    std::uint64_t lastTimer = 0;

    Esp32WakeSource fakeWakeSource()
    {
        return wakeSource;
    }

    void fakeRelease(std::int8_t pin)
    {
        ++releaseCount;
        lastPin = pin;
    }

    bool fakeReadPin(std::int8_t pin, bool& high)
    {
        lastPin = pin;
        high = pinHigh;
        return true;
    }

    bool fakeDisable()
    {
        ++disableCount;
        return disableSucceeds;
    }

    bool fakeExternal(std::int8_t pin, bool wakeOnLow)
    {
        ++externalCount;
        lastPin = pin;
        lastWakeOnLow = wakeOnLow;
        return externalSucceeds;
    }

    bool fakeTimer(std::uint64_t microseconds)
    {
        ++timerCount;
        lastTimer = microseconds;
        return timerSucceeds;
    }

    bool fakeLight()
    {
        ++lightCount;
        return lightSucceeds;
    }

    void fakeDeep()
    {
        ++deepCount;
    }

    void resetFakes()
    {
        wakeSource = Esp32WakeSource::Undefined;
        pinHigh = true;
        disableSucceeds = true;
        externalSucceeds = true;
        timerSucceeds = true;
        lightSucceeds = true;
        releaseCount = 0;
        disableCount = 0;
        externalCount = 0;
        timerCount = 0;
        lightCount = 0;
        deepCount = 0;
        lastPin = -1;
        lastWakeOnLow = false;
        lastTimer = 0;
    }

    Esp32PowerHardware makeHardware()
    {
        return Esp32PowerHardware{
            Esp32PowerApi{
                fakeWakeSource,
                fakeRelease,
                fakeReadPin,
                fakeDisable,
                fakeExternal,
                fakeTimer,
                fakeLight,
                fakeDeep
            }
        };
    }

    void test_wake_causes_and_pin_release_are_mapped()
    {
        resetFakes();
        Esp32PowerHardware hardware = makeHardware();

        assert(hardware.wakeCause() == PowerWakeCause::ColdBoot);
        wakeSource = Esp32WakeSource::ExternalPin;
        assert(hardware.wakeCause() == PowerWakeCause::Touch);
        wakeSource = Esp32WakeSource::Timer;
        assert(hardware.wakeCause() == PowerWakeCause::Timer);
        wakeSource = Esp32WakeSource::Other;
        assert(hardware.wakeCause() == PowerWakeCause::Unknown);

        hardware.releaseWakePin();
        assert(releaseCount == 1);
        assert(lastPin == Esp32Board::TouchInterrupt);
    }

    void test_prepare_arms_active_low_touch_and_optional_timer()
    {
        resetFakes();
        Esp32PowerHardware hardware = makeHardware();

        assert(
            hardware.prepareWake(1234567ULL) ==
            PowerHardwareError::None);
        assert(disableCount == 1);
        assert(externalCount == 1);
        assert(timerCount == 1);
        assert(lastPin == Esp32Board::TouchInterrupt);
        assert(lastWakeOnLow);
        assert(lastTimer == 1234567ULL);

        assert(
            hardware.enterLightSleep() ==
            PowerHardwareError::None);
        assert(lightCount == 1);

        resetFakes();
        hardware = makeHardware();
        assert(
            hardware.prepareWake(std::nullopt) ==
            PowerHardwareError::None);
        assert(timerCount == 0);
        assert(
            hardware.enterDeepSleep() ==
            PowerHardwareError::DeepSleepReturned);
        assert(deepCount == 1);
    }

    void test_failed_or_active_wake_source_never_starts_sleep()
    {
        resetFakes();
        Esp32PowerHardware hardware = makeHardware();

        pinHigh = false;
        assert(
            hardware.prepareWake(std::nullopt) ==
            PowerHardwareError::TouchActive);
        assert(
            hardware.enterLightSleep() ==
            PowerHardwareError::Unavailable);
        assert(lightCount == 0);

        resetFakes();
        hardware = makeHardware();
        externalSucceeds = false;
        assert(
            hardware.prepareWake(std::nullopt) ==
            PowerHardwareError::TouchWakeConfigurationFailed);
        assert(disableCount == 2);
        assert(
            hardware.enterDeepSleep() ==
            PowerHardwareError::Unavailable);
        assert(deepCount == 0);

        resetFakes();
        hardware = makeHardware();
        timerSucceeds = false;
        assert(
            hardware.prepareWake(10ULL) ==
            PowerHardwareError::TimerWakeConfigurationFailed);
        assert(disableCount == 2);
        assert(lightCount == 0);
        assert(deepCount == 0);
    }
}

int main()
{
    test_wake_causes_and_pin_release_are_mapped();
    test_prepare_arms_active_low_touch_and_optional_timer();
    test_failed_or_active_wake_source_never_starts_sleep();

    std::cout << "All ESP32 power hardware tests passed.\n";
    return 0;
}
