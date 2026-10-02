#pragma once
#include <optional>
std::optional<int> batteryPercent(int millivolts);
class BatteryStatus {
public:
    void sampleAdcMillivolts(int adcMillivolts);
    std::optional<int> percent() const {return batteryPercent(millivolts_);}
    int millivolts() const {return millivolts_;}
private:
    int millivolts_=0;
};
