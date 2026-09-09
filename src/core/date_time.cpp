#include "date_time.h"

#include <optional>

bool isLeapYear(int year)
{
    return (year % 400 == 0) ||
           (year % 4 == 0 && year % 100 != 0);
}

int daysInMonth(int year, int month)
{
    if (month < 1 || month > 12)
    {
        return 0;
    }

    if (month == 2)
    {
        return isLeapYear(year) ? 29 : 28;
    }

    if (
        month == 4 ||
        month == 6 ||
        month == 9 ||
        month == 11
    )
    {
        return 30;
    }

    return 31;
}

bool isValidTime(const TimeOfDay& time)
{
    return time.hour >= 0 &&
           time.hour <= 23 &&
           time.minute >= 0 &&
           time.minute <= 59;
}

bool isValidDate(const Date& date)
{
    if (date.year < 2000 || date.year > 2099)
    {
        return false;
    }

    if (date.month < 1 || date.month > 12)
    {
        return false;
    }

    if (date.day < 1)
    {
        return false;
    }

    const int maximumDay =
        daysInMonth(date.year, date.month);

    return date.day <= maximumDay;
}

std::optional<Date> previousDate(
    const Date& value
)
{
    if (
        !isValidDate(value) ||
        (
            value.year == 2000 &&
            value.month == 1 &&
            value.day == 1
        )
    )
    {
        return std::nullopt;
    }

    Date result = value;

    if (result.day > 1)
    {
        --result.day;
        return result;
    }

    if (result.month > 1)
    {
        --result.month;
    }
    else
    {
        --result.year;
        result.month = 12;
    }

    result.day =
        daysInMonth(result.year, result.month);

    return result;
}

std::optional<Date> nextDate(
    const Date& value
)
{
    if (
        !isValidDate(value) ||
        (
            value.year == 2099 &&
            value.month == 12 &&
            value.day == 31
        )
    )
    {
        return std::nullopt;
    }

    Date result = value;

    const int lastDay =
        daysInMonth(result.year, result.month);

    if (result.day < lastDay)
    {
        ++result.day;
        return result;
    }

    result.day = 1;

    if (result.month < 12)
    {
        ++result.month;
    }
    else
    {
        ++result.year;
        result.month = 1;
    }

    return result;
}

int serialDay(const Date& date)
{
    int totalDays = 0;

    for (
        int year = 2000;
        year < date.year;
        ++year
    )
    {
        totalDays +=
            isLeapYear(year) ? 366 : 365;
    }

    for (
        int month = 1;
        month < date.month;
        ++month
    )
    {
        totalDays +=
            daysInMonth(date.year, month);
    }

    totalDays += date.day - 1;

    return totalDays;
}

long long serialMinute(
    const DateTime& dateTime
)
{
    return static_cast<long long>(
        serialDay(dateTime.date)
    ) * 1440LL
        + dateTime.time.hour * 60
        + dateTime.time.minute;
}