# ESP32-S3 Hybrid Power Management Design

Date: 2026-09-16
Status: Approved

## Summary

The digital diary will add hybrid power management before the battery is connected. The implementation will be developed and validated while the Waveshare ESP32-S3-Touch-LCD-1.28 Rev3 is powered through USB.

The user-visible policy is:

- after 45 seconds without valid touch activity, turn the display backlight off and use light sleep;
- after 5 minutes without valid touch activity, enter deep sleep only from a state that has no unsaved or transient UI data;
- wake from touch or from the next scheduled reminder;
- consume the touch that wakes the display so it cannot activate a UI control accidentally;
- keep a due reminder visible with the backlight on until the user acknowledges it;
- never sacrifice stored events or unsaved editor text to save power.

This feature is a new subsystem because it coordinates the runtime, touch input, display, clock, reminder engine, persistent storage, and ESP32 sleep APIs.

## Context

The existing firmware already provides:

- a platform-independent agenda core;
- monthly and daily calendar views;
- event creation, editing, deletion, and persistence;
- timed and all-day events;
- reminder acknowledgement;
- a CST816S touch adapter with release detection;
- a GC9A01 display adapter and controllable backlight;
- an ESP32 system clock configured for Greek local time;
- LittleFS primary and backup storage without automatic formatting;
- an `AgendaRuntime` that polls touch and ticks the UI;
- successful desktop tests, PlatformIO builds, USB uploads, and physical UI acceptance.

The remaining battery-dependent work is actual current, charging, endurance, and low-voltage testing. Those measurements are outside this design.

## Goals

1. Reduce idle power consumption without changing normal agenda behavior.
2. Preserve unsaved editor and setup state by using light sleep when deep sleep is unsafe.
3. Use deep sleep when the current UI can be reconstructed from persistent data.
4. Wake on a deliberate screen touch.
5. Wake at the next reminder without periodic one-minute reboot cycles.
6. Prevent the wake touch from being interpreted as a UI action.
7. Fail open: if safe wake configuration cannot be guaranteed, keep the device awake and usable.
8. Keep ESP32 and Arduino dependencies outside `src/core`.
9. Make policy and coordination behavior deterministic and host-testable.

## Non-goals

- Battery percentage or voltage display.
- Charging-state UI.
- Measuring or promising battery life before the battery is connected.
- Adding Wi-Fi, Bluetooth, NTP, or any other network feature.
- Changing reminder semantics or event persistence formats.
- Saving incomplete editor drafts to LittleFS.
- Automatically formatting LittleFS.
- Adding an external RTC or changing hardware.
- Optimizing every external peripheral before the first correct sleep implementation.

## Chosen Approach

The chosen approach is hybrid light and deep sleep.

Light sleep preserves runtime memory and is therefore used after the display turns off and whenever the current UI contains transient data. Deep sleep provides lower consumption but restarts the application, so it is used only when the UI can be reconstructed safely from the clock and LittleFS.

The alternatives were rejected as follows:

- light sleep only preserves all state but gives materially worse idle consumption;
- deep sleep only gives lower consumption but risks losing unsaved editor or setup state and requires every screen to be serializable.

## Architecture

### AgendaPowerManager

`AgendaPowerManager` is platform-independent C++. It owns power-policy state and makes decisions from explicit inputs. It does not call Arduino, ESP-IDF, display, touch, clock, or storage APIs.

Its responsibilities are:

- track the timestamp of the last valid touch activity;
- use wrap-safe 32-bit elapsed-time arithmetic for millisecond inactivity;
- request display-off/light sleep after 45 seconds;
- request deep sleep after 5 minutes only when the current view is deep-sleep-safe;
- reset inactivity after a successful wake or acknowledged reminder;
- suspend automatic sleep while a reminder is waiting for acknowledgement;
- expose decisions rather than execute hardware operations.

The logical policy states are:

- `Active`: display on and normal runtime processing;
- `LightSleepEligible`: the backlight is off and transient RAM state must be preserved;
- `DeepSleepRequested`: the inactivity threshold passed in a reconstructible UI state;
- `ReminderLatched`: a due reminder is visible and automatic sleep is disabled until acknowledgement.

Names may be adjusted to existing project conventions during planning, but these responsibilities and transitions are fixed.

### AgendaRuntime integration

`AgendaRuntimeUpdateResult` will report whether valid touch activity occurred, separately from whether a mapped action was handled. A valid in-bounds touch counts as activity even if it does not map to a button. An I2C read error, invalid coordinate, or absent touch does not count.

The runtime or a small coordinator above it will:

- pass valid activity to `AgendaPowerManager`;
- reset `TouchReleaseDetector` when a wake touch must be consumed;
- prevent any release belonging to the wake gesture from reaching `AgendaTouchMapper`;
- supply the current `AgendaUiView` classification to the power policy;
- redraw the current screen after light-sleep wake;
- start the normal state machine after deep-sleep wake.

The existing state machine remains responsible for agenda behavior. Power management must not duplicate event editing, persistence, or reminder acknowledgement logic.

### Deep-sleep-safe views

Deep sleep is allowed only when the visible state can be reconstructed without losing user input.

Safe states include:

- monthly calendar;
- daily event list;
- viewing an already saved event.

Deep sleep is blocked while any of the following is active:

- date or time editor for a new or modified event;
- T9 text entry with unsaved text;
- clock setup or clock error recovery;
- delete, discard, retry, or other pending confirmation;
- operation error with pending recovery state;
- an unacknowledged reminder.

Blocked states may still turn the backlight off and enter light sleep, because light sleep preserves RAM.

### Esp32PowerHardware

`Esp32PowerHardware` is the thin ESP32-specific adapter. It wraps ESP-IDF and board operations behind injectable function pointers or an equivalent testable API pattern already used by the project.

Its responsibilities are:

- report cold boot, touch wake, timer wake, and unknown wake causes;
- configure GPIO5, the existing CST816S interrupt pin, as a wake source;
- configure an optional RTC timer wake duration;
- enter light sleep;
- enter deep sleep;
- report configuration and sleep-entry errors;
- leave wake GPIOs in a state that the touch driver can reinitialize safely.

The adapter does not decide when sleeping is appropriate.

### Display and touch preparation

Before sleep:

- the backlight is disabled;
- pending serial diagnostics are flushed when necessary;
- the current touch gesture must be released;
- the CST816S interrupt level is checked before arming wake;
- the touch controller remains capable of generating the wake interrupt.

The required first implementation controls the existing backlight pin only. It will not send an unverified controller-specific sleep command to the GC9A01. A display-controller sleep command may be evaluated later as a measured battery optimization, but is not required for this feature.

After light-sleep wake:

- the wake gesture is consumed through release;
- any GPIO hold or RTC mode is released before normal GPIO use;
- the current UI is redrawn;
- the backlight is enabled.

After deep-sleep wake, normal hardware initialization runs again because deep sleep restarts the digital core.

### Reminder scheduling

The reminder engine will centralize calculation of an event's reminder time. Existing user-visible reminder rules remain unchanged.

A new platform-independent query will return one of:

- the earliest future unacknowledged reminder time;
- an indication that a reminder is already due;
- no pending reminder;
- an explicit invalid-input error.

The controller will expose this result without exposing mutable agenda storage. The power layer will convert a valid future `DateTime` into a 64-bit relative wake duration.

Reminder targets have minute precision, but the ESP32 system clock also provides the current seconds. The ESP32 adapter will convert the target local `DateTime` to an epoch value using the already configured Greek timezone and subtract the current system epoch including seconds. It will therefore target the beginning of the reminder minute rather than deriving a delay from the truncated minute-only UI clock. Conversion failure is an explicit scheduling error and blocks deep sleep.

Deep sleep will use one RTC timer for the next reminder rather than waking every minute. When there is no future reminder, only touch wake is armed.

The due-reminder check remains authoritative after timer wake. The timer never displays a reminder merely because it fired.

## Runtime Flows

### Normal activity

1. The device is active with the display on.
2. Any valid touch resets the inactivity origin.
3. Normal release detection, action mapping, state handling, rendering, and persistence continue unchanged.

### Entering light sleep

1. Forty-five seconds elapse without valid touch activity.
2. The runtime disables the backlight.
3. It verifies that the touch wake source can be armed.
4. It arms touch wake and a timer for the earlier of:
   - the five-minute deep-sleep threshold;
   - the next scheduled reminder.
5. It enters light sleep.

If wake configuration fails, the device does not sleep.

### Waking light sleep by touch

1. The CST816S interrupt wakes the ESP32-S3.
2. The screen is restored and redrawn.
3. The backlight is enabled.
4. The current touch is consumed until release.
5. The inactivity timer is reset.
6. The next independent touch can create a UI action.

### Reaching five minutes

1. The five-minute point wakes or is observed by the coordinator.
2. If the current UI is not deep-sleep-safe, the device remains in light sleep.
3. If the current UI is safe, the clock and next reminder are read.
4. Touch wake and, when applicable, the next reminder timer are armed.
5. The device enters deep sleep.

### Waking deep sleep by touch

1. Startup identifies touch as the wake cause.
2. LCD, touch, clock, and LittleFS are initialized using existing safe startup rules.
3. Events are loaded from primary or backup storage.
4. The monthly calendar is rendered.
5. The backlight is enabled.
6. The wake gesture is consumed so it cannot trigger a calendar action.

### Waking deep sleep by timer

1. Startup identifies the timer as the wake cause and initially keeps the backlight off.
2. Storage and clock are initialized.
3. The normal due-reminder check runs.
4. If a reminder is due, it is rendered, the backlight is enabled, and sleep remains disabled until acknowledgement.
5. If no reminder is due, the next wake time is recalculated and the device returns to sleep without flashing the backlight.

An early timer wake caused by RTC tolerance therefore does not produce a false reminder.

### Reminder acknowledgement

1. The reminder remains visible with the backlight on until the user presses `OK`.
2. The existing controller persists acknowledgement.
3. Only after successful persistence is automatic sleep re-enabled.
4. The inactivity timer starts again from the acknowledgement.

## Startup Rules

- Cold boot retains the existing startup and clock-setup behavior.
- Touch wake returns to the monthly calendar rather than attempting to restore a transient deep-sleep UI screen.
- Timer wake displays only an authoritative due reminder.
- Unknown wake cause is treated as a normal cold boot.
- LittleFS is never formatted automatically.
- The backlight remains off until initialization has succeeded and there is a screen the user should see.

## Error Handling

### Wake-source configuration failure

If touch wake cannot be configured, neither light nor deep sleep is entered. The firmware restores the backlight, logs `TOUCH_WAKE_CONFIG_FAILED`, and remains usable. It must never enter an indefinite sleep that lacks a verified wake source.

### Touch already active

The firmware does not arm sleep while the interrupt is already active or a touch gesture is in progress. It waits for release and resets release detection. This prevents rapid sleep/wake loops.

### Clock failure

If the clock is invalid, unavailable, or unreadable:

- no reminder timer is scheduled;
- deep sleep is blocked;
- the existing `ClockSetup` or `ClockError` flow remains visible;
- light sleep is allowed only when wake by touch is verified and RAM state is preserved.

### Reminder schedule failure

If next-reminder calculation fails, deep sleep is blocked and the error is logged. This avoids sleeping past a reminder because of invalid data.

### Unexpected sleep return

If light sleep reports an error, the runtime restores the display, enables the backlight, logs `LIGHT_SLEEP_FAILED`, and remains active. If a deep-sleep call returns instead of restarting the device, the runtime performs the same recovery and logs `DEEP_SLEEP_FAILED`.

### Startup failures after deep sleep

Existing safe startup behavior remains authoritative:

- LCD failure keeps the backlight off and logs the display failure;
- touch failure reports the existing touch initialization error;
- LittleFS failure shows the storage error without formatting;
- corrupt primary agenda recovery uses the existing backup flow.

### Time calculations

- Millisecond inactivity uses unsigned subtraction and remains correct across `millis()` wrap.
- Date/time differences and microsecond wake intervals use checked 64-bit arithmetic.
- Reminder timer conversion uses current epoch seconds and the target local minute, including daylight-saving rules already configured by `Esp32Clock`.
- Non-positive reminder intervals are treated as due-now checks, not passed as invalid sleep durations.

## Hardware Feasibility Gates

Two USB-powered physical probes must pass before production deep sleep is enabled:

1. GPIO5/CST816S must wake this exact Waveshare Rev3 board reliably from deep sleep across repeated trials.
2. The configured local system time must remain valid and advance correctly across deep sleep.

The probes are part of implementation, not optional acceptance checks. If touch deep-wake fails, the safe fallback is light sleep while the design is revisited. If time continuity fails, reminder-driven deep sleep remains disabled. No workaround may risk an unwakeable device or missed reminders.

## Test Strategy

Development follows RED-GREEN-REFACTOR in batches of two or three tests.

### Reminder scheduling tests

- return the earliest future unacknowledged reminder;
- ignore acknowledged and past events;
- return due-now, none, and invalid-time results explicitly;
- preserve existing reminder timing semantics.

### Power-policy tests

- remain active before 45 seconds;
- request display-off/light sleep at 45 seconds;
- request deep sleep at 5 minutes in safe views;
- block deep sleep in editor, setup, confirmation, error-recovery, and reminder states;
- reset inactivity on valid touch;
- remain correct across 32-bit millisecond wrap.

### Wake-gesture tests

- a wake touch produces no mapped agenda action;
- the gesture remains suppressed until release;
- the following independent touch works normally;
- I2C failure and invalid coordinates do not reset inactivity.

### ESP32 adapter host tests

- configure the verified touch wake pin and requested level;
- configure and omit timer wake correctly;
- decode cold, touch, timer, and unknown wake causes;
- propagate configuration failures;
- never call a sleep-start function after failed wake configuration.

### Integration tests

- light sleep preserves an unsaved editor draft;
- deep-sleep touch startup reconstructs the monthly view from stored events;
- timer startup shows a due reminder and latches the display on;
- early timer startup returns to sleep without enabling the backlight;
- successful acknowledgement persists before sleep is re-enabled.

### Regression verification

- compile and execute every desktop test executable;
- build the ESP32 firmware with PlatformIO from a clean state;
- keep the core platform-dependency scan clean;
- keep the no-network and no-automatic-LittleFS-format checks clean;
- run `git diff --check` before each commit.

## USB-Powered Device Acceptance

Before connecting the battery, the following must pass on the physical Rev3 board:

1. At 45 seconds of inactivity, the backlight turns off.
2. The first touch wakes the display but activates no UI control.
3. A second independent touch works normally.
4. From the monthly view, five minutes of inactivity enters verified deep sleep.
5. Touch wakes deep sleep and returns to the monthly view.
6. Stored events remain intact after repeated sleep/wake cycles.
7. The clock remains valid and advances across deep sleep.
8. While editing unsaved text, five minutes does not enter deep sleep.
9. Light-sleep wake restores the unsaved editor text.
10. A timer wake that is not yet due does not flash the display or show a false reminder.
11. A due reminder turns on the display and remains visible until `OK` is pressed.
12. Serial diagnostics distinguish `LIGHT_SLEEP`, `DEEP_SLEEP`, `WAKE_TOUCH`, `WAKE_TIMER`, and explicit failures.

Temporary USB serial disconnection during deep sleep is expected and is not itself a failure. Acceptance is based on wake behavior, restored UI, persistent data, and diagnostics after reconnection.

## Battery-Stage Work After This Feature

Once polarity and connector orientation have been verified with the multimeter, the battery stage will cover:

- initial supervised connection;
- charging behavior and temperature observation;
- active, light-sleep, and deep-sleep current measurement where practical;
- real-world runtime measurement;
- final timeout tuning if measured endurance requires it;
- enclosure installation without bending, compressing, puncturing, or permanently gluing the Li-Po cell.

## Completion Criteria

The power-management implementation is complete when:

- all new and existing host tests pass;
- a clean PlatformIO build succeeds;
- both hardware feasibility gates pass on the Rev3 board;
- every USB-powered device acceptance item passes;
- no event, acknowledgement, or unsaved editor text is lost in its allowed sleep mode;
- unsafe wake configuration always leaves the device awake and usable;
- the repository is clean after the implementation commits.
