# Alarm and Battery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Complete the agreed alarm and visible battery features on the user's existing ESP32 diary.
**Architecture:** Separate portable AlarmService and battery estimator; integrate via optional state-machine/service references, shared UI geometry and earliest-wake planning. ESP32 adapters own ADC/I2S; existing agenda persistence remains untouched.
**Tech Stack:** C++17, Arduino ESP32 3.3.9, PlatformIO, LittleFS, host g++ tests.
**Spec:** docs/superpowers/specs/2026-10-02-alarm-battery-design.md

## Global Constraints
24 alarms maximum, 3/page. One-time or daily. No snooze or titles. Only Διακοπή during ringing. Separate storage. Existing agenda/partitions and timezone retained.

## Review Focus
- Reset while ringing restores pending alarm; stop write failure stops sound but remains visibly retryable and prevents sleep.
- Alarm preempts an agenda reminder or unsaved editor and returns to it after stop.
- Simultaneous alarms stop as one group, daily cannot duplicate in repeated DST hour.
- Earliest wake and spring DST gap cannot cause an immediate sleep loop.
- Invalid ADC shows unknown; under-load percentage remains explicitly approximate.

### Task 1: Alarm persistence, scheduling and battery model
Files: src/core/alarm_service.{h,cpp}, battery_status.{h,cpp}; tests/test_alarm_service.cpp, test_battery_status.cpp; tools/run_host_tests.py.
Interfaces: AlarmService(ByteStorage&), load(), save(Alarm, DateTime), remove(id), poll(now), stop(now), nextWake(now), alarms(), ringing(), error(). batteryPercent(millivolts).
- [ ] Write tests for persistence/CRC, daily/one-time/simultaneous alarms, stop/reboot, failed writes, date limits; run to see missing API failure.
- [ ] Implement typed error, validated versioned encoding, pending flags and scheduling, voltage interpolation.
- [ ] Run targeted tests and baseline suite; commit.

### Task 2: Alarm screens, status and power integration
Files: core/agenda_ui_*; ui/alarm_ui.{h,cpp}, alarm_renderer.{h,cpp}, agenda_renderer.cpp, agenda_touch_mapper.cpp, agenda_runtime.*, agenda_power_coordinator.*, agenda_power_manager.cpp, ui_geometry.h.
Interfaces: optional AlarmService* in state/power constructors; state fast tick interval only when alarms attached. AlarmUi encapsulates drafts, paging, actions. view carries alarm UI, now and optional battery.
- [ ] Write tests for UI add/edit/delete/toggle, ringing-only stop, editor preservation, earliest wake, disabled alarms and errors; run red.
- [ ] Implement integrated UI and wake scheduling; keep no-service API behavior for baseline tests.
- [ ] Run host suite and commit.

### Task 3: ESP32 adapters, integration and delivery
Files: platform/esp32/battery_hardware.*, alarm_audio.*, board_pins.h, main.cpp; docs/ALARM_BATTERY.md.
- [ ] Test portable PCM generator and ADC conversion; run red.
- [ ] Implement 16kHz stereo duplicated tone melody, bounded I2S writes, stop/deinit, ADC averaging and setup/loop/wake hooks.
- [ ] Build host suite and attempt pinned PlatformIO firmware build; document any unavailable hardware/toolchain checks.
- [ ] Fresh whole-branch code review, fix important findings with regression tests.
- [ ] Commit and deliver git bundle + source archive + Greek install/test instructions without flashing user hardware.
