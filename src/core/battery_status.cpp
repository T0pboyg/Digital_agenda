#include "battery_status.h"
#include <array>
// Approximate resting LiPo voltage curve; not a fuel gauge or charging detector.
std::optional<int> batteryPercent(int mv) {
    if(mv<2800 || mv>4350) return std::nullopt;
    constexpr std::array<int,11> volts{3300,3500,3650,3720,3760,3800,3850,3900,3980,4080,4200};
    if(mv<=volts.front()) return 0;
    for(std::size_t i=1;i<volts.size();++i) if(mv<volts[i])
        return static_cast<int>((i-1)*10)+(mv-volts[i-1])*10/(volts[i]-volts[i-1]);
    return 100;
}
void BatteryStatus::sampleAdcMillivolts(int adc) {
    if(adc<934 || adc>1450) {millivolts_=0; return;}
    const int sample=adc*3;
    millivolts_=millivolts_==0 ? sample : (millivolts_*3+sample+2)/4;
}
