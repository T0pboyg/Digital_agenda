#pragma once

#include <Arduino_GFX_Library.h>

class DisplayHardware
{
public:
    DisplayHardware();

    bool begin();
    void enableBacklight();
    void disableBacklight();

    Arduino_GFX& graphics();

private:
    Arduino_ESP32SPI bus_;
    Arduino_GC9A01 display_;
};
