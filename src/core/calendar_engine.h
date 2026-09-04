#pragma once

#include <array>
#include <optional>

#include "date_time.h"

struct YearMonth
{
    int year;
    int month;
};

struct CalendarCell
{
    std::optional<Date> date;
    bool inDisplayedMonth;
};

using MonthGrid = std::array<CalendarCell, 42>;

int weekdayMondayFirst(const Date& date);

std::optional<YearMonth> previousMonth(const YearMonth& value);
std::optional<YearMonth> nextMonth(const YearMonth& value);

MonthGrid buildMonthGrid(const YearMonth& displayed);