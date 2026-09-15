#pragma once

#include <cstdint>

#include "ui_geometry.h"

namespace DrawSurfaceMath
{

    constexpr std::uint16_t rgb888ToRgb565(
        UiColor color)
    {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(
                 color.red & 0xF8U)
             << 8U) |
            (static_cast<std::uint16_t>(
                 color.green & 0xFCU)
             << 3U) |
            (static_cast<std::uint16_t>(
                 color.blue) >>
             3U));
    }

    constexpr int alignedTextX(
        const UiRect &bounds,
        int textWidth,
        UiTextAlignment alignment)
    {
        switch (alignment)
        {
        case UiTextAlignment::Left:
            return bounds.x;

        case UiTextAlignment::Center:
            return bounds.x +
                   (bounds.width - textWidth) / 2;

        case UiTextAlignment::Right:
            return bounds.x +
                   bounds.width -
                   textWidth;
        }

        return bounds.x;
    }

}