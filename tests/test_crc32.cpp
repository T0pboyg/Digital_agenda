#include <cassert>
#include <iostream>
#include <string>

#include "core/crc32.h"

void test_empty_input_has_zero_checksum()
{
    assert(crc32("") == 0x00000000U);
}

void test_matches_standard_crc32_check_value()
{
    assert(
        crc32("123456789") ==
        0xCBF43926U
    );
}

void test_processes_embedded_zero_and_binary_bytes()
{
    const char bytes[] = {
        static_cast<char>(0x00),
        static_cast<char>(0x01),
        static_cast<char>(0x02),
        static_cast<char>(0x03),
        static_cast<char>(0xFF)
    };

    const std::string data(
        bytes,
        sizeof(bytes)
    );

    assert(
        crc32(data) ==
        0x7B35F858U
    );
}

int main()
{
    test_empty_input_has_zero_checksum();
    test_matches_standard_crc32_check_value();
    test_processes_embedded_zero_and_binary_bytes();

    std::cout << "All CRC32 tests passed.\n";

    return 0;
}