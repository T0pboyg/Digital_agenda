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

bool isValidUtf8(std::string_view text)
{
    std::size_t index = 0;

    while (index < text.size())
    {
        const unsigned char first =
            static_cast<unsigned char>(text[index]);

        std::size_t length = 0;

        if (first <= 0x7F)
        {
            length = 1;
        }
        else if (first >= 0xC2 && first <= 0xDF)
        {
            length = 2;
        }
        else if (first >= 0xE0 && first <= 0xEF)
        {
            length = 3;
        }
        else if (first >= 0xF0 && first <= 0xF4)
        {
            length = 4;
        }
        else
        {
            return false;
        }

        if (index + length > text.size())
        {
            return false;
        }

        for (
            std::size_t offset = 1;
            offset < length;
            ++offset
        )
        {
            const unsigned char continuation =
                static_cast<unsigned char>(
                    text[index + offset]
                );

            if ((continuation & 0xC0) != 0x80)
            {
                return false;
            }
        }

        if (length == 3)
        {
            const unsigned char second =
                static_cast<unsigned char>(
                    text[index + 1]
                );

            if (
                (first == 0xE0 && second < 0xA0) ||
                (first == 0xED && second > 0x9F)
            )
            {
                return false;
            }
        }

        if (length == 4)
        {
            const unsigned char second =
                static_cast<unsigned char>(
                    text[index + 1]
                );

            if (
                (first == 0xF0 && second < 0x90) ||
                (first == 0xF4 && second > 0x8F)
            )
            {
                return false;
            }
        }

        index += length;
    }

    return true;
}