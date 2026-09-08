# Agenda UI State Machine Design

Date: 2026-09-08

Status: Approved

## Goal

Add the first complete user-interface logic layer for the offline Digital
Diary.

The new layer controls screen navigation, event browsing, event editing,
manual clock setup, and reminder presentation without depending on a
specific display library, touch controller, operating system, or network
service.

The initial implementation is deliberately a UI core rather than a
pixel renderer. After it is complete, development continues with the
remaining device layers: display rendering, touch-coordinate mapping,
the ESP32 clock adapter, LittleFS storage, power management, and physical
hardware testing. These items are staged, not removed from the final
product.

## Approved Product Decisions

- The application opens on a classic Monday-first monthly calendar.
- Tapping a date opens a separate day screen.
- Tapping an event opens a details screen before edit or deletion.
- Deletion requires a Yes/No confirmation.
- Event creation and editing use the same four-step wizard:
  1. date,
  2. all-day or time,
  3. T9 text,
  4. review and save.
- A date selected from the month view is prefilled in the editor but can
  still be changed with previous/next controls.
- New events start as all-day events. Time controls appear only when the
  user enables a time.
- Enabling time on a new all-day event initializes the time to 12:00.
- Time changes in five-minute increments.
- The text editor uses a familiar 3-by-4 telephone T9 layout.
- The T9 editor starts in Greek lowercase letter mode.
- The T9 screen exposes Greek/English, letter/number mode, case,
  backspace, space, and completion controls.
- Unsaved changes require confirmation before they are discarded.
- A successful create or edit returns to the monthly screen.
- A successful deletion returns to the selected day screen.
- Save failure preserves the editor and its complete draft and offers
  Retry and Cancel.
- The day list uses explicit up/down page controls rather than swipe.
- Dates from neighbouring months are dimmed but active. Selecting one
  updates the displayed month and opens that day.
- A small dot indicates that a date has one or more events.
- The UI language is Greek, with 24-hour time and `DD/MM/YYYY` dates.
- Reminder popups are deferred while the editor is active.
- Leaving the editor triggers an immediate due-reminder check.
- After a successful reminder acknowledgement, the next due reminder is
  shown immediately, one at a time.
- The first version uses the ESP32-S3 internal timekeeping facilities and
  manual clock setup. It does not require internet access or additional
  RTC hardware.

## Scope

This stage includes:

- a hardware-independent UI state machine,
- semantic UI actions,
- read-only view models,
- startup and manual-clock states,
- month, day, details, and editor navigation,
- editor draft management,
- pagination and event indicators,
- delete and discard confirmations,
- save-error presentation and retry behavior,
- reminder-popup orchestration,
- a platform-neutral clock abstraction,
- fake-clock and UI-state unit tests.

This stage does not yet include:

- pixel-perfect LCD drawing,
- fonts or final colors,
- GC9A01A display integration,
- CST816S coordinates, gestures, or calibration,
- LVGL widgets or callbacks,
- the concrete ESP32 clock implementation,
- LittleFS integration,
- sleep scheduling or battery optimization,
- final physical-device testing.

Those deferred layers are the next roadmap stages after the UI core.

## Architectural Approach

The selected architecture is a single deterministic UI state machine.
It sits above the existing core modules and below the future renderer and
touch adapter.

The dependency direction is:

```text
Touch coordinates
       |
       v
Future TouchMapper
       |
       v
semantic UiAction
       |
       v
AgendaUiStateMachine -----> Clock
       |
       +-----> CalendarEngine
       +-----> T9Engine
       +-----> AgendaController
       |
       v
read-only UiViewModel
       |
       v
Future LCD renderer
```

The touch mapper never changes agenda data. It only converts a physical
touch into a semantic action.

The renderer never performs navigation or persistence. It only draws a
view model.

The state machine does not access `AgendaRepository` directly. All
permanent agenda changes continue to pass through `AgendaController`,
which already provides transactional persistence and rollback.

## Clock Abstraction

The UI requires a clock because event validation and reminder lookup need
a valid current `DateTime`. A new platform-neutral `Clock` interface
provides two operations:

- read the current local `DateTime`,
- set the current local `DateTime` after manual user entry.

A clock result distinguishes:

- success with a valid value,
- an unavailable or uninitialized clock,
- a read or write failure.

The UI accepts only dates supported by the existing core, from 2000
through 2099, and valid 24-hour times.

Desktop tests use a deterministic fake clock. The later ESP32 adapter
uses the ESP32-S3 system clock and internal RTC timer. Because a complete
power loss resets that timer, an invalid or uninitialized value sends the
user to manual clock setup. No NTP or internet connection is required.

## Main Component

`AgendaUiStateMachine` owns only transient application-interface state.
It receives references to `AgendaController` and `Clock`.

Its public surface provides operations equivalent to:

- `start()` to load the agenda and inspect the clock,
- `tick()` to perform a caller-triggered reminder check,
- `handle(UiAction)` to process one semantic user action,
- `view()` to return a read-only snapshot for rendering.

The exact C++ names may be refined in the implementation plan, but these
responsibility boundaries are fixed.

The state machine does not own an internal timer. The future application
loop decides how often to call `tick()`. Each tick is deterministic and
performs at most one reminder lookup.

## Screens

The primary screen states are:

1. `ClockSetup`
2. `Month`
3. `Day`
4. `EventDetails`
5. `EditorDate`
6. `EditorTime`
7. `EditorText`
8. `EditorReview`
9. `StartupError`

The normal startup flow is:

```text
start
  |
  +-- agenda load fails ----------------> StartupError
  |
  +-- agenda loads, clock unavailable --> ClockSetup
  |
  +-- agenda and clock valid -----------> Month
```

The normal browsing flow is:

```text
Month <---- Back ---- Day <---- Back ---- EventDetails
  |                    |                     |
  | tap date           | New                 | Edit
  v                    v                     v
 Day                EditorDate           EditorDate
```

The editor flow is:

```text
EditorDate
    |
EditorTime
    |
EditorText
    |
EditorReview
    |
successful save
    |
  Month
```

Back moves to the previous editor step. Cancel attempts to leave the
whole editor. If the draft is dirty, Cancel first opens a discard
confirmation.

## Overlays

The UI supports one modal overlay at a time:

- reminder,
- delete confirmation,
- discard confirmation,
- save error,
- delete error,
- clock error.

Overlays never stack. A reminder is deferred while any blocking overlay
or editor screen is active. When the blocking state ends, the next tick
can show the reminder.

Closing an overlay returns to the exact underlying screen and transient
state unless the confirmed operation requires a documented transition.

## Transient State

The state machine stores:

- the active screen,
- the active overlay,
- the displayed `YearMonth`,
- the selected `Date`,
- the selected event ID,
- the day-list page index,
- the editor mode: create or update,
- the editor event ID for updates,
- a partial editor draft,
- whether the draft is dirty,
- the current T9 engine,
- a safe copy of the currently displayed reminder event,
- the last UI-level error category required by the view.

It does not duplicate the complete agenda state. Browsing view models are
derived from the read-only agenda exposed by `AgendaController`.

Event IDs are stored instead of pointers. If an ID is no longer present,
the state machine safely returns to the closest valid parent screen.

## Editor Draft

The editor uses a UI-specific partial draft rather than exposing
`EventDraft` directly. During editing, text may temporarily be empty and
individual fields may not yet be complete.

The draft contains:

- selected date,
- all-day flag,
- optional time,
- text,
- create/update mode,
- optional original event ID,
- dirty flag.

For create, the selected day is prefilled and the event starts all-day.

For update, all existing event fields are copied into the draft. The
original event is not mutated until final Save.

On final Save, the state machine commits any pending T9 character,
creates an `EventDraft`, and calls the corresponding
`AgendaController` operation with the current clock value.

Validation failures keep the draft intact. The view exposes a semantic
error so the renderer can display Greek text and highlight the relevant
step without embedding display strings in domain logic.

Successful create or update clears the editor state and returns to the
month containing the saved event.

## Date and Time Editing

Date previous/next controls move by one calendar day and stop at the
supported boundaries of 2000-01-01 and 2099-12-31.

New events default to all-day. Enabling a time exposes hour and minute
controls and initializes the time to 12:00.

Minute controls change by five minutes. Crossing `55 -> 00` or `00 ->
55` carries into the hour while remaining on the same event date. Hour
controls use 24-hour values and wrap between 23 and 00.

Disabling time removes the optional time value and returns the draft to
all-day.

## T9 Editing

The UI reuses the existing `T9Engine` instead of duplicating text input
logic.

The standard 3-by-4 keypad exposes digits 0 through 9. Digit presses
include a monotonic millisecond timestamp so the existing 900 ms
multi-tap behavior remains deterministic.

Each editor session starts in Greek lowercase letter mode. The user can
then change language, case, or numeric mode without changing the global
Greek UI language.

Separate semantic actions provide:

- Greek/English toggle,
- letter/number toggle,
- upper/lowercase toggle,
- backspace,
- completion of the editor step.

Digit 0 inserts a space in letter mode, following the existing engine.
The existing 500-code-point limit remains authoritative. Rejected input
does not leave the UI in a partially changed state.

## Month Screen

The month screen uses `buildMonthGrid()` and therefore always exposes 42
cells in Monday-first order.

- Previous and next arrows call the existing month-navigation helpers.
- At January 2000 and December 2099, the unavailable arrow is disabled.
- Neighbouring-month dates are dimmed but selectable.
- Selecting a neighbouring date changes the displayed month before
  opening the day screen.
- A date with at least one event receives a small event dot.
- Today's date and the selected date use separate visual flags so neither
  is confused with the event dot.

The state machine supplies flags; final colors and shapes belong to the
future renderer.

## Day Screen and Pagination

The day screen reads events in the deterministic order already maintained
by `AgendaManager`:

1. all-day events,
2. timed events from earliest to latest,
3. event ID as the final tie-breaker.

The initial view exposes up to three event rows per page. This value can
be changed as a single UI constant after physical display testing.

Explicit up/down actions change pages. The unavailable direction is
disabled.

After deletion or another state change, the page index is clamped to the
last valid page. An empty day shows an empty-state view and an active New
button.

Each row contains an event ID, all-day/time information, and a shortened
display copy of the text. The complete text remains unchanged in the
agenda and is shown on the details screen.

## Details and Deletion

The details screen displays a safe event copy with date, optional time,
full text, Edit, Delete, and Back actions.

Delete first opens a Yes/No confirmation. Yes calls
`AgendaController::deleteEvent()`.

- Success returns to the selected day and clamps pagination.
- Failure keeps the details screen and offers Retry and Cancel.
- Cancel closes the error without changing agenda state.

## Save Errors

Create and update failures never discard the draft.

For repository failures, the editor remains on review and shows Retry and
Cancel:

- Retry repeats the same controller request.
- Cancel closes only the error and returns to editor review.
- Leaving the editor still requires the normal Cancel action and discard
  confirmation when dirty.

Domain validation errors are shown without calling persistence when the
controller rejects the draft before save.

## Reminder Flow

`tick()` asks `AgendaController::nextReminder()` for one due event only
when:

- startup completed successfully,
- the clock is valid,
- no modal overlay is active,
- the editor is not active.

An action that successfully leaves the editor performs the same lookup
immediately after closing the editor state. It does not wait for an
arbitrary later polling interval.

When an event is due, the UI stores the safe event copy and opens the
reminder overlay.

Pressing OK calls `AgendaController::acknowledgeReminder()`.

- On success, the overlay closes and another reminder lookup occurs so
  the next due event can appear immediately.
- On persistence failure, the same reminder stays visible with an error
  state. Pressing OK retries acknowledgement.
- The reminder is never considered seen solely because it was drawn.

The UI stores no reminder queue. It always asks the controller for the
next valid reminder, preventing stale entries after edit or deletion.

## View Model

`view()` returns an owned, read-only snapshot. It contains:

- active screen and overlay enums,
- screen-specific labels and enabled/disabled flags,
- month cells and event-dot flags,
- visible day-page rows,
- selected event details,
- editor values and T9 mode indicators,
- semantic validation or storage errors,
- reminder details.

The model contains no mutable pointers or references into
`AgendaManager`. The renderer maps semantic labels and errors to Greek
strings.

## Semantic Actions

The future touch layer produces actions rather than directly calling
domain code. Actions cover:

- retry startup,
- increment/decrement manual date and time,
- confirm clock setting,
- previous/next month,
- select a date,
- previous/next day-list page,
- select an event,
- start create or edit,
- move forward/back through editor steps,
- change draft date,
- toggle all-day/time,
- change hour/minute,
- press a T9 digit with timestamp,
- toggle T9 language, mode, or case,
- backspace,
- save or cancel the editor,
- confirm or reject discard,
- request or confirm deletion,
- retry or close an operation error,
- acknowledge a reminder.

An action that is invalid for the current state is ignored and produces
no state or persistence side effect.

## Error Handling

The required behavior is:

| Failure | UI behavior |
|---|---|
| Agenda load failure | Blocking startup error with Retry |
| Missing or invalid clock | Manual clock setup |
| Clock set failure | Stay in clock setup with error |
| Invalid or past event | Keep draft and show validation error |
| Agenda full | Keep draft and show agenda-full error |
| Create/update save failure | Keep full draft; Retry or Cancel error |
| Delete save failure | Keep details; Retry or Cancel error |
| Reminder acknowledgement failure | Keep reminder open; OK retries |
| Invalid action for screen | Ignore with no side effect |
| Missing selected event | Return safely to the selected day |

All domain mutations remain transactional because they use the existing
`AgendaController` operations.

## Testing Strategy

Tests use real `CalendarEngine`, `T9Engine`, `AgendaManager`, reminder
logic, and `AgendaController` behavior. Test doubles are limited to the
storage and clock boundaries.

The UI suite covers:

- startup with valid, unavailable, invalid, and failing clock results,
- agenda-load failure and successful retry,
- manual clock validation and set failure,
- month navigation and supported-range boundaries,
- neighbouring-month date selection,
- today, selected, and event-dot flags,
- day event ordering, empty state, and pagination,
- page clamping after deletion,
- details, edit, and delete-confirmation flow,
- every create/update editor transition,
- prefilled create and update drafts,
- all-day behavior and five-minute time changes,
- T9 digit, language, mode, case, space, backspace, timeout, and limit
  integration,
- dirty-state and discard-confirmation behavior,
- successful create/update and return to month,
- validation errors without losing the draft,
- storage failures, retry, and exact draft preservation,
- invalid actions with no side effects,
- reminder deferral during editor and blocking overlays,
- immediate one-at-a-time reminder progression,
- acknowledgement persistence failure and retry,
- view-model ownership and absence of mutable agenda pointers.

Every new behavior follows red-green-refactor. Builds use C++17 with
`-Wall -Wextra -pedantic`, and the complete existing regression suite is
run before the stage is considered complete.

## Delivery Roadmap

The complete device remains a sequence of explicit stages:

1. Hardware-independent UI state machine and tests.
2. Renderer contract and 240-by-240 round-screen layouts.
3. Touch-region mapping and CST816S calibration.
4. ESP32 internal-clock adapter and manual clock persistence behavior.
5. LittleFS storage adapter and full device startup integration.
6. Application loop, reminder polling, and sleep/wake behavior.
7. Battery optimization, power-loss tests, and physical-device QA.

Completing stage 1 is a checkpoint, not the end of the product. Work
continues through the remaining stages until the diary is usable on the
physical device.
