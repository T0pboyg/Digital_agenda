#include "utf8.h"

std::size_t utf8CodePointCount(std::string_view text)
{
    std::size_t count = 0;

    for (char character : text)
    {
        unsigned char byte =
            static_cast<unsigned char>(character);

        if ((byte & 0xC0) != 0x80)
        {
            ++count;
        }
    }

    return count;
}

std::string utf8LastCodePoint(std::string_view text)
{
    if (text.empty())
    {
        return "";
    }

    std::size_t start = text.size() - 1;

    while (start > 0)
    {
        unsigned char byte =
            static_cast<unsigned char>(text[start]);

        if ((byte & 0xC0) != 0x80)
        {
            break;
        }

        --start;
    }

    return std::string(text.substr(start));
}

bool utf8PopBack(std::string& text)
{
    if (text.empty())
    {
        return false;
    }

    std::size_t start = text.size() - 1;

    while (start > 0)
    {
        unsigned char byte =
            static_cast<unsigned char>(text[start]);

        if ((byte & 0xC0) != 0x80)
        {
            break;
        }

        --start;
    }

    text.erase(start);

    return true;
}