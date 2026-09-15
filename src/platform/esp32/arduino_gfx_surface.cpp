#include <Arduino.h>
#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "arduino_gfx_surface.h"
#include "ui/draw_surface_math.h"

namespace
{

    std::uint16_t nativeColor(
        UiColor color)
    {
        return DrawSurfaceMath::rgb888ToRgb565(
            color);
    }

    std::uint8_t textScale(UiTextStyle)
    {
        return 1;
    }
}

ArduinoGfxSurface::ArduinoGfxSurface(
    Arduino_GFX &graphics)
    : graphics_(graphics)
{
}

void ArduinoGfxSurface::clear(
    UiColor color)
{
    graphics_.fillScreen(
        nativeColor(color));
}

void ArduinoGfxSurface::fillRectangle(
    const UiRect &bounds,
    UiColor color)
{
    if (
        bounds.width <= 0 ||
        bounds.height <= 0)
    {
        return;
    }

    graphics_.fillRect(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        nativeColor(color));
}

void ArduinoGfxSurface::drawRectangle(
    const UiRect &bounds,
    UiColor color,
    int thickness)
{
    if (thickness <= 0)
    {
        return;
    }

    for (int layer = 0; layer < thickness; ++layer)
    {
        const int width =
            bounds.width - 2 * layer;

        const int height =
            bounds.height - 2 * layer;

        if (width <= 0 || height <= 0)
        {
            break;
        }

        graphics_.drawRect(
            bounds.x + layer,
            bounds.y + layer,
            width,
            height,
            nativeColor(color));
    }
}

void ArduinoGfxSurface::fillRoundedRectangle(
    const UiRect &bounds,
    int radius,
    UiColor color)
{
    if (
        bounds.width <= 0 ||
        bounds.height <= 0)
    {
        return;
    }

    graphics_.fillRoundRect(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        std::max(0, radius),
        nativeColor(color));
}

void ArduinoGfxSurface::drawRoundedRectangle(
    const UiRect &bounds,
    int radius,
    UiColor color,
    int thickness)
{
    if (thickness <= 0)
    {
        return;
    }

    for (int layer = 0; layer < thickness; ++layer)
    {
        const int width =
            bounds.width - 2 * layer;

        const int height =
            bounds.height - 2 * layer;

        if (width <= 0 || height <= 0)
        {
            break;
        }

        graphics_.drawRoundRect(
            bounds.x + layer,
            bounds.y + layer,
            width,
            height,
            std::max(0, radius - layer),
            nativeColor(color));
    }
}

void ArduinoGfxSurface::fillCircle(
    UiPoint center,
    int radius,
    UiColor color)
{
    if (radius < 0)
    {
        return;
    }

    graphics_.fillCircle(
        center.x,
        center.y,
        radius,
        nativeColor(color));
}

void ArduinoGfxSurface::drawCircle(
    UiPoint center,
    int radius,
    UiColor color,
    int thickness)
{
    if (
        radius < 0 ||
        thickness <= 0)
    {
        return;
    }

    for (
        int layer = 0;
        layer < thickness &&
        radius - layer >= 0;
        ++layer)
    {
        graphics_.drawCircle(
            center.x,
            center.y,
            radius - layer,
            nativeColor(color));
    }
}

void ArduinoGfxSurface::drawLine(
    UiPoint start,
    UiPoint end,
    UiColor color,
    int thickness)
{
    if (thickness <= 0)
    {
        return;
    }

    const int deltaX =
        end.x - start.x;

    const int deltaY =
        end.y - start.y;

    const bool mostlyHorizontal =
        std::abs(deltaX) >=
        std::abs(deltaY);

    const int firstOffset =
        -(thickness / 2);

    for (int index = 0; index < thickness; ++index)
    {
        const int offset =
            firstOffset + index;

        const int offsetX =
            mostlyHorizontal
                ? 0
                : offset;

        const int offsetY =
            mostlyHorizontal
                ? offset
                : 0;

        graphics_.drawLine(
            start.x + offsetX,
            start.y + offsetY,
            end.x + offsetX,
            end.y + offsetY,
            nativeColor(color));
    }
}

void ArduinoGfxSurface::drawText(
    const UiRect &bounds,
    std::string_view text,
    UiTextStyle style,
    UiColor color,
    UiTextAlignment alignment)
{
    if (
        text.empty() ||
        bounds.width <= 0 ||
        bounds.height <= 0)
    {
        return;
    }

    graphics_.setUTF8Print(true);

    graphics_.setFont(
        u8g2_font_unifont_t_greek);
    graphics_.setTextWrap(false);
    graphics_.setTextSize(
        textScale(style));

    graphics_.setTextColor(
        nativeColor(color));

    const std::string measuredText{
        text};

    std::int16_t textX1 = 0;
    std::int16_t textY1 = 0;
    std::uint16_t textWidth = 0;
    std::uint16_t textHeight = 0;

    graphics_.getTextBounds(
        measuredText.c_str(),
        0,
        0,
        &textX1,
        &textY1,
        &textWidth,
        &textHeight);

    const int textBoundsX =
        DrawSurfaceMath::alignedTextX(
            bounds,
            static_cast<int>(textWidth),
            alignment);

    const int textBoundsY =
        bounds.y +
        (bounds.height -
         static_cast<int>(textHeight)) /
            2;

    graphics_.setTextBound(
        0,
        0,
        graphics_.width(),
        graphics_.height());

    graphics_.setCursor(
        textBoundsX -
            static_cast<int>(textX1),
        textBoundsY -
            static_cast<int>(textY1));

    static_cast<Print &>(graphics_).write(
        reinterpret_cast<const std::uint8_t *>(
            text.data()),
        text.size());
}