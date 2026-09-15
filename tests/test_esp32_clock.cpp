#include <cassert>
#include <ctime>
#include <iostream>

#include "platform/esp32/esp32_clock.h"

namespace
{
    bool readSucceeds = true;
    bool writeSucceeds = true;
    bool writeCalled = false;

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

    void resetFakes()
    {
        readSucceeds = true;
        writeSucceeds = true;
        writeCalled = false;

        readValue = {};
        writtenValue = {};
    }

    Esp32Clock makeClock()
    {
        return Esp32Clock{
            Esp32ClockApi{
                fakeReadLocalTime,
                fakeWriteLocalTime}};
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
}

int main()
{
    test_read_returns_current_local_date_time();
    test_read_reports_failure_and_unset_clock();
    test_set_validates_and_writes_local_date_time();

    std::cout
        << "All ESP32 clock tests passed.\n";

    return 0;
}