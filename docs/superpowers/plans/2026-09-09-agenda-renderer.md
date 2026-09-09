# Agenda Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a hardware-independent 240 x 240 renderer that converts `AgendaUiView` into tested drawing operations for every diary screen and overlay.

**Architecture:** `AgendaRenderer` is a stateless immediate-mode renderer. It reads a `const AgendaUiView&` and writes primitive operations to an abstract `DrawSurface`; desktop tests use a recording surface, and the later GC9A01A adapter will implement the same interface.

**Tech Stack:** C++17, standard library only, MSYS2 MinGW g++ 15.2.0, assert-based desktop tests.

**Spec:** `docs/superpowers/specs/2026-09-09-agenda-renderer-design.md`

## Global Constraints

- Build with `-std=c++17 -Wall -Wextra -pedantic -Isrc` and accept no warnings.
- The display coordinate space is exactly 240 x 240 pixels.
- Keep `src/core` independent of Arduino, ESP32, GC9A01A, CST816S, LVGL, SPI, and hardware font types.
- Use the approved dark palette: background `#0B1217`, surface `#17242B`, accent `#27D3C2`, primary text `#F4F7F8`, secondary text `#9AAAB4`, disabled `#586770`, error `#FF6B6B`, and overlay `#05080A`.
- Render complete static frames only when the application requests them; no animation or continuous refresh belongs in this stage.
- Keep dates in `DD/MM/YYYY`, times in 24-hour `HH:MM`, and fixed interface text in Greek.
- Preserve UTF-8 and never split a code point when shortening text.
- Do not change UI state, call the controller, read the clock, access storage, or interpret touch input from the renderer.
- Work inline on `main`, as requested by the user.
- Add no comments inside production code.
- Implement and verify tests in batches of two or three.

## File Map

- Modify `src/core/utf8.h`: expose a reusable UTF-8 prefix function.
- Modify `src/core/utf8.cpp`: implement safe prefix extraction.
- Modify `tests/test_utf8.cpp`: cover ASCII, Greek, zero, and oversized prefix limits.
- Create `src/ui/ui_geometry.h`: own display dimensions, value types, palette, and shared layout geometry.
- Create `src/ui/draw_surface.h`: define the hardware-neutral drawing output port.
- Create `src/ui/agenda_renderer.h`: expose the stateless renderer API.
- Create `src/ui/agenda_renderer.cpp`: implement screen dispatch, layouts, formatting, and overlays.
- Create `tests/test_agenda_renderer.cpp`: own `RecordingDrawSurface` and all semantic renderer tests.

---

### Task 1: Add reusable UTF-8 prefix extraction

**Files:**
- Modify: `src/core/utf8.h`
- Modify: `src/core/utf8.cpp`
- Modify: `tests/test_utf8.cpp`

**Interfaces:**
- Consumes: valid or partially valid `std::string_view` UTF-8 input.
- Produces: `std::string utf8Prefix(std::string_view text, std::size_t maximumCodePoints)`.

- [ ] **Step 1: Add three failing tests**

Add these functions before `main()` in `tests/test_utf8.cpp`:

```cpp
void test_utf8_prefix_limits_ascii_text()
{
    assert(utf8Prefix("abcdef", 3) == "abc");
}

void test_utf8_prefix_preserves_greek_code_points()
{
    const std::string result =
        utf8Prefix("Ημερολόγιο", 4);

    assert(result == "Ημερ");
    assert(isValidUtf8(result));
    assert(utf8CodePointCount(result) == 4);
}

void test_utf8_prefix_handles_limit_edges()
{
    assert(utf8Prefix("abc", 0).empty());
    assert(utf8Prefix("abc", 9) == "abc");
}
```

Call them before the success message in `main()`:

```cpp
test_utf8_prefix_limits_ascii_text();
test_utf8_prefix_preserves_greek_code_points();
test_utf8_prefix_handles_limit_edges();
```

- [ ] **Step 2: Compile and confirm the RED state**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_utf8.cpp src\core\utf8.cpp -o build\test_utf8.exe
```

Expected: compilation fails because `utf8Prefix` is not declared.

- [ ] **Step 3: Declare the function**

Add to `src/core/utf8.h`:

```cpp
std::string utf8Prefix(
    std::string_view text,
    std::size_t maximumCodePoints
);
```

- [ ] **Step 4: Implement complete-code-point extraction**

Add to `src/core/utf8.cpp`:

```cpp
std::string utf8Prefix(
    std::string_view text,
    std::size_t maximumCodePoints
)
{
    std::size_t position = 0;
    std::size_t count = 0;

    while (
        position < text.size() &&
        count < maximumCodePoints
    )
    {
        const unsigned char lead =
            static_cast<unsigned char>(
                text[position]
            );

        std::size_t length = 0;

        if (lead <= 0x7F)
        {
            length = 1;
        }
        else if ((lead & 0xE0) == 0xC0)
        {
            length = 2;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
            length = 3;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
            length = 4;
        }
        else
        {
            break;
        }

        if (position + length > text.size())
        {
            break;
        }

        bool complete = true;

        for (
            std::size_t offset = 1;
            offset < length;
            ++offset
        )
        {
            const unsigned char continuation =
                static_cast<unsigned char>(
                    text[position + offset]
                );

            if ((continuation & 0xC0) != 0x80)
            {
                complete = false;
                break;
            }
        }

        if (!complete)
        {
            break;
        }

        position += length;
        ++count;
    }

    return std::string(text.substr(0, position));
}
```

- [ ] **Step 5: Rebuild and run the UTF-8 suite**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_utf8.cpp src\core\utf8.cpp -o build\test_utf8.exe
.\build\test_utf8.exe
```

Expected:

```text
All UTF-8 tests passed.
```

- [ ] **Step 6: Check and commit**

```powershell
git diff --check
git add src/core/utf8.h src/core/utf8.cpp tests/test_utf8.cpp
git diff --cached --check
git commit -m "feat: add UTF-8 prefix extraction"
```

---

### Task 2: Establish geometry, drawing surface, and base renderer

**Files:**
- Create: `src/ui/ui_geometry.h`
- Create: `src/ui/draw_surface.h`
- Create: `src/ui/agenda_renderer.h`
- Create: `src/ui/agenda_renderer.cpp`
- Create: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Consumes: `AgendaUiView` from `core/agenda_ui_types.h`.
- Produces: `UiPoint`, `UiRect`, `UiColor`, `UiTextStyle`, `UiTextAlignment`, `DrawSurface`, and `AgendaRenderer::render(const AgendaUiView&, DrawSurface&) const`.

- [ ] **Step 1: Create the geometry contract**

Create `src/ui/ui_geometry.h` with these public types and constants:

```cpp
#pragma once

#include <cstddef>
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
constexpr UiColor Background{0x0B, 0x12, 0x17};
constexpr UiColor Surface{0x17, 0x24, 0x2B};
constexpr UiColor Accent{0x27, 0xD3, 0xC2};
constexpr UiColor PrimaryText{0xF4, 0xF7, 0xF8};
constexpr UiColor SecondaryText{0x9A, 0xAA, 0xB4};
constexpr UiColor Disabled{0x58, 0x67, 0x70};
constexpr UiColor Error{0xFF, 0x6B, 0x6B};
constexpr UiColor Overlay{0x05, 0x08, 0x0A};
}

namespace AgendaUiLayout
{
constexpr UiRect FullDisplay{0, 0, 240, 240};
constexpr UiRect Header{34, 8, 172, 28};
constexpr UiRect Subheader{24, 36, 192, 20};
constexpr UiRect Content{20, 58, 200, 126};
constexpr UiRect FooterLeft{24, 196, 56, 32};
constexpr UiRect FooterCenter{88, 196, 64, 32};
constexpr UiRect FooterRight{160, 196, 56, 32};
constexpr UiRect OverlayCard{28, 48, 184, 144};
constexpr UiRect MonthWeekdays{22, 38, 196, 16};
constexpr UiRect MonthGrid{22, 56, 196, 126};
constexpr int MonthCellWidth = 28;
constexpr int MonthCellHeight = 21;
constexpr UiRect DayRow0{25, 54, 190, 38};
constexpr UiRect DayRow1{25, 96, 190, 38};
constexpr UiRect DayRow2{25, 138, 190, 38};
constexpr UiRect EditorValue{28, 58, 184, 52};
constexpr UiRect EditorSecondary{28, 116, 184, 60};
constexpr UiRect T9Preview{34, 34, 172, 42};
constexpr UiRect T9Status{38, 76, 164, 16};
constexpr int T9GridX = 45;
constexpr int T9GridY = 94;
constexpr int T9KeyWidth = 50;
constexpr int T9KeyHeight = 22;
constexpr int T9ColumnGap = 5;
constexpr int T9RowGap = 2;
}
```

- [ ] **Step 2: Create the drawing port**

Create `src/ui/draw_surface.h`:

```cpp
#pragma once

#include <string_view>

#include "ui_geometry.h"

class DrawSurface
{
public:
    virtual ~DrawSurface() = default;

    virtual void clear(UiColor color) = 0;

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
```

- [ ] **Step 3: Create the renderer API**

Create `src/ui/agenda_renderer.h`:

```cpp
#pragma once

#include "core/agenda_ui_types.h"
#include "draw_surface.h"

class AgendaRenderer
{
public:
    void render(
        const AgendaUiView& view,
        DrawSurface& surface
    ) const;
};
```

- [ ] **Step 4: Write two failing base-frame tests**

Create `tests/test_agenda_renderer.cpp`. Define `DrawCommandType`,
`DrawCommand`, and `RecordingDrawSurface` with one override for every
`DrawSurface` method. Every override appends a command to
`std::vector<DrawCommand> commands`.

Use this command record:

```cpp
enum class DrawCommandType
{
    Clear,
    FillRectangle,
    DrawRectangle,
    FillRoundedRectangle,
    DrawRoundedRectangle,
    FillCircle,
    DrawCircle,
    DrawLine,
    DrawText
};

struct DrawCommand
{
    DrawCommandType type = DrawCommandType::Clear;
    UiRect bounds{0, 0, 0, 0};
    UiPoint start{0, 0};
    UiPoint end{0, 0};
    int radius = 0;
    int thickness = 0;
    UiColor color{0, 0, 0};
    UiTextStyle textStyle = UiTextStyle::Body;
    UiTextAlignment alignment =
        UiTextAlignment::Left;
    std::string text;
};
```

Use this complete recording surface immediately after `DrawCommand`:

```cpp
class RecordingDrawSurface final : public DrawSurface
{
public:
    std::vector<DrawCommand> commands;

    void clear(UiColor color) override
    {
        DrawCommand command;
        command.type = DrawCommandType::Clear;
        command.color = color;
        commands.push_back(command);
    }

    void fillRectangle(
        const UiRect& bounds,
        UiColor color
    ) override
    {
        recordRectangle(
            DrawCommandType::FillRectangle,
            bounds,
            0,
            0,
            color
        );
    }

    void drawRectangle(
        const UiRect& bounds,
        UiColor color,
        int thickness
    ) override
    {
        recordRectangle(
            DrawCommandType::DrawRectangle,
            bounds,
            0,
            thickness,
            color
        );
    }

    void fillRoundedRectangle(
        const UiRect& bounds,
        int radius,
        UiColor color
    ) override
    {
        recordRectangle(
            DrawCommandType::FillRoundedRectangle,
            bounds,
            radius,
            0,
            color
        );
    }

    void drawRoundedRectangle(
        const UiRect& bounds,
        int radius,
        UiColor color,
        int thickness
    ) override
    {
        recordRectangle(
            DrawCommandType::DrawRoundedRectangle,
            bounds,
            radius,
            thickness,
            color
        );
    }

    void fillCircle(
        UiPoint center,
        int radius,
        UiColor color
    ) override
    {
        recordCircle(
            DrawCommandType::FillCircle,
            center,
            radius,
            0,
            color
        );
    }

    void drawCircle(
        UiPoint center,
        int radius,
        UiColor color,
        int thickness
    ) override
    {
        recordCircle(
            DrawCommandType::DrawCircle,
            center,
            radius,
            thickness,
            color
        );
    }

    void drawLine(
        UiPoint start,
        UiPoint end,
        UiColor color,
        int thickness
    ) override
    {
        DrawCommand command;
        command.type = DrawCommandType::DrawLine;
        command.start = start;
        command.end = end;
        command.color = color;
        command.thickness = thickness;
        commands.push_back(command);
    }

    void drawText(
        const UiRect& bounds,
        std::string_view text,
        UiTextStyle style,
        UiColor color,
        UiTextAlignment alignment
    ) override
    {
        DrawCommand command;
        command.type = DrawCommandType::DrawText;
        command.bounds = bounds;
        command.color = color;
        command.textStyle = style;
        command.alignment = alignment;
        command.text = std::string(text);
        commands.push_back(command);
    }

private:
    void recordRectangle(
        DrawCommandType type,
        const UiRect& bounds,
        int radius,
        int thickness,
        UiColor color
    )
    {
        DrawCommand command;
        command.type = type;
        command.bounds = bounds;
        command.radius = radius;
        command.thickness = thickness;
        command.color = color;
        commands.push_back(command);
    }

    void recordCircle(
        DrawCommandType type,
        UiPoint center,
        int radius,
        int thickness,
        UiColor color
    )
    {
        DrawCommand command;
        command.type = type;
        command.start = center;
        command.radius = radius;
        command.thickness = thickness;
        command.color = color;
        commands.push_back(command);
    }
};

bool containsText(
    const RecordingDrawSurface& surface,
    std::string_view expected
)
{
    return std::any_of(
        surface.commands.begin(),
        surface.commands.end(),
        [expected](const DrawCommand& command)
        {
            return
                command.type == DrawCommandType::DrawText &&
                command.text == expected;
        }
    );
}

bool containsTextWithColor(
    const RecordingDrawSurface& surface,
    std::string_view expected,
    UiColor color
)
{
    return std::any_of(
        surface.commands.begin(),
        surface.commands.end(),
        [expected, color](const DrawCommand& command)
        {
            return
                command.type == DrawCommandType::DrawText &&
                command.text == expected &&
                command.color == color;
        }
    );
}

std::size_t countCommands(
    const RecordingDrawSurface& surface,
    DrawCommandType type
)
{
    return static_cast<std::size_t>(
        std::count_if(
            surface.commands.begin(),
            surface.commands.end(),
            [type](const DrawCommand& command)
            {
                return command.type == type;
            }
        )
    );
}

bool rectInside(
    const UiRect& inner,
    const UiRect& outer
)
{
    return
        inner.x >= outer.x &&
        inner.y >= outer.y &&
        inner.x + inner.width <=
            outer.x + outer.width &&
        inner.y + inner.height <=
            outer.y + outer.height;
}

const DrawCommand* findTextCommand(
    const RecordingDrawSurface& surface,
    std::string_view expected
)
{
    const auto found = std::find_if(
        surface.commands.begin(),
        surface.commands.end(),
        [expected](const DrawCommand& command)
        {
            return
                command.type == DrawCommandType::DrawText &&
                command.text == expected;
        }
    );

    if (found == surface.commands.end())
    {
        return nullptr;
    }

    return &*found;
}

std::size_t commandIndex(
    const RecordingDrawSurface& surface,
    const DrawCommand* command
)
{
    assert(command != nullptr);

    return static_cast<std::size_t>(
        command - surface.commands.data()
    );
}

bool pointInDisplay(UiPoint value)
{
    return
        value.x >= 0 &&
        value.y >= 0 &&
        value.x < AGENDA_UI_DISPLAY_WIDTH &&
        value.y < AGENDA_UI_DISPLAY_HEIGHT;
}

bool rectInDisplay(const UiRect& value)
{
    return
        value.x >= 0 &&
        value.y >= 0 &&
        value.width >= 0 &&
        value.height >= 0 &&
        value.x + value.width <=
            AGENDA_UI_DISPLAY_WIDTH &&
        value.y + value.height <=
            AGENDA_UI_DISPLAY_HEIGHT;
}

bool commandInDisplay(const DrawCommand& command)
{
    if (command.type == DrawCommandType::Clear)
    {
        return true;
    }

    if (command.type == DrawCommandType::DrawLine)
    {
        return
            pointInDisplay(command.start) &&
            pointInDisplay(command.end);
    }

    if (
        command.type == DrawCommandType::FillCircle ||
        command.type == DrawCommandType::DrawCircle
    )
    {
        return
            command.start.x - command.radius >= 0 &&
            command.start.y - command.radius >= 0 &&
            command.start.x + command.radius <
                AGENDA_UI_DISPLAY_WIDTH &&
            command.start.y + command.radius <
                AGENDA_UI_DISPLAY_HEIGHT;
    }

    return rectInDisplay(command.bounds);
}

bool allCommandsInBounds(
    const RecordingDrawSurface& surface
)
{
    return std::all_of(
        surface.commands.begin(),
        surface.commands.end(),
        commandInDisplay
    );
}
```

The test file includes exactly the standard headers needed by this support:

```cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/calendar_engine.h"
#include "core/utf8.h"
#include "ui/agenda_renderer.h"
```

Add helpers `containsText`, `allCommandsInBounds`, and these tests:

```cpp
void test_renderer_clears_with_dark_background()
{
    AgendaUiView view;
    RecordingDrawSurface surface;
    AgendaRenderer renderer;

    renderer.render(view, surface);

    assert(!surface.commands.empty());
    assert(
        surface.commands.front().type ==
        DrawCommandType::Clear
    );
    assert(
        surface.commands.front().color ==
        AgendaUiColors::Background
    );
}

void test_renderer_does_not_change_input_view()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 9};
    view.dayPageIndex = 1;
    view.dayPageCount = 3;

    RecordingDrawSurface surface;
    AgendaRenderer renderer;

    renderer.render(view, surface);

    assert(view.screen == AgendaUiScreen::Day);
    assert(view.selectedDate.has_value());
    assert(view.selectedDate->day == 9);
    assert(view.dayPageIndex == 1);
    assert(view.dayPageCount == 3);
}
```

The test `main()` calls both functions and prints:

```cpp
std::cout << "All agenda renderer tests passed.\n";
```

- [ ] **Step 5: Compile and confirm the RED state**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp -o build\test_agenda_renderer.exe
```

Expected: compilation or linking fails because the renderer implementation
does not exist.

- [ ] **Step 6: Add the minimal base renderer**

Create `src/ui/agenda_renderer.cpp` with a full-display clear followed by a
safe fallback presentation. The public method must start exactly with:

```cpp
#include "agenda_renderer.h"

void AgendaRenderer::render(
    const AgendaUiView& view,
    DrawSurface& surface
) const
{
    surface.clear(AgendaUiColors::Background);

    surface.drawText(
        AgendaUiLayout::Header,
        view.screen == AgendaUiScreen::StartupError
            ? "ΣΦΑΛΜΑ"
            : "ΗΜΕΡΟΛΟΓΙΟ",
        UiTextStyle::Title,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );
}
```

- [ ] **Step 7: Build, run, and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
git diff --check
git add src/ui/ui_geometry.h src/ui/draw_surface.h src/ui/agenda_renderer.h src/ui/agenda_renderer.cpp tests/test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: add agenda renderer drawing contract"
```

Expected test output:

```text
All agenda renderer tests passed.
```

---

### Task 3: Render startup error and manual clock setup

**Files:**
- Modify: `src/ui/agenda_renderer.cpp`
- Modify: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Consumes: `AgendaUiScreen::StartupError`, `AgendaUiScreen::ClockSetup`, `AgendaUiView::clockDraft`, and controller error state.
- Produces: formatted Greek startup and clock screens.

- [ ] **Step 1: Add three failing tests**

Add tests that assert:

```cpp
void test_startup_error_shows_retry_action()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::StartupError;
    view.controllerError =
        AgendaControllerError::ReadFailed;

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "ΣΦΑΛΜΑ ΕΚΚΙΝΗΣΗΣ"));
    assert(containsText(surface, "ΑΠΟΤΥΧΙΑ ΑΝΑΓΝΩΣΗΣ"));
    assert(containsText(surface, "ΞΑΝΑ"));
}

void test_clock_setup_formats_date_and_time()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::ClockSetup;
    view.clockDraft = DateTime{
        Date{2026, 9, 9},
        TimeOfDay{7, 5}
    };

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "ΡΥΘΜΙΣΗ ΩΡΑΣ"));
    assert(containsText(surface, "09/09/2026"));
    assert(containsText(surface, "07:05"));
    assert(containsText(surface, "ΕΝΤΑΞΕΙ"));
}

void test_clock_setup_uses_only_in_bounds_commands()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::ClockSetup;

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(allCommandsInBounds(surface));
}
```

Add these calls before the renderer test success message in `main()`:

```cpp
test_startup_error_shows_retry_action();
test_clock_setup_formats_date_and_time();
test_clock_setup_uses_only_in_bounds_commands();
```

- [ ] **Step 2: Run and confirm failure**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
```

Expected: assertion failure for the first missing screen label.

- [ ] **Step 3: Add formatting and shared controls**

Replace the single include at the top of `agenda_renderer.cpp` with:

```cpp
#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "agenda_renderer.h"
#include "core/utf8.h"
```

In the unnamed namespace of `agenda_renderer.cpp`, add:

```cpp
std::string formatDate(const Date& value);
std::string formatTime(const TimeOfDay& value);

void drawButton(
    DrawSurface& surface,
    const UiRect& bounds,
    std::string_view label,
    UiColor fillColor,
    UiColor textColor
);

void renderStartupError(
    const AgendaUiView& view,
    DrawSurface& surface
);

void renderClockSetup(
    const AgendaUiView& view,
    DrawSurface& surface
);
```

Implement formatting with `std::ostringstream`, `std::setw(2)`, and
`std::setfill('0')`. Map controller errors to `ΑΠΟΤΥΧΙΑ ΑΝΑΓΝΩΣΗΣ`,
`ΑΠΟΤΥΧΙΑ ΑΠΟΘΗΚΕΥΣΗΣ`, `ΜΗ ΕΓΚΥΡΑ ΔΕΔΟΜΕΝΑ`, and `ΜΗ ΕΤΟΙΜΟ`.
Draw clock date and time inside
`EditorValue` and `EditorSecondary`, and draw `ΕΝΤΑΞΕΙ` in `FooterCenter`.

Dispatch both screen values explicitly from `AgendaRenderer::render()`.

- [ ] **Step 4: Rebuild, run, and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
git diff --check
git add src/ui/agenda_renderer.cpp tests/test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: render agenda startup and clock screens"
```

---

### Task 4: Render the monthly calendar

**Files:**
- Modify: `src/ui/agenda_renderer.cpp`
- Modify: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Consumes: displayed month, navigation enabled flags, and 42 `AgendaUiMonthCell` values.
- Produces: Monday-first Greek month grid with selected, today, event, adjacent-month, and disabled-navigation states.

- [ ] **Step 1: Add three failing tests**

Add this fixture and the three tests:

```cpp
AgendaUiView septemberMonthView()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Month;
    view.displayedMonth = YearMonth{2026, 9};
    view.monthPreviousEnabled = true;
    view.monthNextEnabled = true;

    const MonthGrid grid =
        buildMonthGrid(view.displayedMonth);

    for (std::size_t index = 0; index < grid.size(); ++index)
    {
        view.monthCells[index].date = grid[index].date;
        view.monthCells[index].inDisplayedMonth =
            grid[index].inDisplayedMonth;
    }

    return view;
}

void test_month_renders_title_weekdays_and_all_cells()
{
    const AgendaUiView view = septemberMonthView();
    RecordingDrawSurface surface;

    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "ΣΕΠΤΕΜΒΡΙΟΣ 2026"));
    assert(containsText(surface, "ΔΕ"));
    assert(containsText(surface, "ΤΡ"));
    assert(containsText(surface, "ΤΕ"));
    assert(containsText(surface, "ΠΕ"));
    assert(containsText(surface, "ΠΑ"));
    assert(containsText(surface, "ΣΑ"));
    assert(containsText(surface, "ΚΥ"));

    const std::size_t cellTextCount =
        static_cast<std::size_t>(
            std::count_if(
                surface.commands.begin(),
                surface.commands.end(),
                [](const DrawCommand& command)
                {
                    return
                        command.type ==
                            DrawCommandType::DrawText &&
                        rectInside(
                            command.bounds,
                            AgendaUiLayout::MonthGrid
                        );
                }
            )
        );

    assert(cellTextCount == 42);
}

void test_month_renders_selected_today_and_event_states()
{
    AgendaUiView view = septemberMonthView();
    view.monthCells[10].isSelected = true;
    view.monthCells[11].isToday = true;
    view.monthCells[12].hasEvents = true;

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    const std::size_t accentFills =
        static_cast<std::size_t>(
            std::count_if(
                surface.commands.begin(),
                surface.commands.end(),
                [](const DrawCommand& command)
                {
                    return
                        command.type ==
                            DrawCommandType::FillCircle &&
                        command.color ==
                            AgendaUiColors::Accent;
                }
            )
        );

    const bool hasTodayOutline =
        std::any_of(
            surface.commands.begin(),
            surface.commands.end(),
            [](const DrawCommand& command)
            {
                return
                    command.type ==
                        DrawCommandType::DrawCircle &&
                    command.color ==
                        AgendaUiColors::Accent;
            }
        );

    assert(accentFills >= 2);
    assert(hasTodayOutline);
}

void test_month_renders_disabled_navigation_and_adjacent_dates()
{
    AgendaUiView view = septemberMonthView();
    view.monthPreviousEnabled = false;
    view.monthNextEnabled = true;

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(
        containsTextWithColor(
            surface,
            "<",
            AgendaUiColors::Disabled
        )
    );
    assert(
        containsTextWithColor(
            surface,
            ">",
            AgendaUiColors::PrimaryText
        )
    );
    assert(
        containsTextWithColor(
            surface,
            "31",
            AgendaUiColors::Disabled
        )
    );
}
```

Add to `main()`:

```cpp
test_month_renders_title_weekdays_and_all_cells();
test_month_renders_selected_today_and_event_states();
test_month_renders_disabled_navigation_and_adjacent_dates();
```

- [ ] **Step 2: Run and confirm the month tests fail**

Use the renderer compile command from Task 3. Expected: missing
`ΣΕΠΤΕΜΒΡΙΟΣ 2026`.

- [ ] **Step 3: Implement month rendering**

Add:

```cpp
std::string greekMonthName(int month);

UiRect monthCellBounds(std::size_t index)
{
    const int column =
        static_cast<int>(index % 7);
    const int row =
        static_cast<int>(index / 7);

    return UiRect{
        AgendaUiLayout::MonthGrid.x +
            column * AgendaUiLayout::MonthCellWidth,
        AgendaUiLayout::MonthGrid.y +
            row * AgendaUiLayout::MonthCellHeight,
        AgendaUiLayout::MonthCellWidth,
        AgendaUiLayout::MonthCellHeight
    };
}

void renderMonth(
    const AgendaUiView& view,
    DrawSurface& surface
);
```

Use a fixed 12-entry Greek month-name array. Render all 42 indexes; skip
only cells whose `date` is empty. Apply state priority in this order:

1. adjacent-month text is disabled;
2. today adds an accent outline;
3. selected adds an accent fill and background-colored number;
4. event marker is drawn last and remains visible.

Dispatch `AgendaUiScreen::Month` to `renderMonth()`.

- [ ] **Step 4: Rebuild, run, and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
git diff --check
git add src/ui/agenda_renderer.cpp tests/test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: render agenda month screen"
```

---

### Task 5: Render day browsing and event details

**Files:**
- Modify: `src/ui/agenda_renderer.cpp`
- Modify: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Consumes: selected date, up to three visible day rows, page state, and optional selected event.
- Produces: day list, paging state, event details, and missing-event safety.

- [ ] **Step 1: Add three failing tests**

```cpp
void test_day_renders_three_rows_and_page_state()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 9};
    view.dayPageIndex = 1;
    view.dayPageCount = 3;
    view.dayPreviousEnabled = true;
    view.dayNextEnabled = true;
    view.dayEvents = {
        AgendaUiEventRow{
            1,
            TimeOfDay{9, 5},
            "Οδοντίατρος"
        },
        AgendaUiEventRow{
            2,
            TimeOfDay{12, 30},
            "Συνάντηση"
        },
        AgendaUiEventRow{
            3,
            TimeOfDay{18, 0},
            "Προπόνηση"
        }
    };

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "09/09/2026"));
    assert(containsText(surface, "2/3"));
    assert(containsText(surface, "09:05"));
    assert(containsText(surface, "Οδοντίατρος"));
    assert(containsText(surface, "Συνάντηση"));
    assert(containsText(surface, "Προπόνηση"));
}

void test_day_uses_all_day_label_and_utf8_safe_preview()
{
    const std::string longText =
        "Αυτό είναι ένα πολύ μεγάλο ελληνικό γεγονός "
        "που πρέπει να κοπεί με ασφάλεια";

    AgendaUiView view;
    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 9};
    view.dayEvents = {
        AgendaUiEventRow{7, std::nullopt, longText}
    };

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    const std::string expected =
        utf8Prefix(longText, 32);

    assert(containsText(surface, "ΟΛΗ ΜΕΡΑ"));
    assert(containsText(surface, expected));
    assert(isValidUtf8(expected));
}

void test_event_details_render_event_or_safe_error()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EventDetails;
    view.selectedEvent = Event{
        7,
        Date{2026, 9, 10},
        TimeOfDay{8, 5},
        "Ιατρικό ραντεβού",
        false,
        DateTime{
            Date{2026, 9, 9},
            TimeOfDay{10, 0}
        }
    };

    RecordingDrawSurface eventSurface;
    AgendaRenderer{}.render(view, eventSurface);

    assert(containsText(eventSurface, "10/09/2026"));
    assert(containsText(eventSurface, "08:05"));
    assert(containsText(eventSurface, "Ιατρικό ραντεβού"));
    assert(containsText(eventSurface, "ΠΙΣΩ"));
    assert(containsText(eventSurface, "ΑΛΛΑΓΗ"));
    assert(containsText(eventSurface, "ΔΙΑΓΡΑΦΗ"));

    view.selectedEvent.reset();

    RecordingDrawSurface missingSurface;
    AgendaRenderer{}.render(view, missingSurface);

    assert(
        containsText(
            missingSurface,
            "ΤΟ ΓΕΓΟΝΟΣ ΔΕΝ ΒΡΕΘΗΚΕ"
        )
    );
}
```

Add to `main()`:

```cpp
test_day_renders_three_rows_and_page_state();
test_day_uses_all_day_label_and_utf8_safe_preview();
test_event_details_render_event_or_safe_error();
```

- [ ] **Step 2: Run and confirm failure**

Compile with the Task 4 command. Expected: day date or detail label missing.

- [ ] **Step 3: Implement day and detail helpers**

Add:

```cpp
const UiRect& dayRowBounds(std::size_t index);

void renderDay(
    const AgendaUiView& view,
    DrawSurface& surface
);

void renderEventDetails(
    const AgendaUiView& view,
    DrawSurface& surface
);
```

`renderDay()` draws at most `AGENDA_UI_EVENTS_PER_PAGE` rows even if the
vector is larger. Use `utf8Prefix(row.text, 32)`. Display page numbers as
human-facing one-based values. When `dayPageCount` is zero, omit the page
fraction. Use disabled colors for unavailable page arrows.

`renderEventDetails()` checks `selectedEvent` before accessing it. Show
`ΟΛΗ ΜΕΡΑ` when `eventTime` is empty. Use accent for edit and error red for
delete.

Dispatch `Day` and `EventDetails` explicitly.

- [ ] **Step 4: Rebuild, run, and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
git diff --check
git add src/ui/agenda_renderer.cpp tests/test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: render agenda day and event details"
```

---

### Task 6: Render date, time, and review editor stages

**Files:**
- Modify: `src/ui/agenda_renderer.cpp`
- Modify: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Consumes: `AgendaUiEditorDraft` create/update mode, date, optional time, text, and dirty state.
- Produces: editor date, editor time, and editor review screens.

- [ ] **Step 1: Add three failing tests**

```cpp
void test_editor_date_renders_draft_and_navigation()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorDate;
    view.editor.date = Date{2026, 9, 10};

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "ΗΜΕΡΟΜΗΝΙΑ"));
    assert(containsText(surface, "10/09/2026"));
    assert(containsText(surface, "ΠΙΣΩ"));
    assert(containsText(surface, "ΕΠΟΜΕΝΟ"));
    assert(containsText(surface, "ΑΚΥΡΩΣΗ"));
}

void test_editor_time_renders_timed_and_all_day_variants()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = TimeOfDay{8, 5};

    RecordingDrawSurface timedSurface;
    AgendaRenderer{}.render(view, timedSurface);

    assert(containsText(timedSurface, "ΩΡΑ"));
    assert(containsText(timedSurface, "ΜΕ ΩΡΑ"));
    assert(containsText(timedSurface, "08:05"));

    view.editor.time.reset();

    RecordingDrawSurface allDaySurface;
    AgendaRenderer{}.render(view, allDaySurface);

    assert(containsText(allDaySurface, "ΟΛΗ ΜΕΡΑ"));
    assert(!containsText(allDaySurface, "08:05"));
}

void test_editor_review_renders_create_and_update_variants()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorReview;
    view.editor.date = Date{2026, 9, 10};
    view.editor.time = TimeOfDay{8, 5};
    view.editor.text = "Ραντεβού";
    view.editor.mode = AgendaUiEditorMode::Create;

    RecordingDrawSurface createSurface;
    AgendaRenderer{}.render(view, createSurface);

    assert(containsText(createSurface, "ΝΕΟ ΓΕΓΟΝΟΣ"));
    assert(containsText(createSurface, "10/09/2026"));
    assert(containsText(createSurface, "08:05"));
    assert(containsText(createSurface, "Ραντεβού"));
    assert(containsText(createSurface, "ΑΠΟΘΗΚΕΥΣΗ"));

    view.editor.mode = AgendaUiEditorMode::Update;
    view.editor.eventId = 7;

    RecordingDrawSurface updateSurface;
    AgendaRenderer{}.render(view, updateSurface);

    assert(
        containsText(
            updateSurface,
            "ΑΛΛΑΓΗ ΓΕΓΟΝΟΤΟΣ"
        )
    );
    assert(containsText(updateSurface, "ΑΠΟΘΗΚΕΥΣΗ"));
}
```

Add to `main()`:

```cpp
test_editor_date_renders_draft_and_navigation();
test_editor_time_renders_timed_and_all_day_variants();
test_editor_review_renders_create_and_update_variants();
```

- [ ] **Step 2: Run and confirm failure**

Compile and run the renderer test. Expected: `ΗΜΕΡΟΜΗΝΙΑ` is missing.

- [ ] **Step 3: Implement the three editor renderers**

Add:

```cpp
void renderEditorDate(
    const AgendaUiView& view,
    DrawSurface& surface
);

void renderEditorTime(
    const AgendaUiView& view,
    DrawSurface& surface
);

void renderEditorReview(
    const AgendaUiView& view,
    DrawSurface& surface
);
```

All three screens use `view.editor`, never the selected event. The review
text uses `utf8Prefix(view.editor.text, 64)`. Dispatch the three matching
screen enum values.

- [ ] **Step 4: Rebuild, run, and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
git diff --check
git add src/ui/agenda_renderer.cpp tests/test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: render agenda editor stages"
```

---

### Task 7: Render the T9 text editor

**Files:**
- Modify: `src/ui/agenda_renderer.cpp`
- Modify: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Consumes: editor text, `T9Language`, `T9Mode`, and uppercase state.
- Produces: text preview, status line, and 12-key T9 grid.

- [ ] **Step 1: Add three failing tests**

```cpp
void test_t9_editor_renders_greek_letters_and_twelve_keys()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;
    view.t9Language = T9Language::Greek;
    view.t9Mode = T9Mode::Letters;
    view.t9Uppercase = false;

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "ΕΛ"));
    assert(containsText(surface, "ΓΡΑΜΜΑΤΑ"));
    assert(containsText(surface, "πεζά"));
    assert(containsText(surface, "2 ΑΒΓ"));
    assert(containsText(surface, "7 ΠΡΣ"));
    assert(containsText(surface, "9 ΧΨΩ"));

    const std::size_t keyCount =
        static_cast<std::size_t>(
            std::count_if(
                surface.commands.begin(),
                surface.commands.end(),
                [](const DrawCommand& command)
                {
                    return
                        command.type ==
                            DrawCommandType::FillRoundedRectangle &&
                        command.bounds.x >=
                            AgendaUiLayout::T9GridX &&
                        command.bounds.y >=
                            AgendaUiLayout::T9GridY &&
                        command.bounds.y < 190;
                }
            )
        );

    assert(keyCount == 12);
}

void test_t9_editor_renders_english_numbers_and_case_state()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;
    view.t9Language = T9Language::English;
    view.t9Mode = T9Mode::Numbers;
    view.t9Uppercase = true;

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "EN"));
    assert(containsText(surface, "ΑΡΙΘΜΟΙ"));
    assert(containsText(surface, "ΚΕΦΑΛΑΙΑ"));
    assert(containsText(surface, "2"));
    assert(containsText(surface, "9"));
    assert(containsText(surface, "0"));
}

void test_t9_editor_preview_is_utf8_safe()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;
    view.editor.text =
        "Αυτό το κείμενο είναι αρκετά μεγάλο ώστε "
        "να χρειάζεται ασφαλή περικοπή χαρακτήρων";

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    const std::string expected =
        utf8Prefix(view.editor.text, 48);

    const DrawCommand* preview =
        findTextCommand(surface, expected);

    assert(preview != nullptr);
    assert(isValidUtf8(preview->text));
    assert(
        utf8CodePointCount(preview->text) <= 48
    );
}
```

Add to `main()`:

```cpp
test_t9_editor_renders_greek_letters_and_twelve_keys();
test_t9_editor_renders_english_numbers_and_case_state();
test_t9_editor_preview_is_utf8_safe();
```

- [ ] **Step 2: Run and confirm failure**

Compile and run. Expected: the first Greek T9 label is missing.

- [ ] **Step 3: Implement T9 rendering**

Add:

```cpp
UiRect t9KeyBounds(std::size_t index);

std::array<std::string, 12> t9Labels(
    T9Language language,
    T9Mode mode
);

void renderEditorText(
    const AgendaUiView& view,
    DrawSurface& surface
);
```

Use these Greek letter labels:

```cpp
{
    "1", "2 ΑΒΓ", "3 ΔΕΖ",
    "4 ΗΘΙ", "5 ΚΛΜ", "6 ΝΞΟ",
    "7 ΠΡΣ", "8 ΤΥΦ", "9 ΧΨΩ",
    "EL/EN", "0", "<"
}
```

Use standard Latin groups for English and plain digits in numbers mode.
Render the preview with `utf8Prefix(text, 48)`. Dispatch `EditorText`.

- [ ] **Step 4: Rebuild, run, and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
git diff --check
git add src/ui/agenda_renderer.cpp tests/test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: render agenda T9 editor"
```

---

### Task 8: Render overlays and harden invalid-state behavior

**Files:**
- Modify: `src/ui/agenda_renderer.cpp`
- Modify: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Consumes: all `AgendaUiOverlay` values, reminder event, controller error, event error, and clock error.
- Produces: overlay-last rendering, Greek confirmations, mapped error messages, and safe fallbacks.

- [ ] **Step 1: Add three failing overlay tests**

```cpp
void test_reminder_overlay_is_drawn_after_base_screen()
{
    AgendaUiView view = septemberMonthView();
    view.overlay = AgendaUiOverlay::Reminder;
    view.reminder = Event{
        7,
        Date{2026, 9, 10},
        TimeOfDay{8, 5},
        "Ιατρικό ραντεβού",
        false,
        DateTime{
            Date{2026, 9, 9},
            TimeOfDay{10, 0}
        }
    };

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    const auto scrim = std::find_if(
        surface.commands.begin(),
        surface.commands.end(),
        [](const DrawCommand& command)
        {
            return
                command.type ==
                    DrawCommandType::FillRectangle &&
                command.bounds ==
                    AgendaUiLayout::FullDisplay &&
                command.color ==
                    AgendaUiColors::Overlay;
        }
    );

    assert(scrim != surface.commands.end());

    const std::size_t scrimIndex =
        static_cast<std::size_t>(
            scrim - surface.commands.begin()
        );

    const DrawCommand* title =
        findTextCommand(surface, "ΥΠΕΝΘΥΜΙΣΗ");
    const DrawCommand* text =
        findTextCommand(surface, "Ιατρικό ραντεβού");
    const DrawCommand* confirm =
        findTextCommand(surface, "ΕΝΤΑΞΕΙ");

    assert(commandIndex(surface, title) > scrimIndex);
    assert(commandIndex(surface, text) > scrimIndex);
    assert(commandIndex(surface, confirm) > scrimIndex);
}

void test_confirmation_overlays_render_both_choices()
{
    AgendaUiView view = septemberMonthView();
    view.overlay = AgendaUiOverlay::ConfirmDelete;

    RecordingDrawSurface deleteSurface;
    AgendaRenderer{}.render(view, deleteSurface);

    assert(containsText(deleteSurface, "ΔΙΑΓΡΑΦΗ;"));
    assert(containsText(deleteSurface, "ΝΑΙ"));
    assert(containsText(deleteSurface, "ΟΧΙ"));

    view.overlay = AgendaUiOverlay::ConfirmDiscard;

    RecordingDrawSurface discardSurface;
    AgendaRenderer{}.render(view, discardSurface);

    assert(
        containsText(
            discardSurface,
            "ΑΠΟΡΡΙΨΗ ΑΛΛΑΓΩΝ;"
        )
    );
    assert(containsText(discardSurface, "ΝΑΙ"));
    assert(containsText(discardSurface, "ΟΧΙ"));
}

void test_operation_and_clock_errors_render_safe_messages()
{
    AgendaUiView view = septemberMonthView();
    view.overlay = AgendaUiOverlay::OperationError;
    view.controllerError =
        AgendaControllerError::WriteFailed;

    RecordingDrawSurface operationSurface;
    AgendaRenderer{}.render(view, operationSurface);

    assert(
        containsText(
            operationSurface,
            "ΑΠΟΤΥΧΙΑ ΑΠΟΘΗΚΕΥΣΗΣ"
        )
    );
    assert(containsText(operationSurface, "ΞΑΝΑ"));
    assert(containsText(operationSurface, "ΑΚΥΡΩΣΗ"));

    view.controllerError = AgendaControllerError::None;
    view.eventError = EventError::EmptyText;

    RecordingDrawSurface validationSurface;
    AgendaRenderer{}.render(view, validationSurface);

    assert(containsText(validationSurface, "ΚΕΝΟ ΚΕΙΜΕΝΟ"));

    view.overlay = AgendaUiOverlay::ClockError;
    view.eventError = EventError::None;
    view.clockError = ClockError::InvalidDateTime;

    RecordingDrawSurface clockSurface;
    AgendaRenderer{}.render(view, clockSurface);

    assert(containsText(clockSurface, "ΜΗ ΕΓΚΥΡΗ ΩΡΑ"));
}
```

Add to `main()`:

```cpp
test_reminder_overlay_is_drawn_after_base_screen();
test_confirmation_overlays_render_both_choices();
test_operation_and_clock_errors_render_safe_messages();
```

- [ ] **Step 2: Run and confirm overlay failure**

Compile and run. Expected: missing reminder or overlay scrim assertion.

- [ ] **Step 3: Implement overlay dispatch**

Add:

```cpp
void drawOverlayCard(
    DrawSurface& surface,
    std::string_view title
);

std::string controllerErrorText(
    AgendaControllerError error
);

std::string eventErrorText(
    EventError error
);

std::string clockErrorText(
    ClockError error
);

void renderOverlay(
    const AgendaUiView& view,
    DrawSurface& surface
);
```

`drawOverlayCard()` first fills `FullDisplay` with `Overlay`, then fills and
outlines `OverlayCard`. `renderOverlay()` returns immediately for `None`.
For every other enum, it draws the matching card after base-screen dispatch.
If a reminder event is absent, show `ΥΠΕΝΘΥΜΙΣΗ ΜΗ ΔΙΑΘΕΣΙΜΗ` instead of
accessing the optional.

Map every current enum value explicitly and use `ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ` in each
switch default.

- [ ] **Step 4: Add three final hardening tests**

```cpp
void test_unknown_screen_renders_safe_error()
{
    AgendaUiView view;
    view.screen =
        static_cast<AgendaUiScreen>(999);

    RecordingDrawSurface surface;
    AgendaRenderer{}.render(view, surface);

    assert(containsText(surface, "ΣΦΑΛΜΑ ΠΡΟΒΟΛΗΣ"));
    assert(allCommandsInBounds(surface));
}

void test_missing_required_values_render_safe_errors()
{
    AgendaUiView dayView;
    dayView.screen = AgendaUiScreen::Day;

    RecordingDrawSurface daySurface;
    AgendaRenderer{}.render(dayView, daySurface);

    assert(
        containsText(
            daySurface,
            "ΗΜΕΡΟΜΗΝΙΑ ΜΗ ΔΙΑΘΕΣΙΜΗ"
        )
    );

    AgendaUiView reminderView =
        septemberMonthView();
    reminderView.overlay = AgendaUiOverlay::Reminder;
    reminderView.reminder.reset();

    RecordingDrawSurface reminderSurface;
    AgendaRenderer{}.render(
        reminderView,
        reminderSurface
    );

    assert(
        containsText(
            reminderSurface,
            "ΥΠΕΝΘΥΜΙΣΗ ΜΗ ΔΙΑΘΕΣΙΜΗ"
        )
    );
}

void test_every_supported_screen_stays_in_bounds()
{
    AgendaUiView view = septemberMonthView();
    view.clockDraft = DateTime{
        Date{2026, 9, 9},
        TimeOfDay{10, 0}
    };
    view.selectedDate = Date{2026, 9, 9};
    view.selectedEvent = Event{
        7,
        Date{2026, 9, 10},
        std::nullopt,
        "Γεγονός",
        false,
        DateTime{
            Date{2026, 9, 9},
            TimeOfDay{10, 0}
        }
    };
    view.editor.date = Date{2026, 9, 10};
    view.editor.time = TimeOfDay{8, 5};
    view.editor.text = "Γεγονός";

    const std::array<AgendaUiScreen, 9> screens{
        AgendaUiScreen::ClockSetup,
        AgendaUiScreen::Month,
        AgendaUiScreen::Day,
        AgendaUiScreen::EventDetails,
        AgendaUiScreen::EditorDate,
        AgendaUiScreen::EditorTime,
        AgendaUiScreen::EditorText,
        AgendaUiScreen::EditorReview,
        AgendaUiScreen::StartupError
    };

    for (AgendaUiScreen screen : screens)
    {
        view.screen = screen;

        RecordingDrawSurface surface;
        AgendaRenderer{}.render(view, surface);

        assert(allCommandsInBounds(surface));
    }
}
```

Add to `main()`:

```cpp
test_unknown_screen_renders_safe_error();
test_missing_required_values_render_safe_errors();
test_every_supported_screen_stays_in_bounds();
```

- [ ] **Step 5: Implement fallback dispatch and pass all renderer tests**

Use an exhaustive `switch (view.screen)` in `render()`. Its `default`
calls a safe renderer that shows `ΣΦΑΛΜΑ ΠΡΟΒΟΛΗΣ`. Always call
`renderOverlay(view, surface)` after this switch.

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
.\build\test_agenda_renderer.exe
```

Expected:

```text
All agenda renderer tests passed.
```

- [ ] **Step 6: Check and commit**

```powershell
git diff --check
git add src/ui/agenda_renderer.cpp tests/test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: render agenda overlays and safe fallbacks"
```

---

### Task 9: Rebuild affected suites and verify the complete project

**Files:**
- Modify only if verification exposes a renderer regression.

**Interfaces:**
- Consumes: the completed renderer and all existing project modules.
- Produces: warning-free binaries, complete regression evidence, and a clean Git state.

- [ ] **Step 1: Rebuild directly affected suites**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_utf8.cpp src\core\utf8.cpp -o build\test_utf8.exe

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_event.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp -o build\test_event.exe

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_t9_engine.cpp src\core\utf8.cpp src\core\t9_engine.cpp -o build\test_t9_engine.exe

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
```

Expected: all five commands exit with code 0 and print no warnings.

- [ ] **Step 2: Run all available test binaries**

```powershell
$tests = @(
    "test_date_time",
    "test_calendar_engine",
    "test_utf8",
    "test_event",
    "test_t9_engine",
    "test_agenda_manager",
    "test_agenda_state",
    "test_crc32",
    "test_event_codec",
    "test_desktop_file_storage",
    "test_agenda_repository",
    "test_agenda_controller",
    "test_reminder_engine",
    "test_agenda_persistence",
    "test_agenda_ui_state_machine",
    "test_agenda_ui_persistence",
    "test_agenda_renderer"
)

foreach ($test in $tests)
{
    & ".\build\$test.exe"

    if ($LASTEXITCODE -ne 0)
    {
        throw "Test failed: $test"
    }
}
```

Expected: all 17 suites print their success messages and the loop throws no
exception.

- [ ] **Step 3: Verify architecture and Git cleanliness**

```powershell
Select-String `
    -Path src\core\*.h,src\core\*.cpp,src\ui\*.h,src\ui\*.cpp `
    -Pattern "Arduino|ESP32|WiFi|NTP|LVGL|GC9A01|CST816"

git diff --check
git status --short
git log --oneline -10
```

Expected:

- the architecture search prints no matches;
- `git diff --check` prints nothing;
- `git status --short` prints nothing;
- the log ends with the eight focused implementation commits from this plan.

- [ ] **Step 4: Create a fresh backup bundle**

```powershell
git bundle create C:\Users\gnaki\Desktop\digital_diary-renderer-complete.bundle --all
git bundle verify C:\Users\gnaki\Desktop\digital_diary-renderer-complete.bundle
```

Expected: Git reports a complete history and says the bundle is okay.

## Execution Handoff

The user has already selected inline execution for this project stage.
Execute this plan with `superpowers:executing-plans`, in order, with a
checkpoint after each test batch and commit. Do not use worktrees or
subagents unless the user explicitly changes that instruction.
