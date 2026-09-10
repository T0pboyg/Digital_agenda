#include <Arduino.h>

#include <cstdint>

#include "board_pins.h"
#include "display_hardware.h"
#include "touch_hardware.h"

namespace
{
constexpr std::uint16_t BackgroundColor =
    0x0000;

constexpr std::uint16_t PrimaryTextColor =
    0xFFFF;

constexpr std::uint16_t AccentColor =
    0x06F7;

constexpr std::uint16_t ErrorColor =
    0xF800;

DisplayHardware displayHardware;
TouchHardware touchHardware;

bool touchReady = false;
bool hasLastPoint = false;
bool readFailureReported = false;

std::uint16_t lastX = 0;
std::uint16_t lastY = 0;

void drawTouchFrame(
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

    graphics.setTextWrap(false);
    graphics.setTextColor(
        AccentColor
    );
    graphics.setTextSize(2);
    graphics.setCursor(60, 24);
    graphics.print("TOUCH TEST");

    graphics.setTextColor(
        PrimaryTextColor
    );
    graphics.setTextSize(1);
    graphics.setCursor(84, 204);
    graphics.print("TOUCH SCREEN");
}

void drawTouchPoint(
    Arduino_GFX& graphics,
    const TouchSample& sample
)
{
    drawTouchFrame(
        graphics
    );

    const auto x =
        static_cast<std::int16_t>(sample.x);

    const auto y =
        static_cast<std::int16_t>(sample.y);

    graphics.drawFastHLine(
        x - 10,
        y,
        21,
        PrimaryTextColor
    );

    graphics.drawFastVLine(
        x,
        y - 10,
        21,
        PrimaryTextColor
    );

    graphics.drawCircle(
        x,
        y,
        4,
        AccentColor
    );
}

void drawTouchFailure(
    Arduino_GFX& graphics
)
{
    drawTouchFrame(
        graphics
    );

    graphics.setTextColor(
        ErrorColor
    );
    graphics.setTextSize(2);
    graphics.setCursor(54, 112);
    graphics.print("INIT FAILED");
}
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println(
        "TOUCH_SMOKE_START"
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

    drawTouchFrame(
        displayHardware.graphics()
    );

    displayHardware.enableBacklight();

    if (!touchHardware.begin())
    {
        Serial.println(
            "TOUCH_INIT_FAILED"
        );

        drawTouchFailure(
            displayHardware.graphics()
        );

        return;
    }

    touchReady = true;

    Serial.println(
        "TOUCH_INIT_OK"
    );

    Serial.println(
        "TOUCH_SMOKE_READY"
    );
}

void loop()
{
    if (!touchReady)
    {
        delay(1000);
        return;
    }

    TouchSample sample{
        false,
        0,
        0
    };

    if (!touchHardware.read(sample))
    {
        if (!readFailureReported)
        {
            Serial.println(
                "TOUCH_READ_FAILED"
            );

            readFailureReported = true;
        }

        delay(50);
        return;
    }

    readFailureReported = false;

    if (!sample.touched)
    {
        hasLastPoint = false;
        delay(20);
        return;
    }

    const bool pointChanged =
        !hasLastPoint ||
        sample.x != lastX ||
        sample.y != lastY;

    if (!pointChanged)
    {
        delay(20);
        return;
    }

    hasLastPoint = true;
    lastX = sample.x;
    lastY = sample.y;

    Serial.print("TOUCH x=");
    Serial.print(sample.x);
    Serial.print(" y=");
    Serial.println(sample.y);

    const bool pointInRange =
        sample.x < static_cast<std::uint16_t>(
            Esp32Board::DisplayWidth
        ) &&
        sample.y < static_cast<std::uint16_t>(
            Esp32Board::DisplayHeight
        );

    if (!pointInRange)
    {
        Serial.println(
            "TOUCH_OUT_OF_RANGE"
        );

        delay(20);
        return;
    }

    drawTouchPoint(
        displayHardware.graphics(),
        sample
    );

    delay(20);
}
