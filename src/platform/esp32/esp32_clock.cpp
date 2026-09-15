#include "esp32_clock.h"

#ifdef ARDUINO
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#endif

namespace
{
    const DateTime EmptyClockValue{
        Date{2000, 1, 1},
        TimeOfDay{0, 0}};

    ClockReadResult makeReadError(
        ClockError error)
    {
        return ClockReadResult{
            error,
            EmptyClockValue};
    }

#ifdef ARDUINO
    constexpr char GreeceTimeZone[] =
        "EET-2EEST,M3.5.0/3,M10.5.0/4";

    bool configureGreeceTimeZone()
    {
        static const bool configured =
            []()
        {
            if (
                ::setenv(
                    "TZ",
                    GreeceTimeZone,
                    1) != 0)
            {
                return false;
            }

            ::tzset();
            return true;
        }();

        return configured;
    }

    bool readSystemLocalTime(
        std::tm &value)
    {
        if (!configureGreeceTimeZone())
        {
            return false;
        }

        const time_t currentTime =
            ::time(nullptr);

        if (
            currentTime ==
            static_cast<time_t>(-1))
        {
            return false;
        }

        return ::localtime_r(
                   &currentTime,
                   &value) != nullptr;
    }

    bool writeSystemLocalTime(
        const std::tm &value)
    {
        if (!configureGreeceTimeZone())
        {
            return false;
        }

        std::tm localTime = value;

        const time_t seconds =
            ::mktime(&localTime);

        if (
            seconds ==
            static_cast<time_t>(-1))
        {
            return false;
        }

        timeval rawTime{};

        rawTime.tv_sec = seconds;
        rawTime.tv_usec = 0;

        return ::settimeofday(
                   &rawTime,
                   nullptr) == 0;
    }
#else
    bool readSystemLocalTime(
        std::tm &)
    {
        return false;
    }

    bool writeSystemLocalTime(
        const std::tm &)
    {
        return false;
    }
#endif
}

Esp32Clock::Esp32Clock()
    : api_{
          readSystemLocalTime,
          writeSystemLocalTime}
{
}

Esp32Clock::Esp32Clock(
    Esp32ClockApi api)
    : api_(api)
{
}

ClockReadResult Esp32Clock::read() const
{
    if (api_.readLocalTime == nullptr)
    {
        return makeReadError(
            ClockError::Unavailable);
    }

    std::tm localTime{};

    if (!api_.readLocalTime(localTime))
    {
        return makeReadError(
            ClockError::ReadFailed);
    }

    const DateTime value{
        Date{
            localTime.tm_year + 1900,
            localTime.tm_mon + 1,
            localTime.tm_mday},
        TimeOfDay{
            localTime.tm_hour,
            localTime.tm_min}};

    if (
        !isValidDate(value.date) ||
        !isValidTime(value.time))
    {
        return makeReadError(
            ClockError::Unavailable);
    }

    return ClockReadResult{
        ClockError::None,
        value};
}

ClockError Esp32Clock::set(
    const DateTime &value)
{
    if (
        !isValidDate(value.date) ||
        !isValidTime(value.time))
    {
        return ClockError::InvalidDateTime;
    }

    if (api_.writeLocalTime == nullptr)
    {
        return ClockError::Unavailable;
    }

    std::tm localTime{};

    localTime.tm_year =
        value.date.year - 1900;

    localTime.tm_mon =
        value.date.month - 1;

    localTime.tm_mday =
        value.date.day;

    localTime.tm_hour =
        value.time.hour;

    localTime.tm_min =
        value.time.minute;

    localTime.tm_sec = 0;
    localTime.tm_isdst = -1;

    if (!api_.writeLocalTime(localTime))
    {
        return ClockError::WriteFailed;
    }

    return ClockError::None;
}