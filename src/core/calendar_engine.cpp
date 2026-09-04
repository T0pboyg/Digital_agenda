#include "calendar_engine.h"

int weekdayMondayFirst(const Date& date)
{
    // Η 01/01/2000 ήταν Σάββατο.
    // Με Δευτέρα = 0, το Σάββατο αντιστοιχεί στο 5.
    return (serialDay(date) + 5) % 7;
}

std::optional<YearMonth> previousMonth(const YearMonth& value)
{
    if (value.year == 2000 && value.month == 1)
    {
        return std::nullopt;
    }

    if (value.month == 1)
    {
        return YearMonth{value.year - 1, 12};
    }

    return YearMonth{value.year, value.month - 1};
}

std::optional<YearMonth> nextMonth(const YearMonth& value)
{
    if (value.year == 2099 && value.month == 12)
    {
        return std::nullopt;
    }

    if (value.month == 12)
    {
        return YearMonth{value.year + 1, 1};
    }

    return YearMonth{value.year, value.month + 1};
}

MonthGrid buildMonthGrid(const YearMonth& displayed)
{
    MonthGrid grid{};

    Date firstDay{
        displayed.year,
        displayed.month,
        1
    };

    int firstWeekday = weekdayMondayFirst(firstDay);
    int currentMonthDays =
        daysInMonth(displayed.year, displayed.month);

    std::optional<YearMonth> previous =
        previousMonth(displayed);

    std::optional<YearMonth> next =
        nextMonth(displayed);

    for (int index = 0; index < 42; index++)
    {
        int dayNumber = index - firstWeekday + 1;

        if (dayNumber < 1)
        {
            if (previous.has_value())
            {
                int previousMonthDays = daysInMonth(
                    previous->year,
                    previous->month
                );

                grid[index].date = Date{
                    previous->year,
                    previous->month,
                    previousMonthDays + dayNumber
                };
            }
            else
            {
                grid[index].date = std::nullopt;
            }

            grid[index].inDisplayedMonth = false;
        }
        else if (dayNumber > currentMonthDays)
        {
            if (next.has_value())
            {
                grid[index].date = Date{
                    next->year,
                    next->month,
                    dayNumber - currentMonthDays
                };
            }
            else
            {
                grid[index].date = std::nullopt;
            }

            grid[index].inDisplayedMonth = false;
        }
        else
        {
            grid[index].date = Date{
                displayed.year,
                displayed.month,
                dayNumber
            };

            grid[index].inDisplayedMonth = true;
        }
    }

    return grid;
}