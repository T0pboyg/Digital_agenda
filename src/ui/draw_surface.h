#pragma once

#include <string_view>

#include "ui_geometry.h"

class DrawSurface
{
public:
    virtual ~DrawSurface() = default;

    virtual void clear(
        UiColor color
    ) = 0;

    virtual void fillRectangle(
        const UiRect& bounds,
        UiColor color
    ) = 0;

    virtual void drawRectangle(
        const UiRect& bounds,
        UiColor color,
        int thickness
    ) = 0;

    virtual void fillRoundedRectangle(
        const UiRect& bounds,
        int radius,
        UiColor color
    ) = 0;

    virtual void drawRoundedRectangle(
        const UiRect& bounds,
        int radius,
        UiColor color,
        int thickness
    ) = 0;

    virtual void fillCircle(
        UiPoint center,
        int radius,
        UiColor color
    ) = 0;

    virtual void drawCircle(
        UiPoint center,
        int radius,
        UiColor color,
        int thickness
    ) = 0;

    virtual void drawLine(
        UiPoint start,
        UiPoint end,
        UiColor color,
        int thickness
    ) = 0;

    virtual void drawText(
        const UiRect& bounds,
        std::string_view text,
        UiTextStyle style,
        UiColor color,
        UiTextAlignment alignment
    ) = 0;
};