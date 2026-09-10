#include "touch_sample.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    const std::array<std::uint8_t, 5> noTouch{
        0x00,
        0xA1,
        0x23,
        0xB2,
        0x34
    };

    const TouchSample inactive =
        decodeCst816sSample(noTouch);

    assert(!inactive.touched);
    assert(inactive.x == 0);
    assert(inactive.y == 0);

    const std::array<std::uint8_t, 5> oneTouch{
        0x01,
        0xA1,
        0x23,
        0xB2,
        0x34
    };

    const TouchSample active =
        decodeCst816sSample(oneTouch);

    assert(active.touched);
    assert(active.x == 0x0123);
    assert(active.y == 0x0234);

    std::cout
        << "All touch sample tests passed.\n";

    return 0;
}
