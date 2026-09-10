#include "touch_sample.h"

TouchSample decodeCst816sSample(
    const std::array<std::uint8_t, 5>& registers
)
{
    if (registers[0] == 0)
    {
        return TouchSample{
            false,
            0,
            0
        };
    }

    const auto x = static_cast<std::uint16_t>(
        (
            static_cast<std::uint16_t>(
                registers[1] & 0x0F
            ) << 8U
        ) |
        registers[2]
    );

    const auto y = static_cast<std::uint16_t>(
        (
            static_cast<std::uint16_t>(
                registers[3] & 0x0F
            ) << 8U
        ) |
        registers[4]
    );

    return TouchSample{
        true,
        x,
        y
    };
}
