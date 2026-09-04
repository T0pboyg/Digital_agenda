#pragma once

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

int serialDay(const Date& date);
long long serialMinute(const DateTime& dateTime);