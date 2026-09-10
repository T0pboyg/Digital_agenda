#pragma once

#include <cstdint>

namespace Esp32Board
{
constexpr std::int8_t LcdBacklight = 2;
constexpr std::int8_t LcdDataCommand = 8;
constexpr std::int8_t LcdChipSelect = 9;
constexpr std::int8_t LcdClock = 10;
constexpr std::int8_t LcdMosi = 11;
constexpr std::int8_t LcdMiso = 12;
constexpr std::int8_t LcdReset = 14;

constexpr std::int8_t TouchSda = 6;
constexpr std::int8_t TouchScl = 7;
constexpr std::int8_t TouchReset = 13;

constexpr std::int16_t DisplayWidth = 240;
constexpr std::int16_t DisplayHeight = 240;
constexpr std::uint8_t DisplayRotation = 0;
}
