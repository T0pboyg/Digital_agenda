#include <Arduino.h>

#include <cstdint>

#include "display_hardware.h"

namespace
{
constexpr std::uint16_t BackgroundColor =
    0x0000;

constexpr std::uint16_t PrimaryTextColor =
    0xFFFF;

constexpr std::uint16_t AccentColor =
    0x06F7;

constexpr std::uint16_t RedColor =
    0xF800;

constexpr std::uint16_t GreenColor =
    0x07E0;

constexpr std::uint16_t BlueColor =
    0x001F;

DisplayHardware displayHardware;

void drawSmokeFrame(
    Arduino_GFX& graphics
)
{
    graphics.fillScreen(
        BackgroundColor
    );

    graphics.drawCircle(
        120,
        120,
        112,
        AccentColor
    );

    graphics.drawCircle(
        120,
        120,
        111,
        AccentColor
    );

    graphics.fillRect(
        49,
        55,
        38,
        38,
        RedColor
    );

    graphics.fillRect(
        101,
        55,
        38,
        38,
        GreenColor
    );

    graphics.fillRect(
        153,
        55,
        38,
        38,
        BlueColor
    );

    graphics.setTextWrap(false);

    graphics.setTextColor(
        PrimaryTextColor
    );
    graphics.setTextSize(2);
    graphics.setCursor(102, 22);
    graphics.print("TOP");

    graphics.setTextColor(
        AccentColor
    );
    graphics.setCursor(42, 112);
    graphics.print("DIGITAL DIARY");

    graphics.drawFastHLine(
        60,
        145,
        120,
        AccentColor
    );

    graphics.setTextColor(
        PrimaryTextColor
    );
    graphics.setCursor(102, 174);
    graphics.print("USB");
}
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println(
        "DISPLAY_SMOKE_START"
    );

    if (!displayHardware.begin())
    {
        Serial.println(
            "DISPLAY_INIT_FAILED"
        );

        return;
    }

    Serial.println(
        "DISPLAY_INIT_OK"
    );

    drawSmokeFrame(
        displayHardware.graphics()
    );

    displayHardware.enableBacklight();

    Serial.println(
        "DISPLAY_SMOKE_READY"
    );
}

void loop()
{
    delay(1000);
}
