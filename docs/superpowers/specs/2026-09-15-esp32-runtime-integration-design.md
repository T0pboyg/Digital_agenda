# ESP32 Runtime Integration Design

Status: Approved
Date: 2026-09-15

## Goal

Replace the temporary ESP32 display smoke screen with the real Digital Diary runtime. The integrated device must load saved events, obtain or request the current date and time, render the agenda UI, accept touchscreen actions, persist changes, and check reminders while remaining fully offline.

## Scope

This milestone includes:

- CST816S touchscreen access over I2C.
- Touch press/release tracking and conversion to `AgendaUiAction`.
- Composition of the existing repository, controller, state machine, renderer, clock, and display surface.
- Primary and backup LittleFS files.
- A clock-setup default derived from firmware compilation date and time.
- Host-side unit tests, a PlatformIO build, and USB-powered device acceptance tests.

This milestone does not include:

- Display sleep or wake-on-touch.
- Battery voltage display or low-battery handling.
- Physical battery connection.
- Wi-Fi, NTP, Bluetooth, or any other online service.

Power management and battery integration form the next and final hardware milestone. The battery remains disconnected until connector polarity and charging compatibility are physically verified.

## Selected Approach

Use small, testable components and a thin `main.cpp`.

The alternatives were rejected as follows:

- Putting all logic in `main.cpp` would make hardware failures and UI behavior difficult to isolate and test.
- Replacing the current UI with LVGL or a third-party CST816S stack would add dependencies and discard much of the tested renderer and touch-mapper work.

## Architecture

The input flow is:

`CST816S -> Cst816sTouchHardware -> TouchReleaseDetector -> AgendaTouchMapper -> AgendaUiStateMachine`

The persistence and time flow is:

`LittleFsByteStorage instances -> AgendaRepository -> AgendaController -> AgendaUiStateMachine <- Esp32Clock`

The output flow is:

`AgendaUiStateMachine::view() -> AgendaRenderer -> ArduinoGfxSurface -> DisplayHardware`

`AgendaRuntime` coordinates those flows. `main.cpp` owns the concrete objects, performs boot-time initialization, and calls the runtime from Arduino `loop()`.

## Components

### Cst816sTouchHardware

Add an ESP32 adapter under `src/platform/esp32/`.

- Use `Wire` with SDA GPIO6 and SCL GPIO7.
- Reset the controller through GPIO13.
- Use the CST816S I2C address `0x15`.
- Read five bytes beginning at touch-data register `0x02`.
- Reuse `decodeCst816sSample()` for finger count and coordinates.
- Return an explicit success/error result so an I2C failure cannot be mistaken for finger release.
- Initially use identity coordinate mapping for display rotation 0 and accept only `x` and `y` values from 0 through 239.

The board's TP_INT signal is GPIO5. Add the pin constant to `board_pins.h`, but do not use interrupts during this always-on milestone. GPIO5 is reserved for the later wake-on-touch implementation.

### TouchReleaseDetector

Add a platform-independent component under `src/ui/`.

- An untouched sample while idle produces no point.
- A touched sample stores its latest valid display coordinate.
- The first untouched sample after a valid touch emits the last valid point exactly once.
- Continued untouched samples do not repeat the event.
- Invalid coordinates are ignored.
- Touch read errors are not passed to the detector and therefore do not create false releases.

### Compile Date and Time

Add a small platform-independent parser for the `__DATE__` and `__TIME__` strings.

- Produce a valid `DateTime` used as `clockSetupDefault`.
- Support the space-padded single-digit day produced by `__DATE__`.
- If parsing unexpectedly fails, use `2026-01-01 12:00` as a safe fallback.
- This value is only the starting value for the existing `ClockSetup` screen; it is not silently accepted as the real current time.

### LittleFsByteStorage Paths

Add `explicit LittleFsByteStorage(std::string path)` for production use. It delegates to the existing default Arduino LittleFS API.

Create and initialize two instances:

- Primary: `/agenda.bin`
- Backup: `/agenda.backup.bin`

Both use `LittleFS.begin(false)`. Automatic formatting remains forbidden. The existing atomic temporary-file write behavior is retained.

Construct `AgendaRepository` with both instances. Before saving a new valid state, the repository keeps the previous valid primary state in the backup file. If the primary file is missing, unreadable, or invalid, the repository loads a valid backup and restores the primary file.

### AgendaRuntime

Add a coordinator that receives references to the touch hardware, release detector, touch mapper, state machine, renderer, and draw surface.

- Poll touch approximately every 10 ms.
- On release, call `AgendaTouchMapper::map()` with the current view, the release point, and the current `millis()` value.
- If an action is returned, pass it to `AgendaUiStateMachine::handle()` and render the resulting view immediately.
- Call `AgendaUiStateMachine::tick()` every 60,000 ms and render the resulting view. This gives reminder and date changes a maximum delay of one minute without continuously redrawing the screen.
- Use unsigned elapsed-time subtraction so scheduling remains correct across `millis()` wraparound.

The controller and repository remain solely responsible for event persistence. The runtime and `main.cpp` do not encode or write agenda state directly.

### main.cpp and PlatformIO

Replace `drawSmokeScreen()` with dependency composition and boot control.

Update `platformio.ini` so the ESP32 firmware compiles:

- root-level `touch_sample.cpp`,
- `core/*.cpp`,
- `ui/*.cpp`, and
- `platform/esp32/*.cpp`.

Desktop-only platform sources remain excluded.

## Boot Sequence

Arduino `setup()` performs these actions in order:

1. Start Serial at 115200 baud.
2. Initialize the display while leaving the backlight off.
3. Mount both LittleFS storage instances without formatting.
4. Initialize and reset the CST816S touch controller.
5. Start `AgendaUiStateMachine`, which loads the agenda through the controller and repository.
6. If the system clock is invalid, let the existing state machine enter `ClockSetup` using the compilation timestamp as its initial draft.
7. Render the first real agenda view.
8. Enable the display backlight.

Arduino `loop()` supplies `millis()` to `AgendaRuntime`, which performs non-blocking touch polling and periodic UI ticks. A short delay may be used to avoid a busy loop, but no long blocking delay is allowed.

## Error Handling

- Display initialization failure: keep the backlight off, write `DISPLAY_INIT_FAILED` to Serial, and do not start the runtime.
- LittleFS mount failure: never format automatically; render a dedicated storage error screen, write `STORAGE_INIT_FAILED` to Serial, and require a restart.
- Touch initialization failure: render a dedicated touch error screen, write `TOUCH_INIT_FAILED` to Serial, and do not start the runtime.
- Transient touch I2C read failure: retain the detector's current state, emit no release, and retry during the next polling cycle. Serial reporting must be throttled to avoid flooding.
- Out-of-range touch coordinates: ignore the sample.
- Invalid primary agenda data: use existing repository backup recovery.
- Invalid primary and backup data: expose the existing operation-error flow without deleting either file.
- Clock unavailable or invalid: use the existing `ClockSetup` and `ClockError` flows.
- Event write or domain validation failure: use the existing controller/state-machine error and retry flows.

## Test Strategy

Development follows RED-GREEN TDD. Tests are introduced in batches of two or three closely related cases, as requested.

### Batch 1: Compilation Timestamp

- Parse a normal date and time.
- Parse a space-padded single-digit day.
- Return the safe fallback for invalid input.

### Batch 2: Touch Release Detection

- Emit the final position for a press/move/release sequence.
- Emit only once for a release.
- Reject out-of-range samples without creating an action.

### Batch 3: CST816S Adapter

- Perform the required initialization and reset operations.
- Read and decode exactly five touch-data bytes.
- Report I2C failure without returning a false untouched sample.

### Batch 4: Storage Paths and Runtime Timing

- Construct production LittleFS storage with distinct primary and backup paths.
- Convert one release into at most one handled UI action and redraw.
- Trigger periodic tick correctly, including across `millis()` wraparound.

After each batch, compile before running the executable so stale test binaries cannot hide changes.

Before device upload, run all existing desktop suites, including date/time, calendar, persistence, state machine, renderer, touch mapper, draw-surface math, ESP32 clock, and LittleFS storage tests. Then run a clean PlatformIO firmware build.

## USB-Powered Device Acceptance

Keep the battery disconnected. Upload and test through USB only.

The milestone is accepted when all of the following are demonstrated on the physical board:

- The real agenda UI replaces the smoke screen.
- Touch targets near the top, center, bottom, left, and right respond to the matching controls.
- The clock can be configured from its compilation-time default.
- A timed event containing Greek text and digits can be created and viewed.
- An all-day event can be created and viewed.
- Events survive a full restart and USB disconnect/reconnect.
- Existing events can be edited and deleted.
- A due reminder appears and can be acknowledged without deleting its event.
- Serial output contains no repeating initialization or I2C failure messages during normal use.

If physical touch coordinates are inverted or rotated, adjust only the touch adapter's coordinate conversion and add a regression test; do not change UI geometry or touch-action bounds.

## Definition of Done

This runtime-integration milestone is done only when all new and existing desktop tests pass, the PlatformIO build succeeds, and every USB-powered device-acceptance item above has been observed. Battery connection, battery monitoring, and sleep/wake remain explicitly outside this milestone and must be completed before the overall Digital Diary project is declared finished.
