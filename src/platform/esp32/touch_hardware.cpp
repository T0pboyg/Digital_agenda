#include "touch_hardware.h"

#include <array>

#include "board_pins.h"
#include "cst816s_protocol.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#endif

namespace
{
    constexpr std::uint8_t TouchAddress = 0x15;
    constexpr std::uint8_t TouchDataRegister = 0x02;
    constexpr std::size_t TouchRegisterCount = 5;
    constexpr std::uint32_t I2cClock = 400000;

#ifdef ARDUINO
    void configureResetPin(std::int8_t pin)
    {
        pinMode(pin, OUTPUT);
    }

    void writeResetPin(
        std::int8_t pin,
        bool high)
    {
        digitalWrite(pin, high ? HIGH : LOW);
    }

    void delayMilliseconds(std::uint32_t milliseconds)
    {
        delay(milliseconds);
    }

    bool beginBus(
        std::int8_t sda,
        std::int8_t scl,
        std::uint32_t clock)
    {
        if (!Wire.begin(sda, scl))
        {
            return false;
        }

        Wire.setClock(clock);
        return true;
    }

    bool probe(std::uint8_t address)
    {
        Wire.beginTransmission(address);
        return Wire.endTransmission() == 0;
    }

    bool writeRegister(
        std::uint8_t address,
        std::uint8_t registerAddress,
        std::uint8_t value)
    {
        Wire.beginTransmission(address);
        Wire.write(registerAddress);
        Wire.write(value);
        return Wire.endTransmission() == 0;
    }

    bool readRegisters(
        std::uint8_t address,
        std::uint8_t firstRegister,
        std::uint8_t *bytes,
        std::size_t count)
    {
        Wire.beginTransmission(address);
        Wire.write(firstRegister);

        if (Wire.endTransmission() != 0)
        {
            return false;
        }

        const auto received =
            Wire.requestFrom(
                address,
                static_cast<std::uint8_t>(count));

        if (received != count)
        {
            while (Wire.available() > 0)
            {
                Wire.read();
            }

            return false;
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            bytes[index] =
                static_cast<std::uint8_t>(Wire.read());
        }

        return true;
    }
#else
    void configureResetPin(std::int8_t)
    {
    }

    void writeResetPin(std::int8_t, bool)
    {
    }

    void delayMilliseconds(std::uint32_t)
    {
    }

    bool beginBus(std::int8_t, std::int8_t, std::uint32_t)
    {
        return false;
    }

    bool probe(std::uint8_t)
    {
        return false;
    }

    bool writeRegister(std::uint8_t, std::uint8_t, std::uint8_t)
    {
        return false;
    }

    bool readRegisters(
        std::uint8_t,
        std::uint8_t,
        std::uint8_t *,
        std::size_t)
    {
        return false;
    }
#endif

    Cst816sTouchApi defaultTouchApi()
    {
        return Cst816sTouchApi{
            configureResetPin,
            writeResetPin,
            delayMilliseconds,
            beginBus,
            probe,
            writeRegister,
            readRegisters};
    }
}

Cst816sTouchHardware::Cst816sTouchHardware()
    : Cst816sTouchHardware(defaultTouchApi())
{
}

Cst816sTouchHardware::Cst816sTouchHardware(
    Cst816sTouchApi api)
    : api_(api)
{
}

TouchInputError Cst816sTouchHardware::begin()
{
    initialized_ = false;

    if (
        api_.configureResetPin == nullptr ||
        api_.writeResetPin == nullptr ||
        api_.delayMilliseconds == nullptr ||
        api_.beginBus == nullptr ||
        api_.probe == nullptr ||
        api_.writeRegister == nullptr ||
        api_.readRegisters == nullptr)
    {
        return TouchInputError::Unavailable;
    }

    api_.configureResetPin(Esp32Board::TouchReset);
    api_.writeResetPin(Esp32Board::TouchReset, false);
    api_.delayMilliseconds(10);
    api_.writeResetPin(Esp32Board::TouchReset, true);
    api_.delayMilliseconds(50);

    if (
        !api_.beginBus(
            Esp32Board::TouchSda,
            Esp32Board::TouchScl,
            I2cClock) ||
        !api_.probe(TouchAddress))
    {
        return TouchInputError::Unavailable;
    }

    constexpr Cst816sRegisterWrite disableAutoSleep =
        cst816sDisableAutoSleepCommand();

    if (
        !api_.writeRegister(
            TouchAddress,
            disableAutoSleep.registerAddress,
            disableAutoSleep.value))
    {
        return TouchInputError::Unavailable;
    }

    initialized_ = true;
    return TouchInputError::None;
}

TouchInputReadResult Cst816sTouchHardware::read()
{
    if (!initialized_)
    {
        return TouchInputReadResult{
            TouchInputError::Unavailable,
            TouchSample{false, 0, 0}};
    }

    std::array<std::uint8_t, TouchRegisterCount>
        registers{};

    if (
        !api_.readRegisters(
            TouchAddress,
            TouchDataRegister,
            registers.data(),
            registers.size()))
    {
        return TouchInputReadResult{
            TouchInputError::ReadFailed,
            TouchSample{false, 0, 0}};
    }

    return TouchInputReadResult{
        TouchInputError::None,
        decodeCst816sSample(registers)};
}