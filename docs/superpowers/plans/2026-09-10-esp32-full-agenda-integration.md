# ESP32 Full Agenda Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task in the established inline workflow. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the tested agenda core, renderer, touch controller, software clock, and LittleFS persistence into a complete offline application on the Waveshare ESP32-S3-Touch-LCD-1.28 Rev3.

**Architecture:** `src/platform/esp32/main.cpp` remains the composition root. Platform-neutral UI code owns shared geometry and maps release points to actions; `AgendaUiStateMachine` remains the only state mutator. Thin ESP32 adapters implement `DrawSurface`, `Clock`, and `ByteStorage`. The loop samples touch without blocking, dispatches at most one action per release, checks reminders once per second, and redraws only after visible state changes.

**Tech Stack:** C++17, assert-based desktop tests, PlatformIO Core 6.2.0, Arduino framework for ESP32-S3, pioarduino platform 55.03.39, Arduino_GFX 1.6.7, U8g2 2.36.18, ESP32 LittleFS, GC9A01A display, CST816S touch controller.

**Spec:** `docs/superpowers/specs/2026-09-10-esp32-full-agenda-integration-design.md`

## Global Constraints

- Start from clean commit `40a773d` (`docs: align agenda touch controls with renderer`).
- Work inline on `main`, as already selected; do not create worktrees or dispatch subagents.
- Keep Arduino, ESP32, filesystem, and graphics-library types out of `src/core` and `src/ui`.
- Do not change any existing core interface. If an unavoidable core-interface change is discovered, stop and design it separately.
- `src/ui/ui_geometry.h` is the only source of interactive bounds. The renderer and mapper may not duplicate coordinates.
- Use visible controls only. Do not introduce gestures, long presses, hidden hit regions, or selection modes.
- Interpret rectangles as half-open ranges: `x <= point.x < x + width` and `y <= point.y < y + height`.
- Dispatch at most one action on release. Cancel a press if its displacement from the initial point exceeds 12 pixels.
- Preserve the existing Greek strings and UTF-8 data unchanged. The embedded surface must respect the exact `std::string_view` length.
- Require clock setup after every boot. Never restore Flash data as authoritative wall-clock time.
- Never call `LittleFS.begin(true)` and never erase or format Flash automatically.
- Keep `default_16MB.csv`; select LittleFS as the PlatformIO filesystem.
- Do not add sleep, Wi-Fi, Bluetooth, NTP, IMU, or battery behavior.
- Do not add production-code comments.
- Run `git diff --check` before every commit.
- Stop for explicit user approval immediately before any PlatformIO upload. Building does not require upload approval.

## File Map

### New platform-neutral UI and tests

- Create `src/ui/agenda_touch_mapper.h` for the release-point mapping API.
- Create `src/ui/agenda_touch_mapper.cpp` for overlay-first, screen-specific action mapping.
- Create `tests/test_agenda_touch_mapper.cpp` for every visible/disabled/empty/boundary mapping case.
- Create `src/ui/draw_surface_math.h` for portable RGB565 and text-alignment helpers.
- Create `tests/test_draw_surface_math.cpp` for exact color conversion and cursor calculations.

### New ESP32 adapters

- Create `src/platform/esp32/arduino_gfx_surface.h` and `.cpp` for `DrawSurface` over `Arduino_GFX` with the Greek U8g2 font.
- Create `src/platform/esp32/esp32_clock.h` and `.cpp` for the boot-invalid monotonic clock.
- Create `tests/test_esp32_clock.cpp` for the adapter's platform-independent time arithmetic.
- Create `src/platform/esp32/littlefs_byte_storage.h` and `.cpp` for mounted-volume state and atomic file replacement.

### Modified files

- Modify `src/ui/ui_geometry.h` to own every interactive rectangle and repeated geometry helper.
- Modify `src/ui/agenda_renderer.cpp` to use shared geometry and render all missing controls.
- Modify `tests/test_agenda_renderer.cpp` to prove those controls are visible at their shared bounds.
- Modify `src/platform/esp32/display_hardware.h` so U8g2 is included before Arduino_GFX.
- Modify `src/platform/esp32/main.cpp` to replace the touch smoke test with the application composition root and runtime loop.
- Modify `platformio.ini` to link root/core/UI/ESP32 sources, pin U8g2, and select LittleFS.

---

### Task 1: Make interactive geometry shared and every action visible

**Files:**
- Modify: `src/ui/ui_geometry.h`
- Modify: `src/ui/agenda_renderer.cpp`
- Modify: `tests/test_agenda_renderer.cpp`

**Interfaces:**
- Produces: named `UiRect` constants for every visible control.
- Produces: `uiRectContains`, `monthCellBounds`, `dayRowBounds`, and `t9KeyBounds` as platform-neutral helpers.
- Preserves: `AgendaRenderer::render(const AgendaUiView&, DrawSurface&) const`.

- [ ] **Step 1: Add renderer-test helpers and failing visibility tests**

Add a helper that requires both a rounded button background and its label at the same shared bounds:

```cpp
bool hasButton(
    const RecordingDrawSurface& surface,
    const UiRect& bounds,
    std::string_view label
)
{
    const bool hasBackground =
        std::any_of(
            surface.commands.begin(),
            surface.commands.end(),
            [&](const DrawCommand& command)
            {
                return
                    command.type ==
                        DrawCommandType::FillRoundedRectangle &&
                    command.bounds == bounds;
            }
        );

    const bool hasLabel =
        std::any_of(
            surface.commands.begin(),
            surface.commands.end(),
            [&](const DrawCommand& command)
            {
                return
                    command.type == DrawCommandType::DrawText &&
                    command.bounds == bounds &&
                    command.text == label;
            }
        );

    return hasBackground && hasLabel;
}
```

Add and call these tests before the existing bounds sweep:

```cpp
void test_clock_setup_renders_all_adjustment_buttons();
void test_day_renders_visible_back_and_create_buttons();
void test_editor_date_renders_visible_day_buttons();
void test_editor_time_renders_mode_and_time_buttons();
void test_t9_status_and_special_keys_are_visibly_actionable();
void test_editor_review_renders_back_cancel_and_save();
void test_overlays_use_shared_button_bounds();
```

The assertions must require:

| Screen | Bounds | Label |
|---|---|---|
| ClockSetup | `ClockDatePrevious`, `ClockDateNext` | `<`, `>` |
| ClockSetup | `ClockHourUp`, `ClockHourDown`, `ClockMinuteUp`, `ClockMinuteDown` | `+`, `-`, `+`, `-` |
| ClockSetup | `FooterCenter` | `ΕΝΤΑΞΕΙ` |
| Day | `DayBack`, `DayCreate` | `<`, `+` |
| EditorDate | `EditorDatePrevious`, `EditorDateNext` | `<`, `>` |
| EditorTime | `EditorTimedMode`, `EditorAllDayMode` | `ΜΕ ΩΡΑ`, `ΟΛΗ ΜΕΡΑ` |
| EditorTime, timed | `EditorHourUp`, `EditorHourDown`, `EditorMinuteUp`, `EditorMinuteDown` | `+`, `-`, `+`, `-` |
| EditorText | `T9Language`, `T9Mode`, `T9Case` | current language, mode, and case strings |
| EditorText | `t9KeyBounds(9)`, `t9KeyBounds(11)` | `* <`, and `# Α/α` or `# A/a` according to language |
| EditorText | `FooterLeft`, `FooterCenter`, `FooterRight` | `ΠΙΣΩ`, `ΑΚΥΡΩΣΗ`, `ΕΠΟΜΕΝΟ` |
| EditorReview | `FooterLeft`, `FooterCenter`, `FooterRight` | `ΠΙΣΩ`, `ΑΚΥΡΩΣΗ`, `ΑΠΟΘΗΚΕΥΣΗ` |
| Reminder | `OverlayAcknowledge` | `ΕΝΤΑΞΕΙ` |
| ConfirmDelete/ConfirmDiscard | `OverlayConfirm`, `OverlayCancel` | `ΝΑΙ`, `ΟΧΙ` |
| OperationError | `OverlayRetry`, `OverlayOperationCancel` | `ΞΑΝΑ`, `ΑΚΥΡΩΣΗ` |
| ClockError | `OverlayClockOk` | `ΕΝΤΑΞΕΙ` |

- [ ] **Step 2: Compile and confirm RED**

```powershell
New-Item -ItemType Directory -Force build | Out-Null
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
```

Expected: compilation fails because the new named geometry does not exist. If compilation succeeds, the new tests must fail because the controls are not yet rendered.

- [ ] **Step 3: Add the shared geometry contract**

Add `<cstddef>` and these exact members inside `AgendaUiLayout`:

```cpp
constexpr UiRect MonthPrevious{20, 8, 28, 28};
constexpr UiRect MonthNext{192, 8, 28, 28};

constexpr UiRect DayBack{20, 8, 28, 28};
constexpr UiRect DayHeader{52, 8, 136, 28};
constexpr UiRect DayCreate{192, 8, 28, 28};

constexpr UiRect ClockDatePrevious{20, 67, 28, 34};
constexpr UiRect ClockDateValue{58, 58, 124, 52};
constexpr UiRect ClockDateNext{192, 67, 28, 34};
constexpr UiRect ClockHourUp{42, 116, 56, 18};
constexpr UiRect ClockHourValue{42, 136, 56, 22};
constexpr UiRect ClockHourDown{42, 158, 56, 18};
constexpr UiRect ClockMinuteUp{142, 116, 56, 18};
constexpr UiRect ClockMinuteValue{142, 136, 56, 22};
constexpr UiRect ClockMinuteDown{142, 158, 56, 18};

constexpr UiRect EditorDatePrevious{20, 67, 28, 34};
constexpr UiRect EditorDateValue{58, 58, 124, 52};
constexpr UiRect EditorDateNext{192, 67, 28, 34};

constexpr UiRect EditorTimedMode{28, 58, 88, 32};
constexpr UiRect EditorAllDayMode{124, 58, 88, 32};
constexpr UiRect EditorHourUp{42, 110, 56, 18};
constexpr UiRect EditorHourValue{42, 130, 56, 26};
constexpr UiRect EditorHourDown{42, 158, 56, 18};
constexpr UiRect EditorMinuteUp{142, 110, 56, 18};
constexpr UiRect EditorMinuteValue{142, 130, 56, 26};
constexpr UiRect EditorMinuteDown{142, 158, 56, 18};

constexpr UiRect T9Language{38, 76, 44, 16};
constexpr UiRect T9Mode{84, 76, 72, 16};
constexpr UiRect T9Case{158, 76, 44, 16};

constexpr UiRect OverlayAcknowledge{82, 154, 76, 28};
constexpr UiRect OverlayConfirm{48, 144, 60, 30};
constexpr UiRect OverlayCancel{132, 144, 60, 30};
constexpr UiRect OverlayRetry{44, 148, 70, 30};
constexpr UiRect OverlayOperationCancel{126, 148, 70, 30};
constexpr UiRect OverlayClockOk{82, 150, 76, 30};

constexpr bool uiRectContains(
    const UiRect& bounds,
    UiPoint point
)
{
    return
        point.x >= bounds.x &&
        point.y >= bounds.y &&
        point.x < bounds.x + bounds.width &&
        point.y < bounds.y + bounds.height;
}

constexpr UiRect monthCellBounds(std::size_t index)
{
    return UiRect{
        MonthGrid.x +
            static_cast<int>(index % 7) * MonthCellWidth,
        MonthGrid.y +
            static_cast<int>(index / 7) * MonthCellHeight,
        MonthCellWidth,
        MonthCellHeight
    };
}

constexpr UiRect dayRowBounds(std::size_t index)
{
    return UiRect{
        DayRow0.x,
        DayRow0.y + static_cast<int>(index) * 42,
        DayRow0.width,
        DayRow0.height
    };
}

constexpr UiRect t9KeyBounds(std::size_t index)
{
    return UiRect{
        T9GridX +
            static_cast<int>(index % 3) *
                (T9KeyWidth + T9ColumnGap),
        T9GridY +
            static_cast<int>(index / 3) *
                (T9KeyHeight + T9RowGap),
        T9KeyWidth,
        T9KeyHeight
    };
}
```

- [ ] **Step 4: Make the renderer consume only shared interactive geometry**

Delete private `MONTH_PREVIOUS`, `MONTH_NEXT`, `monthCellBounds`, `dayRowBounds`, and `t9KeyBounds`. Qualify every use with `AgendaUiLayout::`.

Render each table entry from Step 1 with `drawButton`. Keep disabled month/day arrows visible with `AgendaUiColors::Disabled`. For the T9 status controls, use radius `4`, the current status string, and a surface fill; change key label 9 to `* <` and key label 11 to `# Α/α` in Greek or `# A/a` in English. Use `DayHeader` for the date so the new header buttons cannot overlap it. The two mode buttons must always render; use `Accent` for the selected mode and `Surface` for the other. Only render the four editor time adjustment buttons when `view.editor.time.has_value()`.

- [ ] **Step 5: Run renderer tests and the full bounds sweep**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
if ($LASTEXITCODE -ne 0) { throw 'Renderer compile failed.' }
.\build\test_agenda_renderer.exe
if ($LASTEXITCODE -ne 0) { throw 'Renderer tests failed.' }
```

Expected: `All agenda renderer tests passed.`

- [ ] **Step 6: Check and commit**

```powershell
git diff --check
git add src\ui\ui_geometry.h src\ui\agenda_renderer.cpp tests\test_agenda_renderer.cpp
git diff --cached --check
git commit -m "feat: expose visible agenda touch controls"
```

---

### Task 2: Map release points to exactly one agenda action

**Files:**
- Create: `src/ui/agenda_touch_mapper.h`
- Create: `src/ui/agenda_touch_mapper.cpp`
- Create: `tests/test_agenda_touch_mapper.cpp`

**Interfaces:**

```cpp
class AgendaTouchMapper
{
public:
    std::optional<AgendaUiAction> map(
        const AgendaUiView& view,
        UiPoint releasePoint,
        std::uint32_t timestampMilliseconds
    ) const;
};
```

- [ ] **Step 1: Create failing mapper tests**

Use these helpers:

```cpp
UiPoint centerOf(const UiRect& bounds)
{
    return UiPoint{
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height / 2
    };
}

AgendaUiAction requireAction(
    const std::optional<AgendaUiAction>& action,
    AgendaUiActionType expected
)
{
    assert(action.has_value());
    assert(action->type == expected);
    return *action;
}
```

Add and call these tests:

```cpp
void test_overlay_actions_take_priority_over_screen_actions();
void test_startup_and_clock_controls_map_exactly();
void test_month_navigation_and_all_42_cells_map_actual_dates();
void test_disabled_month_and_page_controls_do_not_map();
void test_day_header_pagination_and_populated_rows_map();
void test_event_details_require_a_selected_event();
void test_editor_date_time_and_navigation_controls_map();
void test_t9_controls_digits_special_keys_and_timestamp_map();
void test_editor_review_maps_back_cancel_and_save();
void test_half_open_boundaries_gaps_empty_cells_and_outside_do_not_map();
```

Use timestamp `123456` for T9 assertions. For each digit key, assert `digit` is the corresponding character in `"123456789*0#"`; indices 9 and 11 must instead assert `T9Backspace` and `ToggleT9Case`. Assert that all T9 actions copy `timestampMilliseconds`.

- [ ] **Step 2: Compile and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_touch_mapper.cpp src\ui\agenda_touch_mapper.cpp -o build\test_agenda_touch_mapper.exe
```

Expected: compilation fails because the mapper files do not exist.

- [ ] **Step 3: Create the header and a small action factory**

Create `src/ui/agenda_touch_mapper.h` with the public interface above and includes for `<cstdint>`, `<optional>`, `core/agenda_ui_types.h`, and `ui_geometry.h`.

In the `.cpp`, use this factory so every result is fully initialized:

```cpp
AgendaUiAction makeAction(
    AgendaUiActionType type,
    std::uint32_t timestampMilliseconds
)
{
    AgendaUiAction action;
    action.type = type;
    action.timestampMilliseconds =
        timestampMilliseconds;
    return action;
}
```

- [ ] **Step 4: Implement overlay-first mapping**

Return `std::nullopt` immediately when the point is outside `FullDisplay`. When an overlay is active, inspect only this table and return without inspecting the underlying screen:

| Overlay | Bounds | Action |
|---|---|---|
| Reminder | `OverlayAcknowledge` | `AcknowledgeReminder` |
| ConfirmDelete | `OverlayConfirm` | `ConfirmDelete` |
| ConfirmDelete | `OverlayCancel` | `CancelDelete` |
| ConfirmDiscard | `OverlayConfirm` | `ConfirmDiscard` |
| ConfirmDiscard | `OverlayCancel` | `CancelDiscard` |
| OperationError | `OverlayRetry` | `RetryOperation` |
| OperationError | `OverlayOperationCancel` | `CancelOperation` |
| ClockError | `OverlayClockOk` | `CancelOperation` |

The `ClockError` button deliberately maps to `CancelOperation`, which is the existing state-machine action that dismisses both operation and clock error overlays.

- [ ] **Step 5: Implement screen mapping with the exact enablement rules**

| Screen | Bounds/data | Action and payload | Enabled when |
|---|---|---|---|
| StartupError | `FooterCenter` | `RetryStartup` | always |
| ClockSetup | date/hour/minute shared controls | matching `Clock*` action | always |
| ClockSetup | `FooterCenter` | `ConfirmClock` | always |
| Month | `MonthPrevious` | `PreviousMonth` | `monthPreviousEnabled` |
| Month | `MonthNext` | `NextMonth` | `monthNextEnabled` |
| Month | cell 0–41 | `SelectDate`, copy `cell.date` | `cell.date.has_value()` |
| Day | `DayBack` | `Back` | `selectedDate.has_value()` |
| Day | `DayCreate` | `StartCreate` | `selectedDate.has_value()` |
| Day | `FooterLeft` | `PreviousDayPage` | `dayPreviousEnabled` |
| Day | `FooterRight` | `NextDayPage` | `dayNextEnabled` |
| Day | row 0–2 | `SelectEvent`, copy `eventId` | index `< dayEvents.size()` |
| EventDetails | footer L/C/R | `Back`/`StartEdit`/`RequestDelete` | `selectedEvent.has_value()` |
| EditorDate | previous/next | `EditorPreviousDay`/`EditorNextDay` | always |
| EditorDate | footer L/C/R | `EditorBack`/`EditorCancel`/`EditorNext` | always |
| EditorTime | either mode button | `ToggleEventTime` | always |
| EditorTime | hour/minute controls | matching `Editor*` action | `editor.time.has_value()` |
| EditorTime | footer L/C/R | `EditorBack`/`EditorCancel`/`EditorNext` | always |
| EditorText | `T9Language`/`T9Mode`/`T9Case` | matching toggle | always |
| EditorText | keypad 0–11 | digit, Backspace, or case action | always |
| EditorText | footer L/C/R | `EditorBack`/`EditorCancel`/`EditorNext` | always |
| EditorReview | footer L/C/R | `EditorBack`/`EditorCancel`/`SaveEditor` | always |

For each screen, check named controls before repeated cells/rows/keys. Return the first match and then exit; never accumulate actions.

- [ ] **Step 6: Run mapper tests**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_touch_mapper.cpp src\ui\agenda_touch_mapper.cpp -o build\test_agenda_touch_mapper.exe
if ($LASTEXITCODE -ne 0) { throw 'Touch mapper compile failed.' }
.\build\test_agenda_touch_mapper.exe
if ($LASTEXITCODE -ne 0) { throw 'Touch mapper tests failed.' }
```

Expected: `All agenda touch mapper tests passed.`

- [ ] **Step 7: Run renderer and mapper together, then commit**

```powershell
.\build\test_agenda_renderer.exe
.\build\test_agenda_touch_mapper.exe
git diff --check
git add src\ui\agenda_touch_mapper.h src\ui\agenda_touch_mapper.cpp tests\test_agenda_touch_mapper.cpp
git diff --cached --check
git commit -m "feat: map agenda touch releases to actions"
```

---

### Task 3: Implement RGB565/text math and the Greek Arduino_GFX surface

**Files:**
- Create: `src/ui/draw_surface_math.h`
- Create: `tests/test_draw_surface_math.cpp`
- Create: `src/platform/esp32/arduino_gfx_surface.h`
- Create: `src/platform/esp32/arduino_gfx_surface.cpp`
- Modify: `src/platform/esp32/display_hardware.h`
- Modify: `platformio.ini`

**Interfaces:**

```cpp
struct UiTextMetrics
{
    int xOffset;
    int yOffset;
    int width;
    int height;
};

constexpr std::uint16_t uiColorToRgb565(UiColor color);

constexpr UiPoint alignedTextCursor(
    const UiRect& bounds,
    const UiTextMetrics& metrics,
    UiTextAlignment alignment
);
```

- [ ] **Step 1: Add RED tests for pure drawing math**

Assert these exact cases:

```cpp
assert(uiColorToRgb565(UiColor{255, 0, 0}) == 0xF800);
assert(uiColorToRgb565(UiColor{0, 255, 0}) == 0x07E0);
assert(uiColorToRgb565(UiColor{0, 0, 255}) == 0x001F);
assert(uiColorToRgb565(UiColor{255, 255, 255}) == 0xFFFF);

constexpr UiRect bounds{10, 20, 100, 40};
constexpr UiTextMetrics metrics{-2, -12, 30, 16};
assert(alignedTextCursor(bounds, metrics, UiTextAlignment::Left) == (UiPoint{12, 34}));
assert(alignedTextCursor(bounds, metrics, UiTextAlignment::Center) == (UiPoint{47, 34}));
assert(alignedTextCursor(bounds, metrics, UiTextAlignment::Right) == (UiPoint{82, 34}));
```

Add `constexpr bool operator==(UiPoint, UiPoint)` to `ui_geometry.h` if the assertions require it.

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_draw_surface_math.cpp -o build\test_draw_surface_math.exe
```

Expected: RED because the helper header does not exist.

- [ ] **Step 2: Implement the exact portable formulas**

Use:

```cpp
constexpr std::uint16_t uiColorToRgb565(UiColor color)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(color.red & 0xF8) << 8) |
        (static_cast<std::uint16_t>(color.green & 0xFC) << 3) |
        (static_cast<std::uint16_t>(color.blue) >> 3)
    );
}
```

For alignment, first place the measured ink box at `bounds.x`, centered, or at the right edge; vertically center the ink box. Return cursor `{inkX - xOffset, inkY - yOffset}`. This produces the exact values in Step 1.

- [ ] **Step 3: Run the portable math test GREEN**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_draw_surface_math.cpp -o build\test_draw_surface_math.exe
.\build\test_draw_surface_math.exe
```

Expected: `All draw surface math tests passed.`

- [ ] **Step 4: Pin U8g2 and enable the full source boundary**

Update `platformio.ini`:

```ini
board_build.filesystem = littlefs
lib_deps =
    moononournation/GFX Library for Arduino@1.6.7
    olikraus/U8g2@2.36.18
build_src_filter =
    -<*>
    +<touch_sample.cpp>
    +<core/*.cpp>
    +<ui/*.cpp>
    +<platform/esp32/*.cpp>
```

In `display_hardware.h`, include U8g2 before Arduino_GFX:

```cpp
#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
```

- [ ] **Step 5: Implement `ArduinoGfxSurface`**

The class must be `final`, store `Arduino_GFX&`, override every `DrawSurface` method, and configure text before every text measurement/draw:

```cpp
graphics_.setFont(u8g2_font_unifont_t_greek);
graphics_.setTextSize(1);
graphics_.setTextWrap(false);
graphics_.setUTF8Print(true);
```

For `drawText`:

1. Copy the exact view into `std::string owned(text.data(), text.size())`.
2. Call `getTextBounds(owned.c_str(), 0, 0, &xOffset, &yOffset, &width, &height)`.
3. Call `alignedTextCursor` with those metrics.
4. Set the RGB565 text color and cursor.
5. Call `graphics_.write(reinterpret_cast<const std::uint8_t*>(owned.data()), owned.size())`.

Implement positive thickness by repeated inset outlines for rectangles, rounded rectangles, and circles. For lines, draw parallel one-pixel lines; offset Y when `abs(dx) >= abs(dy)`, otherwise offset X. Treat thickness less than one as a no-op. Skip inset iterations once width, height, or radius becomes non-positive.

- [ ] **Step 6: Build the embedded adapter without uploading**

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
if ($LASTEXITCODE -ne 0) { throw 'ArduinoGfxSurface build failed.' }
```

Expected: `SUCCESS`. If U8g2's symbol name or Arduino_GFX's text-bounds overload differs in the pinned versions, inspect the downloaded headers and adjust only the adapter; do not alter `DrawSurface` or UI strings.

- [ ] **Step 7: Check and commit**

```powershell
git diff --check
git add platformio.ini src\ui\draw_surface_math.h src\ui\ui_geometry.h tests\test_draw_surface_math.cpp src\platform\esp32\display_hardware.h src\platform\esp32\arduino_gfx_surface.h src\platform\esp32\arduino_gfx_surface.cpp
git diff --cached --check
git commit -m "feat: render agenda through Arduino GFX"
```

---

### Task 4: Implement the boot-invalid monotonic ESP32 clock

**Files:**
- Create: `src/platform/esp32/esp32_clock.h`
- Create: `src/platform/esp32/esp32_clock.cpp`
- Create: `tests/test_esp32_clock.cpp`

**Interfaces:**

```cpp
using MonotonicMillisecondsFunction =
    std::uint64_t (*)();

class Esp32Clock final : public Clock
{
public:
    explicit Esp32Clock(
        MonotonicMillisecondsFunction monotonicMilliseconds
    );

    ClockReadResult read() const override;
    ClockError set(const DateTime& value) override;

private:
    MonotonicMillisecondsFunction monotonicMilliseconds_;
    bool valid_;
    DateTime base_;
    std::uint64_t baseMilliseconds_;
};
```

- [ ] **Step 1: Add RED clock tests with an injected fake timer**

Use a global `std::uint64_t fakeMilliseconds` and a function returning it. Add and call:

```cpp
void test_clock_is_unavailable_before_set();
void test_clock_rejects_invalid_date_and_time();
void test_clock_advances_only_after_complete_elapsed_minutes();
void test_clock_crosses_hour_day_month_and_leap_day_boundaries();
void test_clock_rejects_time_source_going_backwards();
void test_clock_rejects_values_after_2099_12_31_23_59();
```

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_esp32_clock.cpp src\platform\esp32\esp32_clock.cpp src\core\date_time.cpp -o build\test_esp32_clock.exe
```

Expected: RED because the clock files do not exist.

- [ ] **Step 2: Implement the clock without Arduino dependencies**

The constructor sets `valid_` false. `set()` validates both members, captures the base and current 64-bit milliseconds, and leaves the prior valid state unchanged on invalid input. `read()` returns `Unavailable` while invalid and `ReadFailed` if the timer decreases.

Convert the base with `serialMinute(base_)`, add `(now - baseMilliseconds_) / 60000ULL`, and convert back using a private function that:

1. Rejects values below `serialMinute(DateTime{Date{2000, 1, 1}, TimeOfDay{0, 0}})`.
2. Rejects values above `serialMinute(DateTime{Date{2099, 12, 31}, TimeOfDay{23, 59}})`.
3. Removes complete years with `365` or `366` days.
4. Removes complete months with `daysInMonth`.
5. Sets day to the remaining zero-based day plus one and derives hour/minute from the remainder.

- [ ] **Step 3: Run clock tests GREEN**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_esp32_clock.cpp src\platform\esp32\esp32_clock.cpp src\core\date_time.cpp -o build\test_esp32_clock.exe
.\build\test_esp32_clock.exe
```

Expected: `All ESP32 clock tests passed.`

- [ ] **Step 4: Verify the PlatformIO build and commit**

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
if ($LASTEXITCODE -ne 0) { throw 'ESP32 clock build failed.' }
git diff --check
git add src\platform\esp32\esp32_clock.h src\platform\esp32\esp32_clock.cpp tests\test_esp32_clock.cpp
git diff --cached --check
git commit -m "feat: add monotonic ESP32 agenda clock"
```

---

### Task 5: Implement mounted LittleFS byte storage without formatting

**Files:**
- Create: `src/platform/esp32/littlefs_byte_storage.h`
- Create: `src/platform/esp32/littlefs_byte_storage.cpp`

**Interfaces:**

```cpp
class LittleFsVolume
{
public:
    bool begin();
    bool isMounted() const;

private:
    bool mounted_ = false;
};

class LittleFsByteStorage final : public ByteStorage
{
public:
    LittleFsByteStorage(
        LittleFsVolume& volume,
        const char* path,
        const char* temporaryPath
    );

    ByteReadResult read() const override;
    ByteStorageError write(
        const std::vector<std::uint8_t>& bytes
    ) const override;
};
```

- [ ] **Step 1: Create the header and force a build failure**

Create the declarations above, add private `LittleFsVolume& volume_`, `const char* path_`, and `const char* temporaryPath_`, then include the header temporarily from `main.cpp` and declare the three objects used later:

```cpp
LittleFsVolume littleFsVolume;
LittleFsByteStorage primaryStorage{
    littleFsVolume,
    "/agenda.bin",
    "/agenda.tmp"
};
LittleFsByteStorage backupStorage{
    littleFsVolume,
    "/agenda.bak",
    "/agenda.bak.tmp"
};
```

Build and expect an undefined-reference failure because the methods are not implemented:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
```

- [ ] **Step 2: Implement safe mounting and reads**

`LittleFsVolume::begin()` must call exactly `LittleFS.begin(false)` and retain success. Repeated calls after success return true without remounting. Reads must:

1. Return `ReadFailed` when the volume is not mounted.
2. Return `NotFound` when the path does not exist.
3. Open with `FILE_READ`, reject a negative size or a size above `2 * 1024 * 1024` bytes, resize the vector, and require an exact read count.
4. Close before returning.

- [ ] **Step 3: Implement temp-file replacement writes**

Writes must:

1. Return `WriteFailed` when not mounted.
2. Remove only the exact temporary path before starting.
3. Open the temporary path with `FILE_WRITE`, require the exact write count, and require `getWriteError() == 0`.
4. Call `flush()`, check `getWriteError()` again, close the file, remove the target only after the temporary write succeeded, and rename the temporary path to the target.
5. Remove the temporary file on failure and return `WriteFailed`.

The repository already owns main/backup recovery. The adapter must not copy primary data into the backup by itself.

- [ ] **Step 4: Build without uploading**

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
if ($LASTEXITCODE -ne 0) { throw 'LittleFS adapter build failed.' }
```

Expected: `SUCCESS` and no implicit filesystem image upload.

- [ ] **Step 5: Check forbidden formatting and commit**

```powershell
rg -n "LittleFS\.begin\(true\)|format\(" src platformio.ini
```

Expected: no matches.

```powershell
git diff --check
git add src\platform\esp32\littlefs_byte_storage.h src\platform\esp32\littlefs_byte_storage.cpp src\platform\esp32\main.cpp
git diff --cached --check
git commit -m "feat: persist agenda data in LittleFS"
```

---

### Task 6: Replace the touch smoke test with the application composition root

**Files:**
- Modify: `src/platform/esp32/main.cpp`

**Interfaces:**
- Consumes: all four adapters, `AgendaRepository`, `AgendaController`, `AgendaUiStateMachine`, `AgendaRenderer`, and `AgendaTouchMapper`.
- Produces: Arduino `setup()` and non-blocking `loop()`.

- [ ] **Step 1: Construct the complete object graph in dependency order**

Use file-scope objects in this order:

```cpp
DisplayHardware displayHardware;
TouchHardware touchHardware;
ArduinoGfxSurface drawSurface(displayHardware.graphics());
Esp32Clock clock(monotonicMilliseconds);
LittleFsVolume littleFsVolume;
LittleFsByteStorage primaryStorage(
    littleFsVolume,
    "/agenda.bin",
    "/agenda.tmp"
);
LittleFsByteStorage backupStorage(
    littleFsVolume,
    "/agenda.bak",
    "/agenda.bak.tmp"
);
AgendaRepository repository(
    primaryStorage,
    backupStorage
);
AgendaController controller(repository);
AgendaUiStateMachine stateMachine(
    controller,
    clock,
    DateTime{Date{2026, 9, 10}, TimeOfDay{12, 0}}
);
AgendaRenderer renderer;
AgendaTouchMapper touchMapper;
```

Implement `monotonicMilliseconds()` as:

```cpp
std::uint64_t monotonicMilliseconds()
{
    return static_cast<std::uint64_t>(
        esp_timer_get_time()
    ) / 1000ULL;
}
```

- [ ] **Step 2: Add exact startup failure paths**

`setup()` must follow this order and markers:

1. `Serial.begin(115200)`, short serial delay, print `AGENDA_START`.
2. Display failure: print `DISPLAY_INIT_FAILED`, leave backlight off, set a fatal flag, return.
3. Touch failure: print `TOUCH_INIT_FAILED`, render a visible `ΣΦΑΛΜΑ ΑΦΗΣ` message through `ArduinoGfxSurface`, enable backlight, set fatal flag, return.
4. Attempt `littleFsVolume.begin()`. On failure print `LITTLEFS_MOUNT_FAILED`; continue so the state machine renders `StartupError` through the storage error path.
5. Call `controller.start()` once as a diagnostic preflight; print `AGENDA_RECOVERED_FROM_BACKUP` when `recoveredFromBackup` is true. This deliberate preflight is required because `AgendaUiStateMachine::start()` does not expose that result and no core interface may change.
6. Call `stateMachine.start()`, render its view once, enable backlight, and print `AGENDA_READY`.

The state machine performs the authoritative second load. The preflight must not skip `stateMachine.start()`.

- [ ] **Step 3: Implement press/release tracking**

Store:

```cpp
bool pressActive = false;
bool pressCancelled = false;
UiPoint pressStart{0, 0};
UiPoint latestPoint{0, 0};
std::uint64_t nextReminderTickMilliseconds = 0;
```

For each valid sample:

- On the first touched sample, capture start/latest and clear cancellation.
- While touched, update latest and cancel when `dx * dx + dy * dy > 12 * 12`.
- On the first non-touched sample after a press, clear `pressActive`; if not cancelled, map `latestPoint` with the low 32 bits of monotonic milliseconds and dispatch at most the returned action.
- Ignore samples outside 240x240 and never call `delay()` to wait for release.
- A read failure prints `TOUCH_READ_FAILED` only on the transition into failure; a later successful read clears that latch.

- [ ] **Step 4: Centralize action dispatch and immediate clock reminder check**

Create `dispatchAction(const AgendaUiAction&)`:

1. If the action is `RetryStartup` and the volume is not mounted, call `littleFsVolume.begin()` before state handling.
2. For `RetryStartup`, repeat the diagnostic `controller.start()` preflight and recovery marker, then let `stateMachine.handle(action)` perform the authoritative retry.
3. Capture the view before handling, call `handle`, and render exactly once.
4. If the action is `ConfirmClock` and the resulting screen is no longer `ClockSetup`, call `stateMachine.tick()` before rendering. This supplies the spec's immediate post-confirmation reminder evaluation without changing a core interface.

- [ ] **Step 5: Add the once-per-second reminder tick without continuous redraw**

At each loop pass, when the 64-bit monotonic time reaches the next deadline, advance the deadline by 1000 ms, capture `before`, call `stateMachine.tick()`, and capture `after`. Redraw only when any of these fields changes:

```cpp
before.overlay != after.overlay
before.controllerError != after.controllerError
before.eventError != after.eventError
before.clockError != after.clockError
before.reminder.has_value() != after.reminder.has_value()
both reminders exist and before.reminder->id != after.reminder->id
```

Do not compare the complete `AgendaUiView`; its vectors and optional domain types do not define all required equality operators.

- [ ] **Step 6: Build the complete firmware without uploading**

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
if ($LASTEXITCODE -ne 0) { throw 'Full agenda firmware build failed.' }
```

Expected: one successful link containing core, UI, LittleFS, Arduino_GFX, U8g2, display, and touch sources.

- [ ] **Step 7: Verify dependency direction and commit**

```powershell
rg -n "Arduino|LittleFS|Arduino_GFX|U8g2|esp_timer" src\core src\ui
```

Expected: no Arduino/platform matches in `src/core`; the only U8g2/Arduino_GFX references are under `src/platform/esp32`. `src/ui/draw_surface_math.h` remains standard C++ only.

```powershell
git diff --check
git add src\platform\esp32\main.cpp platformio.ini
git diff --cached --check
git commit -m "feat: integrate full agenda on ESP32"
```

---

### Task 7: Run the complete desktop regression suite and final embedded build

**Files:**
- Verify only; modify production code only when a failing test identifies a real defect.

- [ ] **Step 1: Compile every desktop test with warnings enabled**

Run these exact commands:

```powershell
New-Item -ItemType Directory -Force build | Out-Null

$common = @(
    '-std=c++17',
    '-Wall',
    '-Wextra',
    '-pedantic',
    '-Isrc'
)

g++ @common tests\test_date_time.cpp src\core\date_time.cpp -o build\test_date_time.exe
g++ @common tests\test_calendar_engine.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_calendar_engine.exe
g++ @common tests\test_utf8.cpp src\core\utf8.cpp -o build\test_utf8.exe
g++ @common tests\test_event.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp -o build\test_event.exe
g++ @common tests\test_t9_engine.cpp src\core\utf8.cpp src\core\t9_engine.cpp -o build\test_t9_engine.exe
g++ @common tests\test_agenda_manager.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp -o build\test_agenda_manager.exe
g++ @common tests\test_agenda_state.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp -o build\test_agenda_state.exe
g++ @common tests\test_crc32.cpp src\core\crc32.cpp -o build\test_crc32.exe
g++ @common tests\test_event_codec.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\crc32.cpp src\core\event_codec.cpp -o build\test_event_codec.exe
g++ @common tests\test_desktop_file_storage.cpp src\platform\desktop_file_storage.cpp -o build\test_desktop_file_storage.exe
g++ @common tests\test_agenda_repository.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp -o build\test_agenda_repository.exe
g++ @common tests\test_reminder_engine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\reminder_engine.cpp -o build\test_reminder_engine.exe
g++ @common tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
g++ @common tests\test_agenda_persistence.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\platform\desktop_file_storage.cpp -o build\test_agenda_persistence.exe
g++ @common tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
g++ @common tests\test_agenda_ui_persistence.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp src\platform\desktop_file_storage.cpp -o build\test_agenda_ui_persistence.exe
g++ @common tests\test_agenda_renderer.cpp src\ui\agenda_renderer.cpp src\core\utf8.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_agenda_renderer.exe
g++ @common tests\test_agenda_touch_mapper.cpp src\ui\agenda_touch_mapper.cpp -o build\test_agenda_touch_mapper.exe
g++ @common tests\test_draw_surface_math.cpp -o build\test_draw_surface_math.exe
g++ @common tests\test_esp32_clock.cpp src\platform\esp32\esp32_clock.cpp src\core\date_time.cpp -o build\test_esp32_clock.exe
g++ @common tests\test_cst816s_protocol.cpp -o build\test_cst816s_protocol.exe
g++ @common tests\test_touch_sample.cpp src\touch_sample.cpp -o build\test_touch_sample.exe

if ($LASTEXITCODE -ne 0) { throw 'A desktop test compile failed.' }
```

- [ ] **Step 2: Run every desktop executable**

```powershell
Get-ChildItem build\test_*.exe |
    Sort-Object Name |
    ForEach-Object {
        Write-Host "Running $($_.Name)"
        & $_.FullName
        if ($LASTEXITCODE -ne 0)
        {
            throw "$($_.Name) failed."
        }
    }
```

Expected: every executable prints its success message and returns zero.

- [ ] **Step 3: Run a clean PlatformIO build**

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28 -t clean
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
if ($LASTEXITCODE -ne 0) { throw 'Clean PlatformIO build failed.' }
```

- [ ] **Step 4: Inspect final repository state**

```powershell
git diff --check
git status --short
git log --oneline -8
```

Expected: clean working tree and the six implementation commits from Tasks 1–6 above the plan/spec commits.

---

### Task 8: Upload only after approval and execute the physical acceptance checklist

**Files:**
- Verification only.

- [ ] **Step 1: Stop and obtain explicit user approval**

State the exact command and wait for approval. Do not infer approval from earlier design approval.

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28 -t upload
```

- [ ] **Step 2: Upload and capture stable startup markers**

After approval, upload and open the monitor:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28 -t upload
if ($LASTEXITCODE -ne 0) { throw 'Upload failed.' }
& $pio device monitor -b 115200
```

Expected normal markers include `AGENDA_START`, no display/touch failure, and `AGENDA_READY`.

- [ ] **Step 3: Verify display, touch, and full agenda flow**

Check in order:

1. Greek/English/numeric text and accented Greek event text render without replacement glyphs.
2. Clock date/hour/minute controls change exactly one unit and confirmation reaches Month.
3. Month arrows, all calendar cells, Day Back/Create, event rows, and pagination work.
4. Timed and all-day creation traverse date, time, text, review, save.
5. T9 language, mode, case, 900 ms cycling, `*` Backspace, `#` case, and 500-code-point limit behave correctly.
6. Details, edit, delete, discard, retry/cancel, and reminder overlay actions match the visible buttons.
7. A held touch generates one action; movement over 12 pixels generates none.
8. The display remains stable when untouched; no continuous flicker occurs.

- [ ] **Step 4: Verify persistence and reminders with controlled data**

1. Create at least one timed and one all-day event, note their IDs, press RESET, set the clock again, and verify both records and the next ID.
2. Set the clock so each reminder is due, acknowledge it, press RESET, set the clock again, and verify the acknowledged occurrence does not reappear.
3. Use only controlled test records to corrupt `/agenda.bin`; retain a valid `/agenda.bak`; restart and verify the data recovers and Serial prints `AGENDA_RECOVERED_FROM_BACKUP`.
4. Never run a filesystem format command during recovery testing.

- [ ] **Step 5: Record evidence and finish the branch**

Record the PlatformIO success line, serial markers, checklist results, and any failure reproduction before changing code. If code changes are required, return to the relevant task, add a failing test when possible, rebuild, and repeat this acceptance task. When all checks pass, use `superpowers:verification-before-completion`, then `superpowers:finishing-a-development-branch` to close the milestone.

## Spec Coverage Check

| Spec requirement | Plan task |
|---|---|
| Shared visible geometry | Task 1 |
| Overlay-first action mapping | Task 2 |
| Greek UTF-8 Arduino_GFX output | Task 3 |
| Exact string-view length and alignment | Task 3 |
| Boot-invalid 64-bit monotonic clock | Task 4 |
| LittleFS without auto-format | Task 5 |
| Repository/controller/state-machine composition | Task 6 |
| Release-only, movement-cancelled touch | Task 6 |
| Immediate and once-per-second reminder checks | Task 6 |
| Redraw only on action/visible tick change | Task 6 |
| Desktop and embedded verification | Task 7 |
| Full physical acceptance and recovery | Task 8 |

## Implementation References

- Arduino_GFX U8g2 font integration: `https://github.com/moononournation/Arduino_GFX#u8g2-font-api`
- U8g2 font catalogue and Greek subset: `https://github.com/olikraus/u8g2/wiki/fntgrpunifont`
- Arduino-ESP32 LittleFS API: `https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS`
