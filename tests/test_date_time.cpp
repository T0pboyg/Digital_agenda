#include <cassert>
#include <iostream>

#include "core/date_time.h"

void test_accepts_valid_dates()
{
    assert(isValidDate({2026, 9, 3}));
    assert(isValidDate({2024, 2, 29}));
    assert(isValidDate({2000, 2, 29}));
    assert(isValidDate({2099, 12, 31}));
}

void test_rejects_invalid_dates()
{
    assert(!isValidDate({2025, 2, 29}));
    assert(!isValidDate({2026, 4, 31}));
    assert(!isValidDate({2026, 13, 1}));
    assert(!isValidDate({2026, 0, 10}));
    assert(!isValidDate({2026, 1, 0}));
    assert(!isValidDate({1999, 12, 31}));
    assert(!isValidDate({2100, 1, 1}));
}

void test_accepts_valid_times()
{
    assert(isValidTime({0, 0}));
    assert(isValidTime({9, 30}));
    assert(isValidTime({23, 59}));
}

void test_rejects_invalid_times()
{
    assert(!isValidTime({24, 0}));
    assert(!isValidTime({12, 60}));
    assert(!isValidTime({-1, 30}));
    assert(!isValidTime({10, -1}));
}

void test_serial_day_counts_calendar_days()
{
    assert(
        serialDay({2026, 9, 4}) -
        serialDay({2026, 9, 3}) == 1
    );

    assert(
        serialDay({2027, 1, 1}) -
        serialDay({2026, 12, 31}) == 1
    );

    // Το 2024 είναι δίσεκτο.
    assert(
        serialDay({2024, 3, 1}) -
        serialDay({2024, 2, 28}) == 2
    );

    // Το 2025 δεν είναι δίσεκτο.
    assert(
        serialDay({2025, 3, 1}) -
        serialDay({2025, 2, 28}) == 1
    );
}

void test_serial_minute_counts_minutes()
{
    DateTime start{
        {2026, 9, 3},
        {12, 0}
    };

    DateTime forty_five_minutes_later{
        {2026, 9, 3},
        {12, 45}
    };

    DateTime one_day_later{
        {2026, 9, 4},
        {12, 0}
    };

    assert(
        serialMinute(forty_five_minutes_later) -
        serialMinute(start) == 45
    );

    assert(
        serialMinute(one_day_later) -
        serialMinute(start) == 1440
    );
}

void test_leap_year_and_days_in_month()
{
    assert(isLeapYear(2000));
    assert(!isLeapYear(1900));
    assert(isLeapYear(2024));
    assert(!isLeapYear(2025));

    assert(daysInMonth(2024, 2) == 29);
    assert(daysInMonth(2025, 2) == 28);
    assert(daysInMonth(2026, 4) == 30);
    assert(daysInMonth(2026, 1) == 31);
    assert(daysInMonth(2026, 13) == 0);
}

int main()
{
    test_accepts_valid_dates();
    test_rejects_invalid_dates();

    test_accepts_valid_times();
    test_rejects_invalid_times();

    test_serial_day_counts_calendar_days();
    test_serial_minute_counts_minutes();

    test_leap_year_and_days_in_month();

    std::cout << "All date/time tests passed.\n";

    return 0;
}