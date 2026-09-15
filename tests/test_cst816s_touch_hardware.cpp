#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "platform/esp32/board_pins.h"
#include "platform/esp32/touch_hardware.h"

namespace
{
    std::int8_t configuredResetPin = -1;
    std::vector<bool> resetLevels;
    std::vector<std::uint32_t> delays;
    std::int8_t receivedSda = -1;
    std::int8_t receivedScl = -1;
    std::uint32_t receivedClock = 0;
    std::uint8_t probedAddress = 0;
    std::uint8_t writtenAddress = 0;
    std::uint8_t writtenRegister = 0;
    std::uint8_t writtenValue = 0;
    std::uint8_t readAddress = 0;
    std::uint8_t readFirstRegister = 0;
    std::size_t readCount = 0;
    bool readSucceeds = true;
    std::array<std::uint8_t, 5> registerBytes{};

    void fakeConfigureResetPin(std::int8_t pin)
    {
        configuredResetPin = pin;
    }

    void fakeWriteResetPin(
        std::int8_t,
        bool high)
    {
        resetLevels.push_back(high);
    }

    void fakeDelay(std::uint32_t milliseconds)
    {
        delays.push_back(milliseconds);
    }

    bool fakeBeginBus(
        std::int8_t sda,
        std::int8_t scl,
        std::uint32_t clock)
    {
        receivedSda = sda;
        receivedScl = scl;
        receivedClock = clock;
        return true;
    }

    bool fakeProbe(std::uint8_t address)
    {
        probedAddress = address;
        return true;
    }

    bool fakeWriteRegister(
        std::uint8_t address,
        std::uint8_t registerAddress,
        std::uint8_t value)
    {
        writtenAddress = address;
        writtenRegister = registerAddress;
        writtenValue = value;
        return true;
    }

    bool fakeReadRegisters(
        std::uint8_t address,
        std::uint8_t firstRegister,
        std::uint8_t *bytes,
        std::size_t count)
    {
        readAddress = address;
        readFirstRegister = firstRegister;
        readCount = count;

        if (!readSucceeds)
        {
            return false;
        }

        std::copy(
            registerBytes.begin(),
            registerBytes.end(),
            bytes);

        return true;
    }

    void resetFakes()
    {
        configuredResetPin = -1;
        resetLevels.clear();
        delays.clear();
        receivedSda = -1;
        receivedScl = -1;
        receivedClock = 0;
        probedAddress = 0;
        writtenAddress = 0;
        writtenRegister = 0;
        writtenValue = 0;
        readAddress = 0;
        readFirstRegister = 0;
        readCount = 0;
        readSucceeds = true;
        registerBytes = {};
    }

    Cst816sTouchHardware makeHardware()
    {
        return Cst816sTouchHardware{
            Cst816sTouchApi{
                fakeConfigureResetPin,
                fakeWriteResetPin,
                fakeDelay,
                fakeBeginBus,
                fakeProbe,
                fakeWriteRegister,
                fakeReadRegisters}};
    }

    void test_begin_resets_and_configures_the_verified_bus()
    {
        resetFakes();
        Cst816sTouchHardware hardware = makeHardware();

        assert(hardware.begin() == TouchInputError::None);
        assert(configuredResetPin == Esp32Board::TouchReset);
        assert((resetLevels == std::vector<bool>{false, true}));
        assert((delays == std::vector<std::uint32_t>{10, 50}));
        assert(receivedSda == Esp32Board::TouchSda);
        assert(receivedScl == Esp32Board::TouchScl);
        assert(receivedClock == 400000);
        assert(probedAddress == 0x15);
        assert(writtenAddress == 0x15);
        assert(writtenRegister == 0xFE);
        assert(writtenValue == 0x01);
        static_assert(Esp32Board::TouchInterrupt == 5);
    }

    void test_read_fetches_exactly_five_bytes_and_decodes_them()
    {
        resetFakes();
        registerBytes = {0x01, 0xA0, 0x7B, 0xB0, 0x95};

        Cst816sTouchHardware hardware = makeHardware();
        assert(hardware.begin() == TouchInputError::None);

        const TouchInputReadResult result = hardware.read();

        assert(result.error == TouchInputError::None);
        assert(result.sample.touched);
        assert(result.sample.x == 123);
        assert(result.sample.y == 149);
        assert(readAddress == 0x15);
        assert(readFirstRegister == 0x02);
        assert(readCount == 5);
    }

    void test_i2c_failure_is_not_reported_as_finger_release()
    {
        resetFakes();
        Cst816sTouchHardware hardware = makeHardware();
        assert(hardware.begin() == TouchInputError::None);

        readSucceeds = false;

        const TouchInputReadResult result = hardware.read();

        assert(result.error == TouchInputError::ReadFailed);
        assert(!result.sample.touched);
    }
}

int main()
{
    test_begin_resets_and_configures_the_verified_bus();
    test_read_fetches_exactly_five_bytes_and_decodes_them();
    test_i2c_failure_is_not_reported_as_finger_release();

    std::cout
        << "All CST816S touch hardware tests passed.\n";

    return 0;
}