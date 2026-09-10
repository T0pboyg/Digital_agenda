#pragma once

#include <array>
#include <cstdint>

struct TouchSample
{
    bool touched;
    std::uint16_t x;
    std::uint16_t y;
};

TouchSample decodeCst816sSample(
    const std::array<std::uint8_t, 5>& registers
);
