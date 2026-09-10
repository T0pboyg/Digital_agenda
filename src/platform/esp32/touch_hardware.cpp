#include <Arduino.h>
#include <Wire.h>

#include <array>
#include <cstdint>

#include "board_pins.h"
#include "cst816s_protocol.h"
#include "touch_hardware.h"

namespace
{
constexpr std::uint8_t TouchAddress =
    0x15;

constexpr std::uint8_t FingerNumberRegister =
    0x02;

constexpr std::uint8_t RegisterCount =
    5;

constexpr std::uint32_t I2cClock =
    400000;

bool writeRegister(
    const Cst816sRegisterWrite& command
)
{
    Wire.beginTransmission(
        TouchAddress
    );

    Wire.write(
        command.registerAddress
    );

    Wire.write(
        command.value
    );

    return Wire.endTransmission() == 0;
}
}

bool TouchHardware::begin()
{
    pinMode(
        Esp32Board::TouchReset,
        OUTPUT
    );

    digitalWrite(
        Esp32Board::TouchReset,
        LOW
    );
    delay(10);

    digitalWrite(
        Esp32Board::TouchReset,
        HIGH
    );
    delay(50);

    if (!Wire.begin(
            Esp32Board::TouchSda,
            Esp32Board::TouchScl
        ))
    {
        return false;
    }

    Wire.setClock(
        I2cClock
    );

    Wire.beginTransmission(
        TouchAddress
    );

    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    constexpr auto disableAutoSleep =
        cst816sDisableAutoSleepCommand();

    return writeRegister(
        disableAutoSleep
    );
}

bool TouchHardware::read(
    TouchSample& sample
)
{
    sample = {
        false,
        0,
        0
    };

    Wire.beginTransmission(
        TouchAddress
    );

    Wire.write(
        FingerNumberRegister
    );

    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    const auto bytesReceived =
        Wire.requestFrom(
            TouchAddress,
            RegisterCount
        );

    if (bytesReceived != RegisterCount)
    {
        while (Wire.available() > 0)
        {
            Wire.read();
        }

        return false;
    }

    std::array<std::uint8_t, RegisterCount>
        registers{};

    for (auto& value : registers)
    {
        value = static_cast<std::uint8_t>(
            Wire.read()
        );
    }

    sample = decodeCst816sSample(
        registers
    );

    return true;
}
