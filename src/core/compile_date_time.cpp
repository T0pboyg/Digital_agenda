#include "compile_date_time.h"

#include <array>

namespace
{
    constexpr DateTime SafeFallback{
        Date{2026, 1, 1},
        TimeOfDay{12, 0}};

    int digitValue(char value)
    {
        if (value < '0' || value > '9')
        {
            return -1;
        }

        return value - '0';
    }

    int parseTwoDigits(
        std::string_view text,
        std::size_t offset)
    {
        const int first = digitValue(text[offset]);
        const int second = digitValue(text[offset + 1]);

        if (first < 0 || second < 0)
        {
            return -1;
        }

        return first * 10 + second;
    }

    int parseFourDigits(
        std::string_view text,
        std::size_t offset)
    {
        int value = 0;

        for (std::size_t index = 0; index < 4; ++index)
        {
            const int digit = digitValue(text[offset + index]);

            if (digit < 0)
            {
                return -1;
            }

            value = value * 10 + digit;
        }

        return value;
    }

    int parseMonth(std::string_view abbreviation)
    {
        constexpr std::array<std::string_view, 12> Months{
            "Jan", "Feb", "Mar", "Apr",
            "May", "Jun", "Jul", "Aug",
            "Sep", "Oct", "Nov", "Dec"};

        for (std::size_t index = 0; index < Months.size(); ++index)
        {
            if (Months[index] == abbreviation)
            {
                return static_cast<int>(index) + 1;
            }
        }

        return 0;
    }
}

DateTime compileDateTimeOrFallback(
    std::string_view dateText,
    std::string_view timeText)
{
    if (
        dateText.size() != 11 ||
        timeText.size() != 8 ||
        dateText[3] != ' ' ||
        dateText[6] != ' ' ||
        timeText[2] != ':' ||
        timeText[5] != ':')
    {
        return SafeFallback;
    }

    const int month = parseMonth(dateText.substr(0, 3));

    const int day =
        dateText[4] == ' '
            ? digitValue(dateText[5])
            : parseTwoDigits(dateText, 4);

    const int year = parseFourDigits(dateText, 7);
    const int hour = parseTwoDigits(timeText, 0);
    const int minute = parseTwoDigits(timeText, 3);
    const int second = parseTwoDigits(timeText, 6);

    const DateTime value{
        Date{year, month, day},
        TimeOfDay{hour, minute}};

    if (
        !isValidDate(value.date) ||
        !isValidTime(value.time) ||
        second < 0 ||
        second > 59)
    {
        return SafeFallback;
    }

    return value;
}