#pragma once

#include <cstdint>

struct Cst816sRegisterWrite
{
    std::uint8_t registerAddress;
    std::uint8_t value;
};

constexpr Cst816sRegisterWrite
cst816sDisableAutoSleepCommand()
{
    return {
        0xFE,
        0x01
    };
}
