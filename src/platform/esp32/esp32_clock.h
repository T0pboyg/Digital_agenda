#pragma once

#include <ctime>

#include "core/clock.h"

struct Esp32ClockApi
{
    using ReadLocalTimeFunction =
        bool (*)(std::tm &);

    using WriteLocalTimeFunction =
        bool (*)(const std::tm &);

    ReadLocalTimeFunction readLocalTime =
        nullptr;

    WriteLocalTimeFunction writeLocalTime =
        nullptr;
};

class Esp32Clock final : public Clock
{
public:
    Esp32Clock();

    explicit Esp32Clock(
        Esp32ClockApi api);

    ClockReadResult read() const override;

    ClockError set(
        const DateTime &value) override;

private:
    Esp32ClockApi api_;
};