#pragma once

#include <optional>

struct Date
{
    int year;
    int month;
    int day;
};

struct TimeOfDay
{
    int hour;
    int minute;
};

struct DateTime
{
    Date date;
    TimeOfDay time;
};

bool isLeapYear(int year);
int daysInMonth(int year, int month);

bool isValidDate(const Date& date);
bool isValidTime(const TimeOfDay& time);

std::optional<Date> previousDate(
    const Date& value
);

std::optional<Date> nextDate(
    const Date& value
);

int serialDay(const Date& date);
long long serialMinute(const DateTime& dateTime);