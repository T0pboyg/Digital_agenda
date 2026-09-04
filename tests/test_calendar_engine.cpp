#include <cassert>
#include <iostream>
#include <optional>

#include "core/calendar_engine.h"

void expectDate(
    const std::optional<Date>& actual,
    int expectedYear,
    int expectedMonth,
    int expectedDay)
{
    assert(actual.has_value());
    assert(actual->year == expectedYear);
    assert(actual->month == expectedMonth);
    assert(actual->day == expectedDay);
}

void test_week_starts_on_monday()
{
    assert(weekdayMondayFirst({2026, 9, 7}) == 0); // Monday
    assert(weekdayMondayFirst({2026, 9, 1}) == 1); // Tuesday
    assert(weekdayMondayFirst({2000, 1, 1}) == 5); // Saturday
    assert(weekdayMondayFirst({2026, 9, 6}) == 6); // Sunday
}

void test_builds_month_with_neighbouring_dates()
{
    MonthGrid september = buildMonthGrid({2026, 9});

    assert(september.size() == 42);

    expectDate(september[0].date, 2026, 8, 31);
    assert(!september[0].inDisplayedMonth);

    expectDate(september[1].date, 2026, 9, 1);
    assert(september[1].inDisplayedMonth);

    expectDate(september[30].date, 2026, 9, 30);
    assert(september[30].inDisplayedMonth);

    expectDate(september[31].date, 2026, 10, 1);
    assert(!september[31].inDisplayedMonth);
}

void test_builds_six_week_month()
{
    MonthGrid august = buildMonthGrid({2026, 8});

    assert(august.size() == 42);

    expectDate(august[5].date, 2026, 8, 1);
    expectDate(august[35].date, 2026, 8, 31);
    expectDate(august[41].date, 2026, 9, 6);
}

void test_boundary_cells_outside_supported_range_are_empty()
{
    MonthGrid january = buildMonthGrid({2000, 1});

    assert(!january[0].date.has_value());
    assert(!january[4].date.has_value());

    expectDate(january[5].date, 2000, 1, 1);
    assert(january[5].inDisplayedMonth);

    MonthGrid december = buildMonthGrid({2099, 12});

    expectDate(december[31].date, 2099, 12, 31);
    assert(december[31].inDisplayedMonth);

    assert(!december[32].date.has_value());
    assert(!december[41].date.has_value());
}

void test_month_navigation_crosses_years()
{
    auto previous = previousMonth({2026, 1});

    assert(previous.has_value());
    assert(previous->year == 2025);
    assert(previous->month == 12);

    auto next = nextMonth({2026, 12});

    assert(next.has_value());
    assert(next->year == 2027);
    assert(next->month == 1);
}

void test_month_navigation_stops_at_supported_range()
{
    assert(!previousMonth({2000, 1}).has_value());
    assert(!nextMonth({2099, 12}).has_value());
}

int main()
{
    test_week_starts_on_monday();
    test_builds_month_with_neighbouring_dates();
    test_builds_six_week_month();
    test_boundary_cells_outside_supported_range_are_empty();
    test_month_navigation_crosses_years();
    test_month_navigation_stops_at_supported_range();

    std::cout << "All calendar engine tests passed.\n";

    return 0;
}