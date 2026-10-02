#include "battery_hardware.h"
#include "board_pins.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif
void Esp32BatteryHardware::begin(){
#ifdef ARDUINO
    analogReadResolution(12);
    analogSetPinAttenuation(Esp32Board::BatteryAdc,ADC_11db);
#endif
}
std::optional<int> Esp32BatteryHardware::sample(){
#ifdef ARDUINO
    unsigned total=0;
    for(int i=0;i<16;++i)total+=analogReadMilliVolts(Esp32Board::BatteryAdc);
    status_.sampleAdcMillivolts(static_cast<int>((total+8)/16));
#endif
    return status_.percent();
}
