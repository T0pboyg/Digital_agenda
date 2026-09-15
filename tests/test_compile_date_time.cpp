#include <cassert>
#include <iostream>

#include "core/compile_date_time.h"

namespace
{
    void assertDateTime(
        const DateTime &actual,
        int year,
        int month,
        int day,
        int hour,
        int minute)
    {
        assert(actual.date.year == year);
        assert(actual.date.month == month);
        assert(actual.date.day == day);
        assert(actual.time.hour == hour);
        assert(actual.time.minute == minute);
    }

    void test_parses_normal_compiler_date_and_time()
    {
        const DateTime value =
            compileDateTimeOrFallback(
                "Sep 15 2026",
                "14:37:52");

        assertDateTime(value, 2026, 9, 15, 14, 37);
    }

    void test_parses_space_padded_single_digit_day()
    {
        const DateTime value =
            compileDateTimeOrFallback(
                "Jan  7 2026",
                "08:05:09");

        assertDateTime(value, 2026, 1, 7, 8, 5);
    }

    void test_invalid_compiler_text_returns_safe_fallback()
    {
        const DateTime value =
            compileDateTimeOrFallback(
                "Bad input",
                "99:88:77");

        assertDateTime(value, 2026, 1, 1, 12, 0);
    }
}

int main()
{
    test_parses_normal_compiler_date_and_time();
    test_parses_space_padded_single_digit_day();
    test_invalid_compiler_text_returns_safe_fallback();

    std::cout
        << "All compile date/time tests passed.\n";

    return 0;
}