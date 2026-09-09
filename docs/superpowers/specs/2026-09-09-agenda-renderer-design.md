# Agenda Renderer Design

Date: 2026-09-09

Status: Approved

## Goal

Build a hardware-independent renderer for the 240 x 240 circular display of
the Digital Diary. The renderer converts a read-only `AgendaUiView` into
simple drawing operations without depending on Arduino, ESP32, GC9A01A,
CST816S, LVGL, storage, or network services.

The first visual style is a modern dark watch interface with turquoise
accents. Rendering is static and event-driven: the application requests a
new frame only when visible state changes.

## Context

The application already has a hardware-independent UI state machine. It
exposes screens, overlays, dates, events, editor state, T9 state, reminder
state, and errors through `AgendaUiView`.

The renderer is the next layer. It owns appearance and layout, but it does
not own application state or behavior. A later display adapter will
translate the same drawing interface to the GC9A01A controller.

## Scope

The renderer covers these screens:

- clock setup;
- month view;
- day view;
- event details;
- editor date;
- editor time;
- editor text and T9 keypad;
- editor review;
- startup error.

It also covers these overlays:

- reminder;
- delete confirmation;
- discard confirmation;
- operation error;
- clock error.

## Non-goals

This stage does not include:

- the GC9A01A driver or an ESP32 graphics library;
- the CST816S driver or touch calibration;
- concrete bitmap font data;
- animation or continuous rendering;
- persistence, reminder calculation, or navigation logic;
- battery and sleep management;
- Wi-Fi, NTP, or any other internet dependency.

## Architectural decision

Use an immediate-mode, hardware-independent `DrawSurface` interface.
`AgendaRenderer` receives a `const AgendaUiView&` and a `DrawSurface&`, then
issues drawing operations directly to that surface.

This approach was chosen instead of a retained render-command list because
it requires less memory and fewer allocations. It was chosen instead of
LVGL because it keeps the renderer small, testable on the desktop, and
independent of a specific embedded framework.

## Component boundaries

### AgendaRenderer

`AgendaRenderer`:

- reads `AgendaUiView`;
- chooses the correct screen renderer;
- applies the shared theme and geometry;
- draws the base screen first;
- draws an active overlay last;
- never changes `AgendaUiView` or application state.

It does not:

- call `AgendaUiStateMachine::handle()` or `tick()`;
- access `AgendaController`;
- read the clock;
- read or write storage;
- interpret touch coordinates;
- decide when another frame is required.

### DrawSurface

`DrawSurface` is the output port used by the renderer. It provides only the
primitives required by the approved layouts:

- clear the display;
- fill and outline rectangles;
- fill and outline rounded rectangles;
- fill and outline circles;
- draw lines;
- draw UTF-8 text with a text style and alignment.

Coordinates use signed integers. Dimensions use non-negative integers.
`UiRect` contains `x`, `y`, `width`, and `height`. `UiColor` contains 8-bit
red, green, and blue channels. The embedded adapter converts these colors
to RGB565. Text is passed as UTF-8 and supports left, center, and right
alignment.

Drawing operations and `AgendaRenderer::render()` return `void`. A display
transport failure belongs to the future platform adapter and application
integration; it must not mutate the UI model or be represented as a domain
error by the renderer.

The interface does not expose GC9A01A, SPI, Arduino, frame buffers, or a
specific font type.

### Shared geometry

Shared geometry types describe points and rectangles. Layout constants and
formulas define the positions of visible and interactive controls.

The future touch mapper must use the same geometry instead of duplicating
coordinates. This keeps each visible control aligned with its touch target.
The renderer itself still performs no hit testing.

### RecordingDrawSurface

Tests provide a recording implementation of `DrawSurface`. It stores each
drawing operation with its type, coordinates, dimensions, color, text
style, alignment, and text content.

This test double belongs in the renderer test file unless later tests need
to share it.

## Proposed files

The implementation introduces:

```text
src/ui/ui_geometry.h
src/ui/draw_surface.h
src/ui/agenda_renderer.h
src/ui/agenda_renderer.cpp
tests/test_agenda_renderer.cpp
```

The renderer includes `core/agenda_ui_types.h`, but core files do not
include renderer files. The dependency therefore remains one-directional:

```text
core UI model -> renderer -> abstract drawing surface
```

The later hardware dependency will be:

```text
GC9A01A adapter -> DrawSurface
```

## Display and safe area

The target display is fixed at 240 x 240 pixels. The renderer clips its own
layout to these bounds.

Because the display is circular, important text and controls stay inside a
central safe area. Decorative backgrounds may extend to the full display.
Controls near the top and bottom become narrower as they approach the edge.

The screen is divided conceptually into:

- a top title or navigation area;
- a central content area;
- a bottom action area.

Exact integer coordinates are centralized in the shared layout definitions
and verified by tests.

## Visual theme

The approved theme is modern and dark:

| Role | RGB color |
|---|---|
| Background | `#0B1217` |
| Elevated surface | `#17242B` |
| Primary accent | `#27D3C2` |
| Primary text | `#F4F7F8` |
| Secondary text | `#9AAAB4` |
| Disabled content | `#586770` |
| Destructive/error | `#FF6B6B` |
| Overlay scrim | `#05080A` |

The first drawing interface does not require alpha blending. An overlay
uses the solid overlay-scrim color behind its card. The desktop test double
records that requested color.

## Text styles and Greek support

The renderer requests semantic text styles rather than concrete fonts:

- `Title`;
- `Heading`;
- `Body`;
- `Caption`;
- `Keypad`.

The display adapter will later map these roles to bitmap fonts and sizes.
The selected font set must contain the Greek and Latin characters used by
the interface and user event text, as well as digits and required symbols.

The renderer passes UTF-8 strings without converting them to byte-oriented
character arrays. When visible text must be shortened, truncation must use
the existing UTF-8 utilities so that it never splits a code point.

Dates use `DD/MM/YYYY`, times use the 24-hour `HH:MM` format, and month names
and fixed interface labels are Greek. Event text remains exactly in the
language entered by the user.

## Screen layouts

### Clock setup

Show a short Greek title, the draft date, and the draft time. Previous and
next controls surround the currently edited values. The confirmation action
is prominent and remains inside the lower safe area.

### Month

Show the month and year with previous and next arrows. The calendar is
Monday-first and contains 42 cells in 7 columns and 6 rows.

The displayed state uses these visual rules:

- today receives an outline;
- the selected date receives the primary accent fill;
- a date with events receives a small marker;
- dates outside the displayed month use the disabled color;
- disabled month navigation uses the disabled color.

Selection has stronger visual priority than today. An event marker remains
visible on both selected and unselected dates.

### Day

Show the selected date and at most three event rows per page. Each row shows
the time when present and a UTF-8-safe text preview. All-day events use a
short Greek all-day label.

Previous and next page controls reflect the enabled flags from
`AgendaUiView`. A create action is always visually available when the screen
state permits it.

### Event details

Show the selected event date, optional time, and text. Provide visible back,
edit, and delete actions. Delete uses the destructive color.

If no selected event is present, render a safe error presentation instead
of dereferencing the missing value.

### Editor date

Show the editor step, current draft date, previous and next day controls,
and forward, back, and cancel actions.

### Editor time

Show whether the event is timed or all-day. When timed, show hour and minute
controls. Disabled time fields remain readable but visually muted.

### Editor text

Show the current text, language, input mode, case state, and a 3 x 4 T9
keypad. The layout prioritizes keypad target size over decorative content.

### Editor review

Show the complete draft summary with save, back, and cancel actions. Create
and update modes use appropriate Greek labels.

### Startup error

Show a concise Greek error message and a prominent retry action. The screen
must remain renderable even when no agenda data is available.

## Overlay layouts

An overlay is rendered after the base screen. It consists of a dark scrim
and a centered rounded card contained in the circular safe area.

Reminder overlays show the event summary and acknowledgement action.
Confirmation overlays show destructive or discard wording and two distinct
choices. Error overlays show the relevant error category and a retry or
cancel action when the UI state offers one.

An overlay never changes the underlying screen rendering rules.

## Rendering lifecycle

`AgendaRenderer` is stateless and renders a complete frame on every call.
The owner of the renderer decides when to call it.

Every frame begins with a full-display clear using the background color.

The first device integration requests rendering only after a visible state
change, including:

- startup completion or failure;
- a handled touch action;
- a clock value that changes visible content;
- reminder appearance or acknowledgement;
- an operation result that changes a screen or overlay.

The application does not redraw continuously in its main loop. This reduces
SPI traffic, processor use, and battery consumption.

## Invalid and incomplete state

The renderer must not crash when an optional value required by a screen is
missing. It draws a safe error presentation using the available error state.

Unknown enum values fall back to a generic safe presentation. Unsupported
or invalid coordinates are never sent intentionally. The platform adapter
may additionally clip primitives at the physical display boundary.

The renderer does not invent domain data and does not repair state.

## Testing strategy

Development follows test-driven development in batches of two or three
tests. Each batch begins with failing tests, adds the smallest implementation
needed, and then reruns the renderer and affected core suites.

Tests assert semantic drawing commands rather than pixel-perfect images.
They cover:

1. theme primitives and 240 x 240 bounds;
2. each base screen and its essential labels and controls;
3. all 42 month cells and their visual states;
4. day rows, page controls, all-day labels, and UTF-8-safe previews;
5. event details and missing-event safety;
6. every editor stage and T9 state;
7. every overlay and the requirement that overlay commands appear last;
8. disabled actions and error presentation;
9. preservation of the input `AgendaUiView`.

Renderer tests compile with the necessary core implementation files but do
not require Arduino or any hardware SDK. Existing core and persistence test
binaries must continue to pass.

## Completion criteria

The renderer stage is complete when:

- every supported screen has an implemented layout;
- every supported overlay has an implemented layout;
- all drawing commands remain inside the 240 x 240 display bounds;
- Greek UTF-8 text is preserved correctly;
- renderer tests compile with `-std=c++17 -Wall -Wextra -pedantic` without
  warnings;
- all renderer and existing project tests pass;
- `git diff --check` reports no problems;
- the working tree contains only the intended renderer changes before the
  implementation commit.

## Hardware handoff

After renderer completion, device integration proceeds in this order:

1. identify the exact board revision and verified pin configuration;
2. perform a USB-only startup check;
3. run a GC9A01A solid-color display test;
4. implement the GC9A01A `DrawSurface` adapter;
5. render the diary month screen on the physical display;
6. integrate the CST816S touch controller and calibrate hit regions;
7. verify the battery connector polarity before attaching the battery.

This handoff preserves the offline requirement. Neither the renderer nor
the planned hardware integration requires internet access during normal
operation.
