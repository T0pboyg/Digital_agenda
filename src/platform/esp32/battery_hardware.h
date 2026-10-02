#pragma once
#include "core/battery_status.h"
class Esp32BatteryHardware {
public:
    void begin();
    std::optional<int> sample();
private:
    BatteryStatus status_;
};
