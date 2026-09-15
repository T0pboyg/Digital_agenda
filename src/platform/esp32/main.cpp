#include <Arduino.h>

#include "arduino_gfx_surface.h"
#include "display_hardware.h"

namespace
{

    DisplayHardware displayHardware;

    ArduinoGfxSurface drawSurface{
        displayHardware.graphics()};

    void drawSmokeScreen()
    {
        drawSurface.clear(
            AgendaUiColors::Background);

        drawSurface.drawCircle(
            UiPoint{120, 120},
            112,
            AgendaUiColors::Accent,
            2);

        drawSurface.fillRoundedRectangle(
            UiRect{28, 32, 184, 42},
            10,
            AgendaUiColors::Accent);

        drawSurface.drawText(
            UiRect{32, 39, 176, 28},
            "AGENDA 123",
            UiTextStyle::Title,
            AgendaUiColors::Background,
            UiTextAlignment::Center);

        drawSurface.drawText(
            UiRect{20, 91, 200, 30},
            "ΗΜΕΡΟΛΟΓΙΟ",
            UiTextStyle::Heading,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        drawSurface.drawText(
            UiRect{35, 132, 170, 24},
            "ΩΡΑ 08:30",
            UiTextStyle::Body,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);

        drawSurface.drawLine(
            UiPoint{55, 169},
            UiPoint{185, 169},
            AgendaUiColors::SecondaryText,
            2);

        drawSurface.drawText(
            UiRect{40, 180, 160, 20},
            "DISPLAY TEST",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);
    }

}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println(
        "SURFACE_SMOKE_START");

    if (!displayHardware.begin())
    {
        Serial.println(
            "DISPLAY_INIT_FAILED");

        return;
    }

    drawSmokeScreen();
    displayHardware.enableBacklight();

    Serial.println(
        "SURFACE_SMOKE_READY");
}

void loop()
{
    delay(1000);
}