#pragma once

#include <Arduino_GFX_Library.h>

#include "ui/power_hardware.h"

class DisplayHardware final : public BacklightControl
{
public:
    DisplayHardware();

    bool begin();
    void enableBacklight() override;
    void disableBacklight() override;

    Arduino_GFX& graphics();

private:
    Arduino_ESP32SPI bus_;
    Arduino_GC9A01 display_;
};
