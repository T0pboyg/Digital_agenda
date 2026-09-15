#pragma once

#include <cstdint>

constexpr int AGENDA_UI_DISPLAY_WIDTH = 240;
constexpr int AGENDA_UI_DISPLAY_HEIGHT = 240;

struct UiPoint
{
    int x;
    int y;
};

struct UiRect
{
    int x;
    int y;
    int width;
    int height;
};

constexpr bool operator==(
    const UiRect& left,
    const UiRect& right
)
{
    return
        left.x == right.x &&
        left.y == right.y &&
        left.width == right.width &&
        left.height == right.height;
}

struct UiColor
{
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

constexpr bool operator==(
    const UiColor& left,
    const UiColor& right
)
{
    return
        left.red == right.red &&
        left.green == right.green &&
        left.blue == right.blue;
}

constexpr bool operator!=(
    const UiColor& left,
    const UiColor& right
)
{
    return !(left == right);
}

enum class UiTextStyle
{
    Title,
    Heading,
    Body,
    Caption,
    Keypad
};

enum class UiTextAlignment
{
    Left,
    Center,
    Right
};

namespace AgendaUiColors
{
constexpr UiColor Background{
    0x0B,
    0x12,
    0x17
};

constexpr UiColor Surface{
    0x17,
    0x24,
    0x2B
};

constexpr UiColor Accent{
    0x27,
    0xD3,
    0xC2
};

constexpr UiColor PrimaryText{
    0xF4,
    0xF7,
    0xF8
};

constexpr UiColor SecondaryText{
    0x9A,
    0xAA,
    0xB4
};

constexpr UiColor Disabled{
    0x58,
    0x67,
    0x70
};

constexpr UiColor Error{
    0xFF,
    0x6B,
    0x6B
};

constexpr UiColor Overlay{
    0x05,
    0x08,
    0x0A
};
}

namespace AgendaUiLayout
{
constexpr UiRect FullDisplay{
    0,
    0,
    240,
    240
};

constexpr UiRect Header{
    34,
    8,
    172,
    28
};

constexpr UiRect MonthPrevious{
    44,
    184,
    56,
    32
};

constexpr UiRect MonthNext{
    140,
    184,
    56,
    32
};

constexpr UiRect Subheader{
    24,
    36,
    192,
    20
};

constexpr UiRect Content{
    20,
    58,
    200,
    126
};

constexpr UiRect FooterLeft{
    24,
    196,
    56,
    32
};

constexpr UiRect FooterCenter{
    88,
    196,
    64,
    32
};

constexpr UiRect FooterRight{
    160,
    196,
    56,
    32
};

constexpr UiRect OverlayCard{
    28,
    48,
    184,
    144
};

constexpr UiRect MonthWeekdays{
    22,
    38,
    196,
    16
};

constexpr UiRect MonthGrid{
    22,
    56,
    196,
    126
};

constexpr int MonthCellWidth = 28;
constexpr int MonthCellHeight = 21;

constexpr UiRect DayRow0{
    25,
    60,
    190,
    38
};

constexpr UiRect DayRow1{
    25,
    102,
    190,
    38
};

constexpr UiRect DayRow2{
    25,
    144,
    190,
    38
};

constexpr UiRect EditorValue{
    28,
    58,
    184,
    52
};

constexpr UiRect EditorSecondary{
    28,
    116,
    184,
    60
};

constexpr UiRect EditorTimedMode{
    28,
    58,
    88,
    32
};

constexpr UiRect EditorAllDayMode{
    124,
    58,
    88,
    32
};

constexpr UiRect EditorHourUp{
    42,
    110,
    56,
    18
};

constexpr UiRect EditorHourDown{
    42,
    158,
    56,
    18
};

constexpr UiRect EditorMinuteUp{
    142,
    110,
    56,
    18
};

constexpr UiRect EditorMinuteDown{
    142,
    158,
    56,
    18
};

constexpr UiRect ClockDatePrevious{
    20,
    67,
    28,
    34
};

constexpr UiRect ClockDateNext{
    192,
    67,
    28,
    34
};

constexpr UiRect ClockHourUp{
    42,
    116,
    56,
    18
};

constexpr UiRect ClockHourDown{
    42,
    158,
    56,
    18
};

constexpr UiRect ClockMinuteUp{
    142,
    116,
    56,
    18
};

constexpr UiRect ClockMinuteDown{
    142,
    158,
    56,
    18
};

constexpr UiRect T9Preview{
    34,
    34,
    172,
    42
};

constexpr UiRect T9Status{
    38,
    76,
    164,
    16
};

constexpr UiRect DayBack{
    56,
    10,
    28,
    28
};

constexpr UiRect DayHeader{
    52,
    40,
    136,
    18
};

constexpr UiRect DayCreate{
    156,
    10,
    28,
    28
};

constexpr UiRect EditorDatePrevious{
    20,
    67,
    28,
    34
};

constexpr UiRect EditorDateValue{
    58,
    58,
    124,
    52
};

constexpr UiRect EditorDateNext{
    192,
    67,
    28,
    34
};

constexpr int T9GridX = 45;
constexpr int T9GridY = 94;
constexpr int T9KeyWidth = 50;
constexpr int T9KeyHeight = 22;
constexpr int T9ColumnGap = 5;
constexpr int T9RowGap = 2;
}