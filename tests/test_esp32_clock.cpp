#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>

#include "platform/esp32/esp32_clock.h"

namespace
{
    bool readSucceeds = true;
    bool writeSucceeds = true;
    bool writeCalled = false;
    bool epochReadSucceeds = true;
    bool conversionSucceeds = true;

    std::int64_t currentEpochSeconds = 0;
    std::int64_t convertedEpochSeconds = 0;

    std::tm readValue{};
    std::tm writtenValue{};

    bool fakeReadLocalTime(
        std::tm &value)
    {
        if (!readSucceeds)
        {
            return false;
        }

        value = readValue;
        return true;
    }

    bool fakeWriteLocalTime(
        const std::tm &value)
    {
        writeCalled = true;
        writtenValue = value;

        return writeSucceeds;
    }

    bool fakeReadEpochSeconds(
        std::int64_t& value)
    {
        value = currentEpochSeconds;
        return epochReadSucceeds;
    }

    bool fakeConvertLocalDateTime(
        const DateTime&,
        std::int64_t& value)
    {
        value = convertedEpochSeconds;
        return conversionSucceeds;
    }

    void resetFakes()
    {
        readSucceeds = true;
        writeSucceeds = true;
        writeCalled = false;
        epochReadSucceeds = true;
        conversionSucceeds = true;
        currentEpochSeconds = 0;
        convertedEpochSeconds = 0;

        readValue = {};
        writtenValue = {};
    }

    Esp32Clock makeClock()
    {
        return Esp32Clock{
            Esp32ClockApi{
                fakeReadLocalTime,
                fakeWriteLocalTime,
                fakeReadEpochSeconds,
                fakeConvertLocalDateTime}};
    }

    void test_read_returns_current_local_date_time()
    {
        resetFakes();

        readValue.tm_year = 2026 - 1900;
        readValue.tm_mon = 9 - 1;
        readValue.tm_mday = 15;
        readValue.tm_hour = 14;
        readValue.tm_min = 30;

        const ClockReadResult result =
            makeClock().read();

        assert(result.error == ClockError::None);

        assert(result.value.date.year == 2026);
        assert(result.value.date.month == 9);
        assert(result.value.date.day == 15);

        assert(result.value.time.hour == 14);
        assert(result.value.time.minute == 30);
    }

    void test_read_reports_failure_and_unset_clock()
    {
        resetFakes();

        readSucceeds = false;

        const ClockReadResult failedResult =
            makeClock().read();

        assert(
            failedResult.error ==
            ClockError::ReadFailed);

        resetFakes();

        readValue.tm_year = 1970 - 1900;
        readValue.tm_mon = 1 - 1;
        readValue.tm_mday = 1;

        const ClockReadResult unsetResult =
            makeClock().read();

        assert(
            unsetResult.error ==
            ClockError::Unavailable);
    }

    void test_set_validates_and_writes_local_date_time()
    {
        resetFakes();

        Esp32Clock clock = makeClock();

        const DateTime valid{
            Date{2026, 3, 28},
            TimeOfDay{10, 45}};

        assert(clock.set(valid) == ClockError::None);
        assert(writeCalled);

        assert(writtenValue.tm_year == 2026 - 1900);
        assert(writtenValue.tm_mon == 3 - 1);
        assert(writtenValue.tm_mday == 28);
        assert(writtenValue.tm_hour == 10);
        assert(writtenValue.tm_min == 45);
        assert(writtenValue.tm_sec == 0);
        assert(writtenValue.tm_isdst == -1);

        writeCalled = false;

        const DateTime invalid{
            Date{2026, 2, 30},
            TimeOfDay{10, 45}};

        assert(
            clock.set(invalid) ==
            ClockError::InvalidDateTime);

        assert(!writeCalled);

        writeSucceeds = false;

        assert(
            clock.set(valid) ==
            ClockError::WriteFailed);

        assert(writeCalled);
    }

    void test_wake_delay_uses_current_seconds_exactly()
    {
        resetFakes();
        currentEpochSeconds = 1000000;
        convertedEpochSeconds = 1000061;

        const WakeDelayResult result =
            makeClock().delayUntil(
                DateTime{
                    Date{2026, 9, 17},
                    TimeOfDay{17, 0}});

        assert(result.error == ClockError::None);
        assert(!result.dueNow);
        assert(result.microseconds == 61000000ULL);
    }

    void test_non_positive_wake_delay_is_due_now()
    {
        resetFakes();
        currentEpochSeconds = 1000000;
        convertedEpochSeconds = 1000000;

        const WakeDelayResult result =
            makeClock().delayUntil(
                DateTime{
                    Date{2026, 9, 17},
                    TimeOfDay{17, 0}});

        assert(result.error == ClockError::None);
        assert(result.dueNow);
        assert(result.microseconds == 0);
    }

    void test_wake_delay_reports_invalid_and_conversion_errors()
    {
        resetFakes();
        assert(
            makeClock().delayUntil(
                DateTime{
                    Date{2026, 2, 30},
                    TimeOfDay{17, 0}}).error ==
            ClockError::InvalidDateTime);

        epochReadSucceeds = false;
        assert(
            makeClock().delayUntil(
                DateTime{
                    Date{2026, 9, 17},
                    TimeOfDay{17, 0}}).error ==
            ClockError::ReadFailed);

        resetFakes();
        conversionSucceeds = false;
        assert(
            makeClock().delayUntil(
                DateTime{
                    Date{2026, 9, 17},
                    TimeOfDay{17, 0}}).error ==
            ClockError::ReadFailed);
    }
}

int main()
{
    test_read_returns_current_local_date_time();
    test_read_reports_failure_and_unset_clock();
    test_set_validates_and_writes_local_date_time();
    test_wake_delay_uses_current_seconds_exactly();
    test_non_positive_wake_delay_is_due_now();
    test_wake_delay_reports_invalid_and_conversion_errors();

    std::cout
        << "All ESP32 clock tests passed.\n";

    return 0;
}
