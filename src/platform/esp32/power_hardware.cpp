#include "power_hardware.h"

#include "board_pins.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#endif

namespace
{
#ifdef ARDUINO
    Esp32WakeSource readWakeSource()
    {
        switch (::esp_sleep_get_wakeup_cause())
        {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return Esp32WakeSource::Undefined;

        case ESP_SLEEP_WAKEUP_EXT0:
            return Esp32WakeSource::ExternalPin;

        case ESP_SLEEP_WAKEUP_TIMER:
            return Esp32WakeSource::Timer;

        default:
            return Esp32WakeSource::Other;
        }
    }

    void releaseWakePin(std::int8_t pin)
    {
        ::rtc_gpio_deinit(
            static_cast<gpio_num_t>(pin));
    }

    bool readPinHigh(
        std::int8_t pin,
        bool& high
    )
    {
        ::pinMode(pin, INPUT_PULLUP);
        high = ::digitalRead(pin) == HIGH;
        return true;
    }

    bool disableWakeSources()
    {
        return ::esp_sleep_disable_wakeup_source(
                   ESP_SLEEP_WAKEUP_ALL) == ESP_OK;
    }

    bool enableExternalWake(
        std::int8_t pin,
        bool wakeOnLow
    )
    {
        return ::esp_sleep_enable_ext0_wakeup(
                   static_cast<gpio_num_t>(pin),
                   wakeOnLow ? 0 : 1) == ESP_OK;
    }

    bool enableTimerWake(
        std::uint64_t microseconds
    )
    {
        return ::esp_sleep_enable_timer_wakeup(
                   microseconds) == ESP_OK;
    }

    bool enterLightSleep()
    {
        return ::esp_light_sleep_start() == ESP_OK;
    }

    void enterDeepSleep()
    {
        ::esp_deep_sleep_start();
    }
#else
    Esp32WakeSource readWakeSource()
    {
        return Esp32WakeSource::Undefined;
    }

    void releaseWakePin(std::int8_t)
    {
    }

    bool readPinHigh(std::int8_t, bool&)
    {
        return false;
    }

    bool disableWakeSources()
    {
        return false;
    }

    bool enableExternalWake(std::int8_t, bool)
    {
        return false;
    }

    bool enableTimerWake(std::uint64_t)
    {
        return false;
    }

    bool enterLightSleep()
    {
        return false;
    }

    void enterDeepSleep()
    {
    }
#endif

    Esp32PowerApi defaultPowerApi()
    {
        return Esp32PowerApi{
            readWakeSource,
            releaseWakePin,
            readPinHigh,
            disableWakeSources,
            enableExternalWake,
            enableTimerWake,
            enterLightSleep,
            enterDeepSleep
        };
    }
}

Esp32PowerHardware::Esp32PowerHardware()
    : Esp32PowerHardware(defaultPowerApi())
{
}

Esp32PowerHardware::Esp32PowerHardware(
    Esp32PowerApi api
)
    : api_(api)
{
}

PowerWakeCause Esp32PowerHardware::wakeCause() const
{
    if (api_.readWakeSource == nullptr)
    {
        return PowerWakeCause::Unknown;
    }

    switch (api_.readWakeSource())
    {
    case Esp32WakeSource::Undefined:
        return PowerWakeCause::ColdBoot;

    case Esp32WakeSource::ExternalPin:
        return PowerWakeCause::Touch;

    case Esp32WakeSource::Timer:
        return PowerWakeCause::Timer;

    case Esp32WakeSource::Other:
        break;
    }

    return PowerWakeCause::Unknown;
}

void Esp32PowerHardware::releaseWakePin()
{
    if (api_.releaseWakePin != nullptr)
    {
        api_.releaseWakePin(
            Esp32Board::TouchInterrupt);
    }
}

PowerHardwareError Esp32PowerHardware::prepareWake(
    const std::optional<std::uint64_t>&
        timerMicroseconds
)
{
    wakePrepared_ = false;

    if (
        api_.readPinHigh == nullptr ||
        api_.disableWakeSources == nullptr ||
        api_.enableExternalWake == nullptr ||
        (
            timerMicroseconds.has_value() &&
            api_.enableTimerWake == nullptr
        )
    )
    {
        return PowerHardwareError::Unavailable;
    }

    bool touchReleased = false;

    if (
        !api_.readPinHigh(
            Esp32Board::TouchInterrupt,
            touchReleased)
    )
    {
        return PowerHardwareError::Unavailable;
    }

    if (!touchReleased)
    {
        return PowerHardwareError::TouchActive;
    }

    if (!api_.disableWakeSources())
    {
        return PowerHardwareError::Unavailable;
    }

    if (
        !api_.enableExternalWake(
            Esp32Board::TouchInterrupt,
            true)
    )
    {
        api_.disableWakeSources();

        return PowerHardwareError::
            TouchWakeConfigurationFailed;
    }

    if (
        timerMicroseconds.has_value() &&
        (
            *timerMicroseconds == 0 ||
            !api_.enableTimerWake(
                *timerMicroseconds)
        )
    )
    {
        api_.disableWakeSources();

        return PowerHardwareError::
            TimerWakeConfigurationFailed;
    }

    wakePrepared_ = true;
    return PowerHardwareError::None;
}

PowerHardwareError Esp32PowerHardware::enterLightSleep()
{
    if (
        !wakePrepared_ ||
        api_.enterLightSleep == nullptr
    )
    {
        return PowerHardwareError::Unavailable;
    }

    wakePrepared_ = false;

    return api_.enterLightSleep()
        ? PowerHardwareError::None
        : PowerHardwareError::LightSleepFailed;
}

PowerHardwareError Esp32PowerHardware::enterDeepSleep()
{
    if (
        !wakePrepared_ ||
        api_.enterDeepSleep == nullptr
    )
    {
        return PowerHardwareError::Unavailable;
    }

    wakePrepared_ = false;
    api_.enterDeepSleep();

    return PowerHardwareError::DeepSleepReturned;
}
