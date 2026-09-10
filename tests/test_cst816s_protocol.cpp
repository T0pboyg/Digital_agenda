#include <cassert>
#include <iostream>

#include "cst816s_protocol.h"

int main()
{
    constexpr auto command =
        cst816sDisableAutoSleepCommand();

    static_assert(
        command.registerAddress == 0xFE
    );

    static_assert(
        command.value == 0x01
    );

    assert(command.registerAddress == 0xFE);
    assert(command.value == 0x01);

    std::cout
        << "All CST816S protocol tests passed.\n";

    return 0;
}
