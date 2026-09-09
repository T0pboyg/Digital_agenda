#include <Arduino.h>

#include "board_pins.h"
#include "display_hardware.h"

DisplayHardware::DisplayHardware()
    : bus_(
          Esp32Board::LcdDataCommand,
          Esp32Board::LcdChipSelect,
          Esp32Board::LcdClock,
          Esp32Board::LcdMosi,
          Esp32Board::LcdMiso
      ),
      display_(
          &bus_,
          Esp32Board::LcdReset,
          Esp32Board::DisplayRotation,
          true
      )
{
}

bool DisplayHardware::begin()
{
    pinMode(
        Esp32Board::LcdBacklight,
        OUTPUT
    );

    disableBacklight();

    if (!display_.begin())
    {
        return false;
    }

    display_.setRotation(
        Esp32Board::DisplayRotation
    );

    return true;
}

void DisplayHardware::enableBacklight()
{
    digitalWrite(
        Esp32Board::LcdBacklight,
        HIGH
    );
}

void DisplayHardware::disableBacklight()
{
    digitalWrite(
        Esp32Board::LcdBacklight,
        LOW
    );
}

Arduino_GFX& DisplayHardware::graphics()
{
    return display_;
}
