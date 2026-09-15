#include <cassert>
#include <iostream>

#include "ui/draw_surface_math.h"

void test_rgb565_converts_black_and_white()
{
    assert(
        DrawSurfaceMath::rgb888ToRgb565(
            UiColor{0x00, 0x00, 0x00}) == 0x0000U);

    assert(
        DrawSurfaceMath::rgb888ToRgb565(
            UiColor{0xFF, 0xFF, 0xFF}) == 0xFFFFU);
}

void test_rgb565_converts_accent_color()
{
    assert(
        DrawSurfaceMath::rgb888ToRgb565(
            UiColor{0x27, 0xD3, 0xC2}) == 0x2698U);
}

void test_text_x_respects_alignment()
{
    constexpr UiRect bounds{
        20,
        10,
        100,
        30};

    constexpr int textWidth = 24;

    assert(
        DrawSurfaceMath::alignedTextX(
            bounds,
            textWidth,
            UiTextAlignment::Left) == 20);

    assert(
        DrawSurfaceMath::alignedTextX(
            bounds,
            textWidth,
            UiTextAlignment::Center) == 58);

    assert(
        DrawSurfaceMath::alignedTextX(
            bounds,
            textWidth,
            UiTextAlignment::Right) == 96);
}

int main()
{
    test_rgb565_converts_black_and_white();
    test_rgb565_converts_accent_color();
    test_text_x_respects_alignment();

    std::cout
        << "All draw surface math tests passed.\n";

    return 0;
}