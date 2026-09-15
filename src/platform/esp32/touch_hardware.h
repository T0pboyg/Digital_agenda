#pragma once

#include <cstddef>
#include <cstdint>

#include "ui/touch_input.h"

struct Cst816sTouchApi
{
    using ConfigureResetPinFunction =
        void (*)(std::int8_t pin);

    using WriteResetPinFunction =
        void (*)(std::int8_t pin, bool high);

    using DelayFunction =
        void (*)(std::uint32_t milliseconds);

    using BeginBusFunction =
        bool (*)(
            std::int8_t sda,
            std::int8_t scl,
            std::uint32_t clock);

    using ProbeFunction =
        bool (*)(std::uint8_t address);

    using WriteRegisterFunction =
        bool (*)(
            std::uint8_t address,
            std::uint8_t registerAddress,
            std::uint8_t value);

    using ReadRegistersFunction =
        bool (*)(
            std::uint8_t address,
            std::uint8_t firstRegister,
            std::uint8_t *bytes,
            std::size_t count);

    ConfigureResetPinFunction configureResetPin = nullptr;
    WriteResetPinFunction writeResetPin = nullptr;
    DelayFunction delayMilliseconds = nullptr;
    BeginBusFunction beginBus = nullptr;
    ProbeFunction probe = nullptr;
    WriteRegisterFunction writeRegister = nullptr;
    ReadRegistersFunction readRegisters = nullptr;
};

class Cst816sTouchHardware final : public TouchInput
{
public:
    Cst816sTouchHardware();

    explicit Cst816sTouchHardware(
        Cst816sTouchApi api);

    TouchInputError begin();

    TouchInputReadResult read() override;

private:
    Cst816sTouchApi api_;
    bool initialized_ = false;
};