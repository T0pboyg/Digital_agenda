#include "crc32.h"

std::uint32_t crc32(std::string_view data)
{
    std::uint32_t checksum = 0xFFFFFFFFU;

    for (char character : data)
    {
        checksum ^=
            static_cast<unsigned char>(character);

        for (int bit = 0; bit < 8; ++bit)
        {
            if ((checksum & 1U) != 0)
            {
                checksum =
                    (checksum >> 1U) ^
                    0xEDB88320U;
            }
            else
            {
                checksum >>= 1U;
            }
        }
    }

    return checksum ^ 0xFFFFFFFFU;
}