#pragma once

#include "ui/draw_surface.h"

class Arduino_GFX;

class ArduinoGfxSurface final : public DrawSurface
{
public:
    explicit ArduinoGfxSurface(
        Arduino_GFX &graphics);

    void clear(
        UiColor color) override;

    void fillRectangle(
        const UiRect &bounds,
        UiColor color) override;

    void drawRectangle(
        const UiRect &bounds,
        UiColor color,
        int thickness) override;

    void fillRoundedRectangle(
        const UiRect &bounds,
        int radius,
        UiColor color) override;

    void drawRoundedRectangle(
        const UiRect &bounds,
        int radius,
        UiColor color,
        int thickness) override;

    void fillCircle(
        UiPoint center,
        int radius,
        UiColor color) override;

    void drawCircle(
        UiPoint center,
        int radius,
        UiColor color,
        int thickness) override;

    void drawLine(
        UiPoint start,
        UiPoint end,
        UiColor color,
        int thickness) override;

    void drawText(
        const UiRect &bounds,
        std::string_view text,
        UiTextStyle style,
        UiColor color,
        UiTextAlignment alignment) override;

private:
    Arduino_GFX &graphics_;
};