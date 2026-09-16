# ESP32-S3 Hybrid Power Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add safe hybrid light/deep sleep to the Waveshare ESP32-S3-Touch-LCD-1.28 Rev3 diary while preserving events, unsaved editor state, clock correctness, wake-touch safety, and reminder behavior.

**Architecture:** A platform-independent AgendaPowerManager owns inactivity policy, and an AgendaPowerCoordinator combines that policy with the controller, clock, and next-reminder schedule. Esp32PowerHardware is a thin injectable ESP-IDF adapter; AgendaRuntime reports valid touch activity and suppresses wake gestures. Production deep sleep remains disabled until two USB-powered physical gates pass.

**Tech Stack:** C++17, desktop g++ host tests, Arduino/Pioarduino for ESP32-S3, ESP-IDF sleep APIs, Arduino_GFX, LittleFS, PowerShell, PlatformIO.

**Spec:** docs/superpowers/specs/2026-09-16-esp32-hybrid-power-management-design.md

## Global Constraints

- Start from commit fecad26 with a clean worktree; keep digital_diary-power-plan.bundle untracked and outside every commit.
- Keep the Li-Po battery disconnected throughout this plan; all device checks run from USB.
- Use 45,000 ms for light sleep and 300,000 ms for deep-sleep eligibility.
- Count only a successful, in-bounds touched sample as activity.
- Consume the complete touch gesture that wakes the display.
- Allow deep sleep only for Month, Day, or EventDetails with AgendaUiOverlay::None.
- Never sleep automatically while AgendaUiOverlay::Reminder is visible.
- Preserve editor/setup/confirmation/error state in RAM by using light sleep.
- Keep reminders at 17:00 on the previous date and keep persistence formats unchanged.
- Derive reminder wake delay from epoch seconds, not minute-only subtraction.
- Do not add Wi-Fi, Bluetooth, NTP, automatic LittleFS formatting, an external RTC, or a GC9A01 sleep command.
- Keep Arduino, ESP-IDF, Wire, LittleFS, and Arduino_GFX dependencies outside src/core.
- Keep production deep sleep disabled until GPIO5 touch wake and clock continuity pass repeated physical probes.
- Work in RED → GREEN batches of two or three tests, run git diff --check, and commit after each task.

## File Map

| File | Responsibility |
|---|---|
| src/core/reminder_engine.{h,cpp} | Earliest future/due-now reminder query |
| src/core/agenda_controller.{h,cpp} | Read-only reminder schedule facade |
| src/ui/power_hardware.h | Platform-neutral sleep/backlight/exact-delay interfaces |
| src/ui/agenda_power_manager.{h,cpp} | Wrap-safe inactivity policy and safe-view classification |
| src/ui/agenda_power_coordinator.{h,cpp} | Policy/reminder timer selection and fail-open behavior |
| src/ui/agenda_runtime.{h,cpp} | Valid touch activity and wake-gesture suppression |
| src/ui/touch_release_detector.{h,cpp} | Clear stale gesture state on wake |
| src/platform/esp32/esp32_clock.{h,cpp} | Exact epoch-based wake delay |
| src/platform/esp32/power_hardware.{h,cpp} | Injectable ESP-IDF adapter |
| src/platform/esp32/display_hardware.h | Platform-neutral backlight implementation |
| src/platform/esp32/main.cpp | Runtime composition and staged physical gate |
| tests/test_agenda_power_*.cpp | Pure policy/coordinator regression |
| tests/test_esp32_power_hardware.cpp | Adapter failure gating |

---

### Task 1: Centralize reminder scheduling

**Files:**
- Modify: src/core/reminder_engine.h
- Modify: src/core/reminder_engine.cpp
- Modify: src/core/agenda_controller.h
- Modify: src/core/agenda_controller.cpp
- Modify: tests/test_reminder_engine.cpp
- Modify: tests/test_agenda_controller.cpp

**Interfaces:**
- Consumes: AgendaManager::events(), isPast(), previousDate(), and serialMinute().
- Produces: ReminderScheduleResult findNextReminderSchedule(...) and AgendaController::reminderSchedule(...).

- [ ] **Step 1: Add three reminder-engine schedule tests**

Apply this exact diff:

```diff
diff --git a/tests/test_reminder_engine.cpp b/tests/test_reminder_engine.cpp
index 61eab25..b420dd2 100644
--- a/tests/test_reminder_engine.cpp
+++ b/tests/test_reminder_engine.cpp
@@ -433,6 +433,93 @@ void test_invalid_current_date_or_time_is_rejected()

     assert(!invalidTime.eventId.has_value());
 }
+
+void test_schedule_returns_earliest_future_unseen_reminder()
+{
+    const AgendaManager agenda =
+        reminderAgenda({
+            reminderEvent(
+                60,
+                Date{2026, 9, 19},
+                std::nullopt),
+            reminderEvent(
+                61,
+                Date{2026, 9, 18},
+                TimeOfDay{9, 0}),
+            reminderEvent(
+                62,
+                Date{2026, 9, 17},
+                std::nullopt,
+                true)
+        }, 63);
+
+    const ReminderScheduleResult result =
+        findNextReminderSchedule(
+            agenda,
+            DateTime{
+                Date{2026, 9, 16},
+                TimeOfDay{10, 0}});
+
+    assert(result.error == ReminderEngineError::None);
+    assert(!result.dueNow);
+    assert(result.scheduledAt.has_value());
+    assert(result.scheduledAt->date.year == 2026);
+    assert(result.scheduledAt->date.month == 9);
+    assert(result.scheduledAt->date.day == 17);
+    assert(result.scheduledAt->time.hour == 17);
+    assert(result.scheduledAt->time.minute == 0);
+}
+
+void test_schedule_reports_due_now_without_future_target()
+{
+    const AgendaManager agenda =
+        reminderAgenda({
+            reminderEvent(
+                70,
+                Date{2026, 9, 17},
+                std::nullopt)
+        }, 71);
+
+    const ReminderScheduleResult result =
+        findNextReminderSchedule(
+            agenda,
+            DateTime{
+                Date{2026, 9, 16},
+                TimeOfDay{17, 0}});
+
+    assert(result.error == ReminderEngineError::None);
+    assert(result.dueNow);
+    assert(!result.scheduledAt.has_value());
+}
+
+void test_schedule_reports_none_and_invalid_time_explicitly()
+{
+    const AgendaManager empty;
+
+    const ReminderScheduleResult none =
+        findNextReminderSchedule(
+            empty,
+            DateTime{
+                Date{2026, 9, 16},
+                TimeOfDay{10, 0}});
+
+    assert(none.error == ReminderEngineError::None);
+    assert(!none.dueNow);
+    assert(!none.scheduledAt.has_value());
+
+    const ReminderScheduleResult invalid =
+        findNextReminderSchedule(
+            empty,
+            DateTime{
+                Date{2026, 9, 16},
+                TimeOfDay{24, 0}});
+
+    assert(
+        invalid.error ==
+        ReminderEngineError::InvalidDateTime);
+    assert(!invalid.dueNow);
+    assert(!invalid.scheduledAt.has_value());
+}
 }

 int main()
@@ -450,9 +537,12 @@ int main()
     test_month_year_and_leap_boundaries();
     test_first_supported_date_uses_numeric_threshold();
     test_invalid_current_date_or_time_is_rejected();
+    test_schedule_returns_earliest_future_unseen_reminder();
+    test_schedule_reports_due_now_without_future_target();
+    test_schedule_reports_none_and_invalid_time_explicitly();

     std::cout
         << "All reminder engine tests passed.\n";

     return 0;
-}
\ No newline at end of file
+}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
New-Item -ItemType Directory -Force build | Out-Null
$engineSources = @('tests\test_reminder_engine.cpp','src\core\date_time.cpp','src\core\utf8.cpp','src\core\event.cpp','src\core\agenda_state.cpp','src\core\agenda_manager.cpp','src\core\reminder_engine.cpp')
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc @engineSources -o build\test_reminder_engine.exe
Write-Host "Reminder schedule RED exit code: $LASTEXITCODE"
```

Expected: compilation fails because ReminderScheduleResult and findNextReminderSchedule do not exist.

- [ ] **Step 3: Add the core schedule query**

Apply this exact diff:

```diff
diff --git a/src/core/reminder_engine.cpp b/src/core/reminder_engine.cpp
index b30d2ce..747976a 100644
--- a/src/core/reminder_engine.cpp
+++ b/src/core/reminder_engine.cpp
@@ -1,5 +1,19 @@
 #include "reminder_engine.h"

+namespace
+{
+    constexpr int ReminderHour = 17;
+
+    long long reminderMinuteFor(
+        const Event& event)
+    {
+        return (
+            static_cast<long long>(
+                serialDay(event.eventDate)) - 1LL
+        ) * 1440LL + ReminderHour * 60LL;
+    }
+}
+
 ReminderCheckResult findNextDueReminder(
     const AgendaManager& agenda,
     const DateTime& now
@@ -30,11 +44,7 @@ ReminderCheckResult findNextDueReminder(
         }

         const long long reminderMinute =
-            (
-                static_cast<long long>(
-                    serialDay(event.eventDate)
-                ) - 1LL
-            ) * 1440LL + 17LL * 60LL;
+            reminderMinuteFor(event);

         if (nowMinute >= reminderMinute)
         {
@@ -49,4 +59,81 @@ ReminderCheckResult findNextDueReminder(
         ReminderEngineError::None,
         std::nullopt
     };
-}
\ No newline at end of file
+}
+
+ReminderScheduleResult findNextReminderSchedule(
+    const AgendaManager& agenda,
+    const DateTime& now
+)
+{
+    if (
+        !isValidDate(now.date) ||
+        !isValidTime(now.time)
+    )
+    {
+        return ReminderScheduleResult{
+            ReminderEngineError::InvalidDateTime,
+            false,
+            std::nullopt
+        };
+    }
+
+    const long long nowMinute =
+        serialMinute(now);
+
+    std::optional<DateTime> earliest;
+    std::optional<long long> earliestMinute;
+
+    for (const Event& event : agenda.events())
+    {
+        if (
+            event.reminderSeen ||
+            isPast(event, now)
+        )
+        {
+            continue;
+        }
+
+        const long long reminderMinute =
+            reminderMinuteFor(event);
+
+        if (nowMinute >= reminderMinute)
+        {
+            return ReminderScheduleResult{
+                ReminderEngineError::None,
+                true,
+                std::nullopt
+            };
+        }
+
+        const std::optional<Date> reminderDate =
+            previousDate(event.eventDate);
+
+        if (!reminderDate.has_value())
+        {
+            return ReminderScheduleResult{
+                ReminderEngineError::InvalidDateTime,
+                false,
+                std::nullopt
+            };
+        }
+
+        if (
+            !earliestMinute.has_value() ||
+            reminderMinute < *earliestMinute
+        )
+        {
+            earliestMinute = reminderMinute;
+            earliest = DateTime{
+                *reminderDate,
+                TimeOfDay{ReminderHour, 0}
+            };
+        }
+    }
+
+    return ReminderScheduleResult{
+        ReminderEngineError::None,
+        false,
+        earliest
+    };
+}
diff --git a/src/core/reminder_engine.h b/src/core/reminder_engine.h
index 17ea865..5690b17 100644
--- a/src/core/reminder_engine.h
+++ b/src/core/reminder_engine.h
@@ -17,7 +17,19 @@ struct ReminderCheckResult
     std::optional<std::uint32_t> eventId;
 };

+struct ReminderScheduleResult
+{
+    ReminderEngineError error;
+    bool dueNow;
+    std::optional<DateTime> scheduledAt;
+};
+
 ReminderCheckResult findNextDueReminder(
     const AgendaManager& agenda,
     const DateTime& now
-);
\ No newline at end of file
+);
+
+ReminderScheduleResult findNextReminderSchedule(
+    const AgendaManager& agenda,
+    const DateTime& now
+);
```

The query skips acknowledged/past events, reports due-now explicitly, and returns the earliest future previous-day 17:00 target.

- [ ] **Step 4: Compile and run GREEN**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc @engineSources -o build\test_reminder_engine.exe
if ($LASTEXITCODE -ne 0) { throw 'Reminder engine compile failed.' }
.\build\test_reminder_engine.exe
if ($LASTEXITCODE -ne 0) { throw 'Reminder engine tests failed.' }
```

- [ ] **Step 5: Add the two controller facade tests**

Apply this exact diff:

```diff
diff --git a/tests/test_agenda_controller.cpp b/tests/test_agenda_controller.cpp
index 875a59d..1c60b77 100644
--- a/tests/test_agenda_controller.cpp
+++ b/tests/test_agenda_controller.cpp
@@ -1489,6 +1489,47 @@ void test_due_reminders_advance_one_at_a_time()
     assert(storage.writeCount == 2);
 }

+void test_reminder_schedule_requires_ready_controller()
+{
+    TestByteStorage storage;
+    AgendaRepository repository(storage);
+    AgendaController controller(repository);
+
+    const AgendaControllerReminderScheduleResult result =
+        controller.reminderSchedule(
+            DateTime{
+                Date{2026, 9, 16},
+                TimeOfDay{10, 0}});
+
+    assert(result.error == AgendaControllerError::NotReady);
+    assert(result.reminderError == ReminderEngineError::None);
+    assert(!result.dueNow);
+    assert(!result.scheduledAt.has_value());
+}
+
+void test_controller_exposes_reminder_schedule_without_writing()
+{
+    TestByteStorage storage;
+    AgendaRepository repository(storage);
+    AgendaController controller(repository);
+
+    assert(
+        controller.start().error ==
+        AgendaControllerError::None);
+
+    const AgendaControllerReminderScheduleResult result =
+        controller.reminderSchedule(
+            DateTime{
+                Date{2026, 9, 16},
+                TimeOfDay{10, 0}});
+
+    assert(result.error == AgendaControllerError::None);
+    assert(result.reminderError == ReminderEngineError::None);
+    assert(!result.dueNow);
+    assert(!result.scheduledAt.has_value());
+    assert(storage.writeCount == 0);
+}
+
 int main()
 {
     test_new_controller_is_not_ready();
@@ -1523,6 +1564,8 @@ int main()
     test_acknowledgement_write_failure_restores_exact_state();
     test_acknowledgement_read_failure_restores_state();
     test_due_reminders_advance_one_at_a_time();
+    test_reminder_schedule_requires_ready_controller();
+    test_controller_exposes_reminder_schedule_without_writing();

     std::cout
         << "All agenda controller tests passed.\n";
```

- [ ] **Step 6: Compile and confirm RED**

```powershell
$controllerSources = @('tests\test_agenda_controller.cpp','src\core\date_time.cpp','src\core\utf8.cpp','src\core\event.cpp','src\core\agenda_state.cpp','src\core\agenda_manager.cpp','src\core\crc32.cpp','src\core\event_codec.cpp','src\core\agenda_repository.cpp','src\core\reminder_engine.cpp','src\core\agenda_controller.cpp')
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc @controllerSources -o build\test_agenda_controller.exe
Write-Host "Controller schedule RED exit code: $LASTEXITCODE"
```

Expected: compilation fails because AgendaControllerReminderScheduleResult and reminderSchedule do not exist.

- [ ] **Step 7: Add the controller facade**

Apply this exact diff:

```diff
diff --git a/src/core/agenda_controller.cpp b/src/core/agenda_controller.cpp
index 355b672..ed2f729 100644
--- a/src/core/agenda_controller.cpp
+++ b/src/core/agenda_controller.cpp
@@ -136,6 +136,32 @@ AgendaController::nextReminder(
     };
 }

+AgendaControllerReminderScheduleResult
+AgendaController::reminderSchedule(
+    const DateTime& now
+) const
+{
+    if (!ready_)
+    {
+        return AgendaControllerReminderScheduleResult{
+            AgendaControllerError::NotReady,
+            ReminderEngineError::None,
+            false,
+            std::nullopt
+        };
+    }
+
+    const ReminderScheduleResult result =
+        findNextReminderSchedule(agenda_, now);
+
+    return AgendaControllerReminderScheduleResult{
+        AgendaControllerError::None,
+        result.error,
+        result.dueNow,
+        result.scheduledAt
+    };
+}
+
 AgendaControllerCreateResult
 AgendaController::createEvent(
     const EventDraft& draft,
diff --git a/src/core/agenda_controller.h b/src/core/agenda_controller.h
index 58bb961..416899d 100644
--- a/src/core/agenda_controller.h
+++ b/src/core/agenda_controller.h
@@ -42,6 +42,14 @@ struct AgendaControllerReminderResult
     std::optional<Event> event;
 };

+struct AgendaControllerReminderScheduleResult
+{
+    AgendaControllerError error;
+    ReminderEngineError reminderError;
+    bool dueNow;
+    std::optional<DateTime> scheduledAt;
+};
+
 class AgendaController
 {
 public:
@@ -59,6 +67,10 @@ public:
         const DateTime& now
     ) const;

+    AgendaControllerReminderScheduleResult reminderSchedule(
+        const DateTime& now
+    ) const;
+
     AgendaControllerCreateResult createEvent(
         const EventDraft& draft,
         const DateTime& now
```

- [ ] **Step 8: Compile/run both suites and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc @controllerSources -o build\test_agenda_controller.exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda controller compile failed.' }
.\build\test_reminder_engine.exe
if ($LASTEXITCODE -ne 0) { throw 'Reminder engine tests failed.' }
.\build\test_agenda_controller.exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda controller tests failed.' }
git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Whitespace check failed.' }
git add -- src/core/reminder_engine.h src/core/reminder_engine.cpp src/core/agenda_controller.h src/core/agenda_controller.cpp tests/test_reminder_engine.cpp tests/test_agenda_controller.cpp
git diff --cached --check
git commit -m "feat: expose next reminder schedule"
git status --short
```


---

### Task 2: Add the pure inactivity policy

**Files:**
- Create: src/ui/agenda_power_manager.h
- Create: src/ui/agenda_power_manager.cpp
- Create: tests/test_agenda_power_manager.cpp

**Interfaces:**
- Consumes: AgendaUiView, AgendaUiScreen, AgendaUiOverlay.
- Produces: agendaViewAllowsDeepSleep(), AgendaPowerManager::begin(), recordActivity(), decide().

- [ ] **Step 1: Create the three-test policy suite**

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>

#include "ui/agenda_power_manager.h"

namespace
{
    AgendaUiView viewFor(
        AgendaUiScreen screen,
        AgendaUiOverlay overlay = AgendaUiOverlay::None
    )
    {
        AgendaUiView view;
        view.screen = screen;
        view.overlay = overlay;
        return view;
    }

    void test_policy_thresholds_and_safe_deep_sleep()
    {
        AgendaPowerManager manager;
        const AgendaUiView month =
            viewFor(AgendaUiScreen::Month);

        manager.begin(1000);

        assert(
            manager.decide(45999, month).action ==
            AgendaPowerAction::None);

        const AgendaPowerDecision light =
            manager.decide(46000, month);

        assert(
            light.action ==
            AgendaPowerAction::EnterLightSleep);
        assert(
            light.policyWakeAfterMilliseconds ==
            255000);

        assert(
            manager.decide(301000, month).action ==
            AgendaPowerAction::EnterDeepSleep);
    }

    void test_unsafe_and_reminder_views_never_request_deep_sleep()
    {
        AgendaPowerManager manager;
        manager.begin(0);

        const AgendaPowerDecision editor =
            manager.decide(
                300000,
                viewFor(AgendaUiScreen::EditorText));

        assert(
            editor.action ==
            AgendaPowerAction::EnterLightSleep);
        assert(
            !editor.policyWakeAfterMilliseconds.has_value());

        const AgendaPowerDecision reminder =
            manager.decide(
                300000,
                viewFor(
                    AgendaUiScreen::Month,
                    AgendaUiOverlay::Reminder));

        assert(
            reminder.action ==
            AgendaPowerAction::None);
    }

    void test_activity_reset_is_wrap_safe()
    {
        AgendaPowerManager manager;
        const AgendaUiView month =
            viewFor(AgendaUiScreen::Month);

        constexpr std::uint32_t start =
            UINT32_MAX - 100U;

        manager.begin(start);
        manager.recordActivity(start + 50U);

        assert(
            manager.decide(
                start + 50U + 44999U,
                month).action ==
            AgendaPowerAction::None);

        assert(
            manager.decide(
                start + 50U + 45000U,
                month).action ==
            AgendaPowerAction::EnterLightSleep);
    }
}

int main()
{
    test_policy_thresholds_and_safe_deep_sleep();
    test_unsafe_and_reminder_views_never_request_deep_sleep();
    test_activity_reset_is_wrap_safe();

    std::cout << "All agenda power manager tests passed.\n";
    return 0;
}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_power_manager.cpp src\ui\agenda_power_manager.cpp -o build\test_agenda_power_manager.exe
Write-Host "Power manager RED exit code: $LASTEXITCODE"
```

Expected: missing agenda_power_manager files.

- [ ] **Step 3: Create the policy header**

```cpp
#pragma once

#include <cstdint>
#include <optional>

#include "core/agenda_ui_types.h"

constexpr std::uint32_t AgendaLightSleepTimeoutMilliseconds =
    45000;

constexpr std::uint32_t AgendaDeepSleepTimeoutMilliseconds =
    300000;

enum class AgendaPowerAction
{
    None,
    EnterLightSleep,
    EnterDeepSleep
};

struct AgendaPowerDecision
{
    AgendaPowerAction action;

    std::optional<std::uint32_t>
        policyWakeAfterMilliseconds;
};

bool agendaViewAllowsDeepSleep(
    const AgendaUiView& view
);

class AgendaPowerManager
{
public:
    void begin(
        std::uint32_t nowMilliseconds
    );

    void recordActivity(
        std::uint32_t nowMilliseconds
    );

    AgendaPowerDecision decide(
        std::uint32_t nowMilliseconds,
        const AgendaUiView& view
    ) const;

private:
    std::uint32_t lastActivityMilliseconds_ = 0;
    bool started_ = false;
};
```

- [ ] **Step 4: Create the wrap-safe implementation**

```cpp
#include "agenda_power_manager.h"

bool agendaViewAllowsDeepSleep(
    const AgendaUiView& view
)
{
    if (view.overlay != AgendaUiOverlay::None)
    {
        return false;
    }

    return
        view.screen == AgendaUiScreen::Month ||
        view.screen == AgendaUiScreen::Day ||
        view.screen == AgendaUiScreen::EventDetails;
}

void AgendaPowerManager::begin(
    std::uint32_t nowMilliseconds
)
{
    lastActivityMilliseconds_ = nowMilliseconds;
    started_ = true;
}

void AgendaPowerManager::recordActivity(
    std::uint32_t nowMilliseconds
)
{
    lastActivityMilliseconds_ = nowMilliseconds;
    started_ = true;
}

AgendaPowerDecision AgendaPowerManager::decide(
    std::uint32_t nowMilliseconds,
    const AgendaUiView& view
) const
{
    if (
        !started_ ||
        view.overlay == AgendaUiOverlay::Reminder
    )
    {
        return AgendaPowerDecision{
            AgendaPowerAction::None,
            std::nullopt
        };
    }

    const std::uint32_t inactiveMilliseconds =
        static_cast<std::uint32_t>(
            nowMilliseconds -
            lastActivityMilliseconds_);

    if (
        inactiveMilliseconds <
        AgendaLightSleepTimeoutMilliseconds
    )
    {
        return AgendaPowerDecision{
            AgendaPowerAction::None,
            std::nullopt
        };
    }

    if (
        inactiveMilliseconds >=
        AgendaDeepSleepTimeoutMilliseconds
    )
    {
        return AgendaPowerDecision{
            agendaViewAllowsDeepSleep(view)
                ? AgendaPowerAction::EnterDeepSleep
                : AgendaPowerAction::EnterLightSleep,
            std::nullopt
        };
    }

    return AgendaPowerDecision{
        AgendaPowerAction::EnterLightSleep,
        AgendaDeepSleepTimeoutMilliseconds -
            inactiveMilliseconds
    };
}
```

- [ ] **Step 5: Compile/run GREEN and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_power_manager.cpp src\ui\agenda_power_manager.cpp -o build\test_agenda_power_manager.exe
if ($LASTEXITCODE -ne 0) { throw 'Power manager compile failed.' }
.\build\test_agenda_power_manager.exe
if ($LASTEXITCODE -ne 0) { throw 'Power manager tests failed.' }
git diff --check
git add -- src/ui/agenda_power_manager.h src/ui/agenda_power_manager.cpp tests/test_agenda_power_manager.cpp
git diff --cached --check
git commit -m "feat: define agenda inactivity policy"
git status --short
```


---

### Task 3: Report touch activity and consume wake gestures

**Files:**
- Modify: src/ui/touch_release_detector.h
- Modify: src/ui/touch_release_detector.cpp
- Modify: src/ui/agenda_runtime.h
- Modify: src/ui/agenda_runtime.cpp
- Modify: tests/test_agenda_runtime.cpp

**Interfaces:**
- Produces: AgendaRuntimeUpdateResult::touchActivity, AgendaRuntime::suppressActionsUntilRelease(), TouchReleaseDetector::reset().

- [ ] **Step 1: Fix the stale round-display coordinate and add two tests**

Apply this exact diff:

```diff
diff --git a/tests/test_agenda_runtime.cpp b/tests/test_agenda_runtime.cpp
index c3a72e0..03e1fe6 100644
--- a/tests/test_agenda_runtime.cpp
+++ b/tests/test_agenda_runtime.cpp
@@ -164,7 +164,7 @@ namespace

         stateMachine.start();

-        ScriptedTouchInput touch({{TouchInputError::None, {true, 200, 20}},
+        ScriptedTouchInput touch({{TouchInputError::None, {true, 168, 200}},
                                   {TouchInputError::None, {false, 0, 0}},
                                   {TouchInputError::None, {false, 0, 0}}});

@@ -249,15 +249,104 @@ namespace
         assert(surface.clearCount == 1);
         assert(clock.readCount == 2);
     }
+
+    void test_only_successful_in_bounds_touch_reports_activity()
+    {
+        RuntimeStorage storage;
+        RuntimeClock clock;
+        AgendaRepository repository(storage);
+        AgendaController controller(repository);
+        AgendaUiStateMachine stateMachine(
+            controller,
+            clock,
+            clock.value);
+        stateMachine.start();
+
+        ScriptedTouchInput touch({
+            {TouchInputError::None, {true, 120, 120}},
+            {TouchInputError::None, {true, 240, 10}},
+            {TouchInputError::ReadFailed, {true, 120, 120}}
+        });
+
+        TouchReleaseDetector releaseDetector;
+        AgendaTouchMapper touchMapper;
+        AgendaRenderer renderer;
+        CountingDrawSurface surface;
+        AgendaRuntime runtime(
+            touch,
+            releaseDetector,
+            touchMapper,
+            stateMachine,
+            renderer,
+            surface);
+
+        runtime.begin(0);
+
+        assert(runtime.update(10).touchActivity);
+        assert(!runtime.update(20).touchActivity);
+
+        const AgendaRuntimeUpdateResult failed =
+            runtime.update(30);
+
+        assert(!failed.touchActivity);
+        assert(
+            failed.touchError ==
+            TouchInputError::ReadFailed);
+    }
+
+    void test_wake_gesture_is_suppressed_through_release()
+    {
+        RuntimeStorage storage;
+        RuntimeClock clock;
+        AgendaRepository repository(storage);
+        AgendaController controller(repository);
+        AgendaUiStateMachine stateMachine(
+            controller,
+            clock,
+            clock.value);
+        stateMachine.start();
+
+        ScriptedTouchInput touch({
+            {TouchInputError::None, {true, 168, 200}},
+            {TouchInputError::ReadFailed, {false, 0, 0}},
+            {TouchInputError::None, {false, 0, 0}},
+            {TouchInputError::None, {true, 168, 200}},
+            {TouchInputError::None, {false, 0, 0}}
+        });
+
+        TouchReleaseDetector releaseDetector;
+        AgendaTouchMapper touchMapper;
+        AgendaRenderer renderer;
+        CountingDrawSurface surface;
+        AgendaRuntime runtime(
+            touch,
+            releaseDetector,
+            touchMapper,
+            stateMachine,
+            renderer,
+            surface);
+
+        runtime.begin(0);
+        runtime.suppressActionsUntilRelease();
+
+        assert(!runtime.update(10).actionHandled);
+        assert(!runtime.update(20).actionHandled);
+        assert(!runtime.update(30).actionHandled);
+        assert(!runtime.update(40).actionHandled);
+        assert(runtime.update(50).actionHandled);
+        assert(stateMachine.view().displayedMonth.month == 10);
+    }
 }

 int main()
 {
     test_one_release_handles_one_action_and_redraws_once();
     test_minute_tick_uses_wrap_safe_elapsed_time();
+    test_only_successful_in_bounds_touch_reports_activity();
+    test_wake_gesture_is_suppressed_through_release();

     std::cout
         << "All agenda runtime tests passed.\n";

     return 0;
-}
\ No newline at end of file
+}
```

The coordinate 168,200 is inside the current MonthNext control. The prior 200,20 point was left stale by the round-display layout change.

- [ ] **Step 2: Compile and confirm RED**

```powershell
$runtimeCore = @('src\core\date_time.cpp','src\core\utf8.cpp','src\core\event.cpp','src\core\agenda_state.cpp','src\core\agenda_manager.cpp','src\core\crc32.cpp','src\core\event_codec.cpp','src\core\agenda_repository.cpp','src\core\reminder_engine.cpp','src\core\agenda_controller.cpp','src\core\calendar_engine.cpp','src\core\t9_engine.cpp','src\core\agenda_ui_state_machine.cpp')
$runtimeUi = @('src\ui\agenda_renderer.cpp','src\ui\agenda_touch_mapper.cpp','src\ui\touch_release_detector.cpp','src\ui\agenda_runtime.cpp')
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_runtime.cpp @runtimeCore @runtimeUi -o build\test_agenda_runtime.exe
Write-Host "Wake gesture RED exit code: $LASTEXITCODE"
```

Expected: missing touchActivity and suppressActionsUntilRelease.

- [ ] **Step 3: Add reset/activity/suppression behavior**

Apply this exact diff:

```diff
diff --git a/src/ui/agenda_runtime.cpp b/src/ui/agenda_runtime.cpp
index 48aa223..8230a28 100644
--- a/src/ui/agenda_runtime.cpp
+++ b/src/ui/agenda_runtime.cpp
@@ -39,6 +39,12 @@ void AgendaRuntime::begin(
     started_ = true;
 }

+void AgendaRuntime::suppressActionsUntilRelease()
+{
+    releaseDetector_.reset();
+    suppressUntilRelease_ = true;
+}
+
 AgendaRuntimeUpdateResult AgendaRuntime::update(
     std::uint32_t nowMilliseconds)
 {
@@ -67,22 +73,38 @@ AgendaRuntimeUpdateResult AgendaRuntime::update(

         if (input.error == TouchInputError::None)
         {
-            const std::optional<UiPoint> released =
-                releaseDetector_.update(input.sample);
+            result.touchActivity =
+                input.sample.touched &&
+                input.sample.x < AGENDA_UI_DISPLAY_WIDTH &&
+                input.sample.y < AGENDA_UI_DISPLAY_HEIGHT;

-            if (released.has_value())
+            if (suppressUntilRelease_)
+            {
+                if (!input.sample.touched)
+                {
+                    suppressUntilRelease_ = false;
+                    releaseDetector_.reset();
+                }
+            }
+            else
             {
-                const std::optional<AgendaUiAction> action =
-                    touchMapper_.map(
-                        stateMachine_.view(),
-                        *released,
-                        nowMilliseconds);
+                const std::optional<UiPoint> released =
+                    releaseDetector_.update(input.sample);

-                if (action.has_value())
+                if (released.has_value())
                 {
-                    stateMachine_.handle(*action);
-                    result.actionHandled = true;
-                    renderRequired = true;
+                    const std::optional<AgendaUiAction> action =
+                        touchMapper_.map(
+                            stateMachine_.view(),
+                            *released,
+                            nowMilliseconds);
+
+                    if (action.has_value())
+                    {
+                        stateMachine_.handle(*action);
+                        result.actionHandled = true;
+                        renderRequired = true;
+                    }
                 }
             }
         }
@@ -110,4 +132,4 @@ AgendaRuntimeUpdateResult AgendaRuntime::update(
     }

     return result;
-}
\ No newline at end of file
+}
diff --git a/src/ui/agenda_runtime.h b/src/ui/agenda_runtime.h
index 8e5eac7..700bf09 100644
--- a/src/ui/agenda_runtime.h
+++ b/src/ui/agenda_runtime.h
@@ -13,6 +13,7 @@ struct AgendaRuntimeUpdateResult
     bool touchPolled = false;
     TouchInputError touchError =
         TouchInputError::None;
+    bool touchActivity = false;
     bool actionHandled = false;
     bool ticked = false;
     bool rendered = false;
@@ -32,6 +33,8 @@ public:
     void begin(
         std::uint32_t nowMilliseconds);

+    void suppressActionsUntilRelease();
+
     AgendaRuntimeUpdateResult update(
         std::uint32_t nowMilliseconds);

@@ -44,6 +47,7 @@ private:
     DrawSurface &surface_;

     bool started_ = false;
+    bool suppressUntilRelease_ = false;
     std::uint32_t lastTouchPollMilliseconds_ = 0;
     std::uint32_t lastTickMilliseconds_ = 0;
-};
\ No newline at end of file
+};
diff --git a/src/ui/touch_release_detector.cpp b/src/ui/touch_release_detector.cpp
index 9d7e061..a985131 100644
--- a/src/ui/touch_release_detector.cpp
+++ b/src/ui/touch_release_detector.cpp
@@ -1,5 +1,10 @@
 #include "touch_release_detector.h"

+void TouchReleaseDetector::reset()
+{
+    latestPoint_.reset();
+}
+
 std::optional<UiPoint> TouchReleaseDetector::update(
     const TouchSample &sample)
 {
@@ -26,4 +31,4 @@ std::optional<UiPoint> TouchReleaseDetector::update(
     latestPoint_.reset();

     return released;
-}
\ No newline at end of file
+}
diff --git a/src/ui/touch_release_detector.h b/src/ui/touch_release_detector.h
index e6c1e05..c3700b0 100644
--- a/src/ui/touch_release_detector.h
+++ b/src/ui/touch_release_detector.h
@@ -8,9 +8,11 @@
 class TouchReleaseDetector
 {
 public:
+    void reset();
+
     std::optional<UiPoint> update(
         const TouchSample &sample);

 private:
     std::optional<UiPoint> latestPoint_;
-};
\ No newline at end of file
+};
```

An I2C error keeps suppression armed. Only a successful not-touched sample ends it. The suppression path must still allow the periodic one-minute tick.

- [ ] **Step 4: Compile/run GREEN and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_runtime.cpp @runtimeCore @runtimeUi -o build\test_agenda_runtime.exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda runtime compile failed.' }
.\build\test_agenda_runtime.exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda runtime tests failed.' }
git diff --check
git add -- src/ui/touch_release_detector.h src/ui/touch_release_detector.cpp src/ui/agenda_runtime.h src/ui/agenda_runtime.cpp tests/test_agenda_runtime.cpp
git diff --cached --check
git commit -m "feat: consume touch wake gestures"
git status --short
```


---

### Task 4: Convert reminder targets with exact epoch seconds

**Files:**
- Create: src/ui/power_hardware.h
- Modify: src/platform/esp32/esp32_clock.h
- Modify: src/platform/esp32/esp32_clock.cpp
- Modify: tests/test_esp32_clock.cpp

**Interfaces:**
- Produces: WakeDelayClock::delayUntil() and WakeDelayResult.

- [ ] **Step 1: Add three exact-delay tests**

Apply this exact diff:

```diff
diff --git a/tests/test_esp32_clock.cpp b/tests/test_esp32_clock.cpp
index 1b6c93a..6fe1598 100644
--- a/tests/test_esp32_clock.cpp
+++ b/tests/test_esp32_clock.cpp
@@ -9,6 +9,11 @@ namespace
     bool readSucceeds = true;
     bool writeSucceeds = true;
     bool writeCalled = false;
+    bool epochReadSucceeds = true;
+    bool conversionSucceeds = true;
+
+    std::int64_t currentEpochSeconds = 0;
+    std::int64_t convertedEpochSeconds = 0;

     std::tm readValue{};
     std::tm writtenValue{};
@@ -34,11 +39,30 @@ namespace
         return writeSucceeds;
     }

+    bool fakeReadEpochSeconds(
+        std::int64_t& value)
+    {
+        value = currentEpochSeconds;
+        return epochReadSucceeds;
+    }
+
+    bool fakeConvertLocalDateTime(
+        const DateTime&,
+        std::int64_t& value)
+    {
+        value = convertedEpochSeconds;
+        return conversionSucceeds;
+    }
+
     void resetFakes()
     {
         readSucceeds = true;
         writeSucceeds = true;
         writeCalled = false;
+        epochReadSucceeds = true;
+        conversionSucceeds = true;
+        currentEpochSeconds = 0;
+        convertedEpochSeconds = 0;

         readValue = {};
         writtenValue = {};
@@ -49,7 +73,9 @@ namespace
         return Esp32Clock{
             Esp32ClockApi{
                 fakeReadLocalTime,
-                fakeWriteLocalTime}};
+                fakeWriteLocalTime,
+                fakeReadEpochSeconds,
+                fakeConvertLocalDateTime}};
     }

     void test_read_returns_current_local_date_time()
@@ -143,6 +169,68 @@ namespace

         assert(writeCalled);
     }
+
+    void test_wake_delay_uses_current_seconds_exactly()
+    {
+        resetFakes();
+        currentEpochSeconds = 1000000;
+        convertedEpochSeconds = 1000061;
+
+        const WakeDelayResult result =
+            makeClock().delayUntil(
+                DateTime{
+                    Date{2026, 9, 17},
+                    TimeOfDay{17, 0}});
+
+        assert(result.error == ClockError::None);
+        assert(!result.dueNow);
+        assert(result.microseconds == 61000000ULL);
+    }
+
+    void test_non_positive_wake_delay_is_due_now()
+    {
+        resetFakes();
+        currentEpochSeconds = 1000000;
+        convertedEpochSeconds = 1000000;
+
+        const WakeDelayResult result =
+            makeClock().delayUntil(
+                DateTime{
+                    Date{2026, 9, 17},
+                    TimeOfDay{17, 0}});
+
+        assert(result.error == ClockError::None);
+        assert(result.dueNow);
+        assert(result.microseconds == 0);
+    }
+
+    void test_wake_delay_reports_invalid_and_conversion_errors()
+    {
+        resetFakes();
+        assert(
+            makeClock().delayUntil(
+                DateTime{
+                    Date{2026, 2, 30},
+                    TimeOfDay{17, 0}}).error ==
+            ClockError::InvalidDateTime);
+
+        epochReadSucceeds = false;
+        assert(
+            makeClock().delayUntil(
+                DateTime{
+                    Date{2026, 9, 17},
+                    TimeOfDay{17, 0}}).error ==
+            ClockError::ReadFailed);
+
+        resetFakes();
+        conversionSucceeds = false;
+        assert(
+            makeClock().delayUntil(
+                DateTime{
+                    Date{2026, 9, 17},
+                    TimeOfDay{17, 0}}).error ==
+            ClockError::ReadFailed);
+    }
 }

 int main()
@@ -150,9 +238,12 @@ int main()
     test_read_returns_current_local_date_time();
     test_read_reports_failure_and_unset_clock();
     test_set_validates_and_writes_local_date_time();
+    test_wake_delay_uses_current_seconds_exactly();
+    test_non_positive_wake_delay_is_due_now();
+    test_wake_delay_reports_invalid_and_conversion_errors();

     std::cout
         << "All ESP32 clock tests passed.\n";

     return 0;
-}
\ No newline at end of file
+}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_esp32_clock.cpp src\platform\esp32\esp32_clock.cpp src\core\date_time.cpp -o build\test_esp32_clock.exe
Write-Host "Wake delay RED exit code: $LASTEXITCODE"
```

Expected: missing API fields, WakeDelayResult, and delayUntil.

- [ ] **Step 3: Create the platform-neutral interfaces**

```cpp
#pragma once

#include <cstdint>
#include <optional>

#include "core/clock.h"

enum class PowerWakeCause
{
    ColdBoot,
    Touch,
    Timer,
    Unknown
};

enum class PowerHardwareError
{
    None,
    Unavailable,
    TouchActive,
    TouchWakeConfigurationFailed,
    TimerWakeConfigurationFailed,
    LightSleepFailed,
    DeepSleepReturned
};

class PowerHardware
{
public:
    virtual ~PowerHardware() = default;

    virtual PowerWakeCause wakeCause() const = 0;
    virtual void releaseWakePin() = 0;

    virtual PowerHardwareError prepareWake(
        const std::optional<std::uint64_t>&
            timerMicroseconds
    ) = 0;

    virtual PowerHardwareError enterLightSleep() = 0;
    virtual PowerHardwareError enterDeepSleep() = 0;
};

class BacklightControl
{
public:
    virtual ~BacklightControl() = default;

    virtual void enableBacklight() = 0;
    virtual void disableBacklight() = 0;
};

struct WakeDelayResult
{
    ClockError error;
    bool dueNow;
    std::uint64_t microseconds;
};

class WakeDelayClock
{
public:
    virtual ~WakeDelayClock() = default;

    virtual WakeDelayResult delayUntil(
        const DateTime& target
    ) const = 0;
};
```

- [ ] **Step 4: Extend Esp32Clock**

Apply this exact diff:

```diff
diff --git a/src/platform/esp32/esp32_clock.cpp b/src/platform/esp32/esp32_clock.cpp
index a683a15..71edb0c 100644
--- a/src/platform/esp32/esp32_clock.cpp
+++ b/src/platform/esp32/esp32_clock.cpp
@@ -97,6 +97,51 @@ namespace
                    &rawTime,
                    nullptr) == 0;
     }
+
+    bool readSystemEpochSeconds(
+        std::int64_t& value
+    )
+    {
+        const time_t seconds = ::time(nullptr);
+
+        if (seconds == static_cast<time_t>(-1))
+        {
+            return false;
+        }
+
+        value = static_cast<std::int64_t>(seconds);
+        return true;
+    }
+
+    bool localDateTimeToSystemEpochSeconds(
+        const DateTime& value,
+        std::int64_t& seconds
+    )
+    {
+        if (!configureGreeceTimeZone())
+        {
+            return false;
+        }
+
+        std::tm localTime{};
+        localTime.tm_year = value.date.year - 1900;
+        localTime.tm_mon = value.date.month - 1;
+        localTime.tm_mday = value.date.day;
+        localTime.tm_hour = value.time.hour;
+        localTime.tm_min = value.time.minute;
+        localTime.tm_sec = 0;
+        localTime.tm_isdst = -1;
+
+        const time_t converted = ::mktime(&localTime);
+
+        if (converted == static_cast<time_t>(-1))
+        {
+            return false;
+        }
+
+        seconds = static_cast<std::int64_t>(converted);
+        return true;
+    }
 #else
     bool readSystemLocalTime(
         std::tm &)
@@ -109,13 +154,28 @@ namespace
     {
         return false;
     }
+
+    bool readSystemEpochSeconds(std::int64_t&)
+    {
+        return false;
+    }
+
+    bool localDateTimeToSystemEpochSeconds(
+        const DateTime&,
+        std::int64_t&
+    )
+    {
+        return false;
+    }
 #endif
 }

 Esp32Clock::Esp32Clock()
     : api_{
           readSystemLocalTime,
-          writeSystemLocalTime}
+          writeSystemLocalTime,
+          readSystemEpochSeconds,
+          localDateTimeToSystemEpochSeconds}
 {
 }

@@ -204,4 +264,66 @@ ClockError Esp32Clock::set(
     }

     return ClockError::None;
-}
\ No newline at end of file
+}
+
+WakeDelayResult Esp32Clock::delayUntil(
+    const DateTime& target
+) const
+{
+    if (
+        !isValidDate(target.date) ||
+        !isValidTime(target.time)
+    )
+    {
+        return WakeDelayResult{
+            ClockError::InvalidDateTime,
+            false,
+            0
+        };
+    }
+
+    if (
+        api_.readEpochSeconds == nullptr ||
+        api_.localDateTimeToEpochSeconds == nullptr
+    )
+    {
+        return WakeDelayResult{
+            ClockError::Unavailable,
+            false,
+            0
+        };
+    }
+
+    std::int64_t nowSeconds = 0;
+    std::int64_t targetSeconds = 0;
+
+    if (
+        !api_.readEpochSeconds(nowSeconds) ||
+        !api_.localDateTimeToEpochSeconds(
+            target,
+            targetSeconds)
+    )
+    {
+        return WakeDelayResult{
+            ClockError::ReadFailed,
+            false,
+            0
+        };
+    }
+
+    if (targetSeconds <= nowSeconds)
+    {
+        return WakeDelayResult{
+            ClockError::None,
+            true,
+            0
+        };
+    }
+
+    return WakeDelayResult{
+        ClockError::None,
+        false,
+        static_cast<std::uint64_t>(
+            targetSeconds - nowSeconds) * 1000000ULL
+    };
+}
diff --git a/src/platform/esp32/esp32_clock.h b/src/platform/esp32/esp32_clock.h
index 094fdd7..a031ffe 100644
--- a/src/platform/esp32/esp32_clock.h
+++ b/src/platform/esp32/esp32_clock.h
@@ -1,8 +1,10 @@
 #pragma once

+#include <cstdint>
 #include <ctime>

 #include "core/clock.h"
+#include "ui/power_hardware.h"

 struct Esp32ClockApi
 {
@@ -12,14 +14,28 @@ struct Esp32ClockApi
     using WriteLocalTimeFunction =
         bool (*)(const std::tm &);

+    using ReadEpochSecondsFunction =
+        bool (*)(std::int64_t&);
+
+    using LocalDateTimeToEpochSecondsFunction =
+        bool (*)(const DateTime&, std::int64_t&);
+
     ReadLocalTimeFunction readLocalTime =
         nullptr;

     WriteLocalTimeFunction writeLocalTime =
         nullptr;
+
+    ReadEpochSecondsFunction readEpochSeconds =
+        nullptr;
+
+    LocalDateTimeToEpochSecondsFunction
+        localDateTimeToEpochSeconds = nullptr;
 };

-class Esp32Clock final : public Clock
+class Esp32Clock final :
+    public Clock,
+    public WakeDelayClock
 {
 public:
     Esp32Clock();
@@ -32,6 +48,10 @@ public:
     ClockError set(
         const DateTime &value) override;

+    WakeDelayResult delayUntil(
+        const DateTime& target
+    ) const override;
+
 private:
     Esp32ClockApi api_;
-};
\ No newline at end of file
+};
```

The ESP32 conversion uses tm_sec=0 and tm_isdst=-1 under the configured Greek timezone. Non-positive intervals become due-now rather than zero-duration timers.

- [ ] **Step 5: Compile/run GREEN and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_esp32_clock.cpp src\platform\esp32\esp32_clock.cpp src\core\date_time.cpp -o build\test_esp32_clock.exe
if ($LASTEXITCODE -ne 0) { throw 'ESP32 clock compile failed.' }
.\build\test_esp32_clock.exe
if ($LASTEXITCODE -ne 0) { throw 'ESP32 clock tests failed.' }
git diff --check
git add -- src/ui/power_hardware.h src/platform/esp32/esp32_clock.h src/platform/esp32/esp32_clock.cpp tests/test_esp32_clock.cpp
git diff --cached --check
git commit -m "feat: calculate exact reminder wake delay"
git status --short
```


---

### Task 5: Coordinate policy and reminder timers

**Files:**
- Create: src/ui/agenda_power_coordinator.h
- Create: src/ui/agenda_power_coordinator.cpp
- Create: tests/test_agenda_power_coordinator.cpp

**Interfaces:**
- Consumes: AgendaPowerManager, AgendaController::reminderSchedule(), Clock::read(), WakeDelayClock::delayUntil().
- Produces: AgendaPowerPlan, AgendaPowerCoordinator::update(), planImmediateDeepSleep().

- [ ] **Step 1: Create the three-test suite**

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "core/agenda_repository.h"
#include "ui/agenda_power_coordinator.h"

namespace
{
    class TestStorage final : public ByteStorage
    {
    public:
        ByteReadResult read() const override
        {
            return ByteReadResult{
                ByteStorageError::NotFound,
                {}};
        }

        ByteStorageError write(
            const std::vector<std::uint8_t>&
        ) const override
        {
            return ByteStorageError::None;
        }
    };

    class TestClock final : public Clock
    {
    public:
        ClockError readError = ClockError::None;
        DateTime value{
            Date{2026, 9, 16},
            TimeOfDay{10, 0}};

        ClockReadResult read() const override
        {
            return ClockReadResult{readError, value};
        }

        ClockError set(const DateTime& next) override
        {
            value = next;
            return ClockError::None;
        }
    };

    class TestWakeDelayClock final : public WakeDelayClock
    {
    public:
        WakeDelayResult result{
            ClockError::None,
            false,
            120000000ULL};

        WakeDelayResult delayUntil(
            const DateTime&
        ) const override
        {
            return result;
        }
    };

    AgendaUiView monthView()
    {
        AgendaUiView view;
        view.screen = AgendaUiScreen::Month;
        view.overlay = AgendaUiOverlay::None;
        return view;
    }

    void addEvent(
        AgendaController& controller,
        const Date& date
    )
    {
        const AgendaControllerCreateResult created =
            controller.createEvent(
                EventDraft{
                    date,
                    std::nullopt,
                    "Future event"},
                DateTime{
                    Date{2026, 9, 16},
                    TimeOfDay{10, 0}});

        assert(created.error == AgendaControllerError::None);
        assert(created.eventError == EventError::None);
    }

    void test_earlier_reminder_timer_wins_over_policy_timer()
    {
        TestStorage storage;
        AgendaRepository repository(storage);
        AgendaController controller(repository);
        assert(controller.start().error == AgendaControllerError::None);
        addEvent(controller, Date{2026, 9, 18});

        TestClock clock;
        TestWakeDelayClock wakeDelayClock;
        AgendaPowerManager manager;
        AgendaPowerCoordinator coordinator(
            manager,
            controller,
            clock,
            wakeDelayClock);

        coordinator.begin(0);

        const AgendaPowerPlan plan =
            coordinator.update(45000, false, monthView());

        assert(plan.mode == AgendaSleepMode::Light);
        assert(plan.timerMicroseconds == 120000000ULL);
        assert(!plan.reminderDueNow);
        assert(plan.error == AgendaPowerCoordinatorError::None);
    }

    void test_due_reminder_blocks_sleep()
    {
        TestStorage storage;
        AgendaRepository repository(storage);
        AgendaController controller(repository);
        assert(controller.start().error == AgendaControllerError::None);
        addEvent(controller, Date{2026, 9, 17});

        TestClock clock;
        clock.value.time = TimeOfDay{17, 0};
        TestWakeDelayClock wakeDelayClock;
        AgendaPowerManager manager;
        AgendaPowerCoordinator coordinator(
            manager,
            controller,
            clock,
            wakeDelayClock);

        coordinator.begin(0);
        const AgendaPowerPlan plan =
            coordinator.update(45000, false, monthView());

        assert(plan.mode == AgendaSleepMode::None);
        assert(!plan.timerMicroseconds.has_value());
        assert(plan.reminderDueNow);
        assert(plan.error == AgendaPowerCoordinatorError::None);
    }

    void test_clock_failure_allows_light_but_blocks_deep_sleep()
    {
        TestStorage storage;
        AgendaRepository repository(storage);
        AgendaController controller(repository);
        assert(controller.start().error == AgendaControllerError::None);

        TestClock clock;
        clock.readError = ClockError::ReadFailed;
        TestWakeDelayClock wakeDelayClock;
        AgendaPowerManager manager;
        AgendaPowerCoordinator coordinator(
            manager,
            controller,
            clock,
            wakeDelayClock);

        coordinator.begin(0);

        const AgendaPowerPlan light =
            coordinator.update(45000, false, monthView());

        assert(light.mode == AgendaSleepMode::Light);
        assert(light.timerMicroseconds == 255000000ULL);
        assert(
            light.error ==
            AgendaPowerCoordinatorError::ClockReadFailed);

        const AgendaPowerPlan deep =
            coordinator.update(300000, false, monthView());

        assert(deep.mode == AgendaSleepMode::None);
        assert(!deep.timerMicroseconds.has_value());
        assert(
            deep.error ==
            AgendaPowerCoordinatorError::ClockReadFailed);
    }
}

int main()
{
    test_earlier_reminder_timer_wins_over_policy_timer();
    test_due_reminder_blocks_sleep();
    test_clock_failure_allows_light_but_blocks_deep_sleep();

    std::cout << "All agenda power coordinator tests passed.\n";
    return 0;
}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
$coordinatorCore = @('src\core\date_time.cpp','src\core\utf8.cpp','src\core\event.cpp','src\core\agenda_state.cpp','src\core\agenda_manager.cpp','src\core\crc32.cpp','src\core\event_codec.cpp','src\core\agenda_repository.cpp','src\core\reminder_engine.cpp','src\core\agenda_controller.cpp')
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_power_coordinator.cpp @coordinatorCore src\ui\agenda_power_manager.cpp src\ui\agenda_power_coordinator.cpp -o build\test_agenda_power_coordinator.exe
Write-Host "Coordinator RED exit code: $LASTEXITCODE"
```

Expected: missing coordinator files.

- [ ] **Step 3: Create the coordinator header**

```cpp
#pragma once

#include <cstdint>
#include <optional>

#include "agenda_power_manager.h"
#include "power_hardware.h"
#include "core/agenda_controller.h"

enum class AgendaSleepMode
{
    None,
    Light,
    Deep
};

enum class AgendaPowerCoordinatorError
{
    None,
    ClockReadFailed,
    ReminderScheduleFailed,
    WakeDelayFailed
};

struct AgendaPowerPlan
{
    AgendaSleepMode mode;
    std::optional<std::uint64_t> timerMicroseconds;
    bool reminderDueNow;
    AgendaPowerCoordinatorError error;
};

class AgendaPowerCoordinator
{
public:
    AgendaPowerCoordinator(
        AgendaPowerManager& manager,
        AgendaController& controller,
        Clock& clock,
        WakeDelayClock& wakeDelayClock
    );

    void begin(
        std::uint32_t nowMilliseconds
    );

    void recordActivity(
        std::uint32_t nowMilliseconds
    );

    AgendaPowerPlan update(
        std::uint32_t nowMilliseconds,
        bool touchActivity,
        const AgendaUiView& view
    );

    AgendaPowerPlan planImmediateDeepSleep(
        const AgendaUiView& view
    ) const;

private:
    AgendaPowerPlan buildSleepPlan(
        AgendaSleepMode mode,
        const std::optional<std::uint32_t>&
            policyWakeAfterMilliseconds
    ) const;

    AgendaPowerManager& manager_;
    AgendaController& controller_;
    Clock& clock_;
    WakeDelayClock& wakeDelayClock_;
};
```

- [ ] **Step 4: Create the coordinator implementation**

```cpp
#include "agenda_power_coordinator.h"

#include <algorithm>

namespace
{
    AgendaSleepMode modeFor(
        AgendaPowerAction action
    )
    {
        switch (action)
        {
        case AgendaPowerAction::EnterLightSleep:
            return AgendaSleepMode::Light;

        case AgendaPowerAction::EnterDeepSleep:
            return AgendaSleepMode::Deep;

        case AgendaPowerAction::None:
            break;
        }

        return AgendaSleepMode::None;
    }

    std::optional<std::uint64_t> policyTimer(
        const std::optional<std::uint32_t>& milliseconds
    )
    {
        if (!milliseconds.has_value())
        {
            return std::nullopt;
        }

        return static_cast<std::uint64_t>(*milliseconds) *
               1000ULL;
    }

    AgendaPowerPlan failedPlan(
        AgendaSleepMode requestedMode,
        const std::optional<std::uint64_t>& timer,
        AgendaPowerCoordinatorError error
    )
    {
        return AgendaPowerPlan{
            requestedMode == AgendaSleepMode::Light
                ? AgendaSleepMode::Light
                : AgendaSleepMode::None,
            requestedMode == AgendaSleepMode::Light
                ? timer
                : std::nullopt,
            false,
            error
        };
    }
}

AgendaPowerCoordinator::AgendaPowerCoordinator(
    AgendaPowerManager& manager,
    AgendaController& controller,
    Clock& clock,
    WakeDelayClock& wakeDelayClock
)
    : manager_(manager),
      controller_(controller),
      clock_(clock),
      wakeDelayClock_(wakeDelayClock)
{
}

void AgendaPowerCoordinator::begin(
    std::uint32_t nowMilliseconds
)
{
    manager_.begin(nowMilliseconds);
}

void AgendaPowerCoordinator::recordActivity(
    std::uint32_t nowMilliseconds
)
{
    manager_.recordActivity(nowMilliseconds);
}

AgendaPowerPlan AgendaPowerCoordinator::update(
    std::uint32_t nowMilliseconds,
    bool touchActivity,
    const AgendaUiView& view
)
{
    if (touchActivity)
    {
        manager_.recordActivity(nowMilliseconds);
    }

    const AgendaPowerDecision decision =
        manager_.decide(nowMilliseconds, view);

    if (decision.action == AgendaPowerAction::None)
    {
        return AgendaPowerPlan{
            AgendaSleepMode::None,
            std::nullopt,
            false,
            AgendaPowerCoordinatorError::None
        };
    }

    return buildSleepPlan(
        modeFor(decision.action),
        decision.policyWakeAfterMilliseconds);
}

AgendaPowerPlan AgendaPowerCoordinator::planImmediateDeepSleep(
    const AgendaUiView& view
) const
{
    if (!agendaViewAllowsDeepSleep(view))
    {
        return AgendaPowerPlan{
            AgendaSleepMode::None,
            std::nullopt,
            false,
            AgendaPowerCoordinatorError::None
        };
    }

    return buildSleepPlan(
        AgendaSleepMode::Deep,
        std::nullopt);
}

AgendaPowerPlan AgendaPowerCoordinator::buildSleepPlan(
    AgendaSleepMode mode,
    const std::optional<std::uint32_t>&
        policyWakeAfterMilliseconds
) const
{
    const std::optional<std::uint64_t> policyWake =
        policyTimer(policyWakeAfterMilliseconds);

    const ClockReadResult current = clock_.read();

    if (current.error != ClockError::None)
    {
        return failedPlan(
            mode,
            policyWake,
            AgendaPowerCoordinatorError::ClockReadFailed);
    }

    const AgendaControllerReminderScheduleResult schedule =
        controller_.reminderSchedule(current.value);

    if (
        schedule.error != AgendaControllerError::None ||
        schedule.reminderError != ReminderEngineError::None
    )
    {
        return failedPlan(
            mode,
            policyWake,
            AgendaPowerCoordinatorError::ReminderScheduleFailed);
    }

    if (schedule.dueNow)
    {
        return AgendaPowerPlan{
            AgendaSleepMode::None,
            std::nullopt,
            true,
            AgendaPowerCoordinatorError::None
        };
    }

    std::optional<std::uint64_t> wake = policyWake;

    if (schedule.scheduledAt.has_value())
    {
        const WakeDelayResult delay =
            wakeDelayClock_.delayUntil(
                *schedule.scheduledAt);

        if (delay.error != ClockError::None)
        {
            return failedPlan(
                mode,
                policyWake,
                AgendaPowerCoordinatorError::WakeDelayFailed);
        }

        if (delay.dueNow)
        {
            return AgendaPowerPlan{
                AgendaSleepMode::None,
                std::nullopt,
                true,
                AgendaPowerCoordinatorError::None
            };
        }

        if (
            !wake.has_value() ||
            delay.microseconds < *wake
        )
        {
            wake = delay.microseconds;
        }
    }

    return AgendaPowerPlan{
        mode,
        wake,
        false,
        AgendaPowerCoordinatorError::None
    };
}
```

Light sleep may retain its policy timer after a reminder-schedule error. Deep sleep is blocked on clock, schedule, or conversion error. The earlier policy/reminder timer wins.

- [ ] **Step 5: Compile/run GREEN and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_power_coordinator.cpp @coordinatorCore src\ui\agenda_power_manager.cpp src\ui\agenda_power_coordinator.cpp -o build\test_agenda_power_coordinator.exe
if ($LASTEXITCODE -ne 0) { throw 'Coordinator compile failed.' }
.\build\test_agenda_power_coordinator.exe
if ($LASTEXITCODE -ne 0) { throw 'Coordinator tests failed.' }
git diff --check
git add -- src/ui/agenda_power_coordinator.h src/ui/agenda_power_coordinator.cpp tests/test_agenda_power_coordinator.cpp
git diff --cached --check
git commit -m "feat: coordinate agenda sleep timers"
git status --short
```


---

### Task 6: Add the ESP32 sleep adapter

**Files:**
- Create: src/platform/esp32/power_hardware.h
- Create: src/platform/esp32/power_hardware.cpp
- Create: tests/test_esp32_power_hardware.cpp
- Modify: src/platform/esp32/display_hardware.h

**Interfaces:**
- Consumes: Esp32Board::TouchInterrupt, ESP-IDF sleep calls, RTC GPIO release.
- Produces: Esp32PowerHardware and DisplayHardware as BacklightControl.

- [ ] **Step 1: Create the three-test adapter suite**

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>

#include "platform/esp32/board_pins.h"
#include "platform/esp32/power_hardware.h"

namespace
{
    Esp32WakeSource wakeSource = Esp32WakeSource::Undefined;
    bool pinHigh = true;
    bool disableSucceeds = true;
    bool externalSucceeds = true;
    bool timerSucceeds = true;
    bool lightSucceeds = true;

    int releaseCount = 0;
    int disableCount = 0;
    int externalCount = 0;
    int timerCount = 0;
    int lightCount = 0;
    int deepCount = 0;
    std::int8_t lastPin = -1;
    bool lastWakeOnLow = false;
    std::uint64_t lastTimer = 0;

    Esp32WakeSource fakeWakeSource()
    {
        return wakeSource;
    }

    void fakeRelease(std::int8_t pin)
    {
        ++releaseCount;
        lastPin = pin;
    }

    bool fakeReadPin(std::int8_t pin, bool& high)
    {
        lastPin = pin;
        high = pinHigh;
        return true;
    }

    bool fakeDisable()
    {
        ++disableCount;
        return disableSucceeds;
    }

    bool fakeExternal(std::int8_t pin, bool wakeOnLow)
    {
        ++externalCount;
        lastPin = pin;
        lastWakeOnLow = wakeOnLow;
        return externalSucceeds;
    }

    bool fakeTimer(std::uint64_t microseconds)
    {
        ++timerCount;
        lastTimer = microseconds;
        return timerSucceeds;
    }

    bool fakeLight()
    {
        ++lightCount;
        return lightSucceeds;
    }

    void fakeDeep()
    {
        ++deepCount;
    }

    void resetFakes()
    {
        wakeSource = Esp32WakeSource::Undefined;
        pinHigh = true;
        disableSucceeds = true;
        externalSucceeds = true;
        timerSucceeds = true;
        lightSucceeds = true;
        releaseCount = 0;
        disableCount = 0;
        externalCount = 0;
        timerCount = 0;
        lightCount = 0;
        deepCount = 0;
        lastPin = -1;
        lastWakeOnLow = false;
        lastTimer = 0;
    }

    Esp32PowerHardware makeHardware()
    {
        return Esp32PowerHardware{
            Esp32PowerApi{
                fakeWakeSource,
                fakeRelease,
                fakeReadPin,
                fakeDisable,
                fakeExternal,
                fakeTimer,
                fakeLight,
                fakeDeep
            }
        };
    }

    void test_wake_causes_and_pin_release_are_mapped()
    {
        resetFakes();
        Esp32PowerHardware hardware = makeHardware();

        assert(hardware.wakeCause() == PowerWakeCause::ColdBoot);
        wakeSource = Esp32WakeSource::ExternalPin;
        assert(hardware.wakeCause() == PowerWakeCause::Touch);
        wakeSource = Esp32WakeSource::Timer;
        assert(hardware.wakeCause() == PowerWakeCause::Timer);
        wakeSource = Esp32WakeSource::Other;
        assert(hardware.wakeCause() == PowerWakeCause::Unknown);

        hardware.releaseWakePin();
        assert(releaseCount == 1);
        assert(lastPin == Esp32Board::TouchInterrupt);
    }

    void test_prepare_arms_active_low_touch_and_optional_timer()
    {
        resetFakes();
        Esp32PowerHardware hardware = makeHardware();

        assert(
            hardware.prepareWake(1234567ULL) ==
            PowerHardwareError::None);
        assert(disableCount == 1);
        assert(externalCount == 1);
        assert(timerCount == 1);
        assert(lastPin == Esp32Board::TouchInterrupt);
        assert(lastWakeOnLow);
        assert(lastTimer == 1234567ULL);

        assert(
            hardware.enterLightSleep() ==
            PowerHardwareError::None);
        assert(lightCount == 1);

        resetFakes();
        hardware = makeHardware();
        assert(
            hardware.prepareWake(std::nullopt) ==
            PowerHardwareError::None);
        assert(timerCount == 0);
        assert(
            hardware.enterDeepSleep() ==
            PowerHardwareError::DeepSleepReturned);
        assert(deepCount == 1);
    }

    void test_failed_or_active_wake_source_never_starts_sleep()
    {
        resetFakes();
        Esp32PowerHardware hardware = makeHardware();

        pinHigh = false;
        assert(
            hardware.prepareWake(std::nullopt) ==
            PowerHardwareError::TouchActive);
        assert(
            hardware.enterLightSleep() ==
            PowerHardwareError::Unavailable);
        assert(lightCount == 0);

        resetFakes();
        hardware = makeHardware();
        externalSucceeds = false;
        assert(
            hardware.prepareWake(std::nullopt) ==
            PowerHardwareError::TouchWakeConfigurationFailed);
        assert(disableCount == 2);
        assert(
            hardware.enterDeepSleep() ==
            PowerHardwareError::Unavailable);
        assert(deepCount == 0);

        resetFakes();
        hardware = makeHardware();
        timerSucceeds = false;
        assert(
            hardware.prepareWake(10ULL) ==
            PowerHardwareError::TimerWakeConfigurationFailed);
        assert(disableCount == 2);
        assert(lightCount == 0);
        assert(deepCount == 0);
    }
}

int main()
{
    test_wake_causes_and_pin_release_are_mapped();
    test_prepare_arms_active_low_touch_and_optional_timer();
    test_failed_or_active_wake_source_never_starts_sleep();

    std::cout << "All ESP32 power hardware tests passed.\n";
    return 0;
}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_esp32_power_hardware.cpp src\platform\esp32\power_hardware.cpp -o build\test_esp32_power_hardware.exe
Write-Host "Adapter RED exit code: $LASTEXITCODE"
```

Expected: missing adapter files.

- [ ] **Step 3: Create the adapter header**

```cpp
#pragma once

#include <cstdint>

#include "ui/power_hardware.h"

enum class Esp32WakeSource
{
    Undefined,
    ExternalPin,
    Timer,
    Other
};

struct Esp32PowerApi
{
    using ReadWakeSourceFunction =
        Esp32WakeSource (*)();

    using ReleaseWakePinFunction =
        void (*)(std::int8_t pin);

    using ReadPinHighFunction =
        bool (*)(std::int8_t pin, bool& high);

    using DisableWakeSourcesFunction =
        bool (*)();

    using EnableExternalWakeFunction =
        bool (*)(std::int8_t pin, bool wakeOnLow);

    using EnableTimerWakeFunction =
        bool (*)(std::uint64_t microseconds);

    using EnterLightSleepFunction =
        bool (*)();

    using EnterDeepSleepFunction =
        void (*)();

    ReadWakeSourceFunction readWakeSource = nullptr;
    ReleaseWakePinFunction releaseWakePin = nullptr;
    ReadPinHighFunction readPinHigh = nullptr;
    DisableWakeSourcesFunction disableWakeSources = nullptr;
    EnableExternalWakeFunction enableExternalWake = nullptr;
    EnableTimerWakeFunction enableTimerWake = nullptr;
    EnterLightSleepFunction enterLightSleep = nullptr;
    EnterDeepSleepFunction enterDeepSleep = nullptr;
};

class Esp32PowerHardware final : public PowerHardware
{
public:
    Esp32PowerHardware();

    explicit Esp32PowerHardware(
        Esp32PowerApi api);

    PowerWakeCause wakeCause() const override;
    void releaseWakePin() override;

    PowerHardwareError prepareWake(
        const std::optional<std::uint64_t>&
            timerMicroseconds
    ) override;

    PowerHardwareError enterLightSleep() override;
    PowerHardwareError enterDeepSleep() override;

private:
    Esp32PowerApi api_;
    bool wakePrepared_ = false;
};
```

- [ ] **Step 4: Create the adapter implementation**

```cpp
#include "power_hardware.h"

#include "board_pins.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#endif

namespace
{
#ifdef ARDUINO
    Esp32WakeSource readWakeSource()
    {
        switch (::esp_sleep_get_wakeup_cause())
        {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return Esp32WakeSource::Undefined;

        case ESP_SLEEP_WAKEUP_EXT0:
            return Esp32WakeSource::ExternalPin;

        case ESP_SLEEP_WAKEUP_TIMER:
            return Esp32WakeSource::Timer;

        default:
            return Esp32WakeSource::Other;
        }
    }

    void releaseWakePin(std::int8_t pin)
    {
        ::rtc_gpio_deinit(
            static_cast<gpio_num_t>(pin));
    }

    bool readPinHigh(
        std::int8_t pin,
        bool& high
    )
    {
        ::pinMode(pin, INPUT_PULLUP);
        high = ::digitalRead(pin) == HIGH;
        return true;
    }

    bool disableWakeSources()
    {
        return ::esp_sleep_disable_wakeup_source(
                   ESP_SLEEP_WAKEUP_ALL) == ESP_OK;
    }

    bool enableExternalWake(
        std::int8_t pin,
        bool wakeOnLow
    )
    {
        return ::esp_sleep_enable_ext0_wakeup(
                   static_cast<gpio_num_t>(pin),
                   wakeOnLow ? 0 : 1) == ESP_OK;
    }

    bool enableTimerWake(
        std::uint64_t microseconds
    )
    {
        return ::esp_sleep_enable_timer_wakeup(
                   microseconds) == ESP_OK;
    }

    bool enterLightSleep()
    {
        return ::esp_light_sleep_start() == ESP_OK;
    }

    void enterDeepSleep()
    {
        ::esp_deep_sleep_start();
    }
#else
    Esp32WakeSource readWakeSource()
    {
        return Esp32WakeSource::Undefined;
    }

    void releaseWakePin(std::int8_t)
    {
    }

    bool readPinHigh(std::int8_t, bool&)
    {
        return false;
    }

    bool disableWakeSources()
    {
        return false;
    }

    bool enableExternalWake(std::int8_t, bool)
    {
        return false;
    }

    bool enableTimerWake(std::uint64_t)
    {
        return false;
    }

    bool enterLightSleep()
    {
        return false;
    }

    void enterDeepSleep()
    {
    }
#endif

    Esp32PowerApi defaultPowerApi()
    {
        return Esp32PowerApi{
            readWakeSource,
            releaseWakePin,
            readPinHigh,
            disableWakeSources,
            enableExternalWake,
            enableTimerWake,
            enterLightSleep,
            enterDeepSleep
        };
    }
}

Esp32PowerHardware::Esp32PowerHardware()
    : Esp32PowerHardware(defaultPowerApi())
{
}

Esp32PowerHardware::Esp32PowerHardware(
    Esp32PowerApi api
)
    : api_(api)
{
}

PowerWakeCause Esp32PowerHardware::wakeCause() const
{
    if (api_.readWakeSource == nullptr)
    {
        return PowerWakeCause::Unknown;
    }

    switch (api_.readWakeSource())
    {
    case Esp32WakeSource::Undefined:
        return PowerWakeCause::ColdBoot;

    case Esp32WakeSource::ExternalPin:
        return PowerWakeCause::Touch;

    case Esp32WakeSource::Timer:
        return PowerWakeCause::Timer;

    case Esp32WakeSource::Other:
        break;
    }

    return PowerWakeCause::Unknown;
}

void Esp32PowerHardware::releaseWakePin()
{
    if (api_.releaseWakePin != nullptr)
    {
        api_.releaseWakePin(
            Esp32Board::TouchInterrupt);
    }
}

PowerHardwareError Esp32PowerHardware::prepareWake(
    const std::optional<std::uint64_t>&
        timerMicroseconds
)
{
    wakePrepared_ = false;

    if (
        api_.readPinHigh == nullptr ||
        api_.disableWakeSources == nullptr ||
        api_.enableExternalWake == nullptr ||
        (
            timerMicroseconds.has_value() &&
            api_.enableTimerWake == nullptr
        )
    )
    {
        return PowerHardwareError::Unavailable;
    }

    bool touchReleased = false;

    if (
        !api_.readPinHigh(
            Esp32Board::TouchInterrupt,
            touchReleased)
    )
    {
        return PowerHardwareError::Unavailable;
    }

    if (!touchReleased)
    {
        return PowerHardwareError::TouchActive;
    }

    if (!api_.disableWakeSources())
    {
        return PowerHardwareError::Unavailable;
    }

    if (
        !api_.enableExternalWake(
            Esp32Board::TouchInterrupt,
            true)
    )
    {
        api_.disableWakeSources();

        return PowerHardwareError::
            TouchWakeConfigurationFailed;
    }

    if (
        timerMicroseconds.has_value() &&
        (
            *timerMicroseconds == 0 ||
            !api_.enableTimerWake(
                *timerMicroseconds)
        )
    )
    {
        api_.disableWakeSources();

        return PowerHardwareError::
            TimerWakeConfigurationFailed;
    }

    wakePrepared_ = true;
    return PowerHardwareError::None;
}

PowerHardwareError Esp32PowerHardware::enterLightSleep()
{
    if (
        !wakePrepared_ ||
        api_.enterLightSleep == nullptr
    )
    {
        return PowerHardwareError::Unavailable;
    }

    wakePrepared_ = false;

    return api_.enterLightSleep()
        ? PowerHardwareError::None
        : PowerHardwareError::LightSleepFailed;
}

PowerHardwareError Esp32PowerHardware::enterDeepSleep()
{
    if (
        !wakePrepared_ ||
        api_.enterDeepSleep == nullptr
    )
    {
        return PowerHardwareError::Unavailable;
    }

    wakePrepared_ = false;
    api_.enterDeepSleep();

    return PowerHardwareError::DeepSleepReturned;
}
```

GPIO5 must be high before active-low EXT0 wake is armed. Any partial configuration is cleared. wakePrepared_ prevents sleep after a failed preparation.

- [ ] **Step 5: Implement BacklightControl**

Apply this exact diff:

```diff
diff --git a/src/platform/esp32/display_hardware.h b/src/platform/esp32/display_hardware.h
index d60776e..fe0dca0 100644
--- a/src/platform/esp32/display_hardware.h
+++ b/src/platform/esp32/display_hardware.h
@@ -2,14 +2,16 @@

 #include <Arduino_GFX_Library.h>

-class DisplayHardware
+#include "ui/power_hardware.h"
+
+class DisplayHardware final : public BacklightControl
 {
 public:
     DisplayHardware();

     bool begin();
-    void enableBacklight();
-    void disableBacklight();
+    void enableBacklight() override;
+    void disableBacklight() override;

     Arduino_GFX& graphics();
```

- [ ] **Step 6: Compile/run GREEN and commit**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_esp32_power_hardware.cpp src\platform\esp32\power_hardware.cpp -o build\test_esp32_power_hardware.exe
if ($LASTEXITCODE -ne 0) { throw 'Adapter compile failed.' }
.\build\test_esp32_power_hardware.exe
if ($LASTEXITCODE -ne 0) { throw 'Adapter tests failed.' }
git diff --check
git add -- src/platform/esp32/power_hardware.h src/platform/esp32/power_hardware.cpp src/platform/esp32/display_hardware.h tests/test_esp32_power_hardware.cpp
git diff --cached --check
git commit -m "feat: add ESP32 sleep hardware adapter"
git status --short
```


---

### Task 7: Compose gated hybrid power management

**Files:**
- Modify: src/platform/esp32/main.cpp
- Verify only: platformio.ini already includes all ui and ESP32 source files.

**Interfaces:**
- Consumes all Tasks 1–6.
- Produces explicit sleep/wake/error serial markers.
- Keeps DeepSleepHardwareVerified=false and a 15-second probe timer.

- [ ] **Step 1: Confirm the pre-integration static check is RED**

```powershell
$main = Get-Content .\src\platform\esp32\main.cpp -Raw
if ($main -notmatch 'AgendaPowerCoordinator') { throw 'RED: coordinator is not composed.' }
if ($main -notmatch 'LIGHT_SLEEP' -or $main -notmatch 'WAKE_TOUCH') { throw 'RED: diagnostics are absent.' }
```

Expected: the first assertion throws.

- [ ] **Step 2: Apply the validated firmware integration diff**

```diff
diff --git a/src/platform/esp32/main.cpp b/src/platform/esp32/main.cpp
index 7f125d3..512fc35 100644
--- a/src/platform/esp32/main.cpp
+++ b/src/platform/esp32/main.cpp
@@ -1,6 +1,7 @@
 #include <Arduino.h>

 #include <cstdint>
+#include <optional>
 #include <string>
 #include <string_view>

@@ -12,7 +13,10 @@
 #include "display_hardware.h"
 #include "esp32_clock.h"
 #include "littlefs_byte_storage.h"
+#include "power_hardware.h"
 #include "touch_hardware.h"
+#include "ui/agenda_power_coordinator.h"
+#include "ui/agenda_power_manager.h"
 #include "ui/agenda_renderer.h"
 #include "ui/agenda_runtime.h"
 #include "ui/agenda_touch_mapper.h"
@@ -20,33 +24,24 @@

 namespace
 {
-    constexpr std::uint32_t TouchErrorReportIntervalMilliseconds =
-        5000;
+    constexpr std::uint32_t TouchErrorReportIntervalMilliseconds = 5000;
+    constexpr bool DeepSleepHardwareVerified = false;
+    constexpr std::uint64_t DeepSleepProbeSafetyTimerMicroseconds =
+        15000000ULL;

     DisplayHardware displayHardware;
-
-    ArduinoGfxSurface drawSurface{
-        displayHardware.graphics()};
-
+    ArduinoGfxSurface drawSurface{displayHardware.graphics()};
     Cst816sTouchHardware touchHardware;
+    Esp32PowerHardware powerHardware;
     Esp32Clock clock;

-    LittleFsByteStorage primaryStorage{
-        std::string{"/agenda.bin"}};
-
-    LittleFsByteStorage backupStorage{
-        std::string{"/agenda.backup.bin"}};
-
-    AgendaRepository repository(
-        primaryStorage,
-        backupStorage);
-
+    LittleFsByteStorage primaryStorage{std::string{"/agenda.bin"}};
+    LittleFsByteStorage backupStorage{std::string{"/agenda.backup.bin"}};
+    AgendaRepository repository(primaryStorage, backupStorage);
     AgendaController controller(repository);

     const DateTime clockSetupDefault =
-        compileDateTimeOrFallback(
-            __DATE__,
-            __TIME__);
+        compileDateTimeOrFallback(__DATE__, __TIME__);

     AgendaUiStateMachine stateMachine(
         controller,
@@ -56,7 +51,6 @@ namespace
     AgendaRenderer renderer;
     AgendaTouchMapper touchMapper;
     TouchReleaseDetector releaseDetector;
-
     AgendaRuntime runtime(
         touchHardware,
         releaseDetector,
@@ -65,28 +59,42 @@ namespace
         renderer,
         drawSurface);

+    AgendaPowerManager powerManager;
+    AgendaPowerCoordinator powerCoordinator(
+        powerManager,
+        controller,
+        clock,
+        clock);
+
     bool runtimeReady = false;
     bool touchReadFailureActive = false;
     std::uint32_t lastTouchErrorReportMilliseconds = 0;

-    void drawFatalScreen(std::string_view message)
+    void renderCurrentView()
     {
-        drawSurface.clear(
-            AgendaUiColors::Background);
+        renderer.render(stateMachine.view(), drawSurface);
+    }

+    void showCurrentView()
+    {
+        renderCurrentView();
+        displayHardware.enableBacklight();
+    }
+
+    void drawFatalScreen(std::string_view message)
+    {
+        drawSurface.clear(AgendaUiColors::Background);
         drawSurface.drawCircle(
             UiPoint{120, 120},
             112,
             AgendaUiColors::Error,
             2);
-
         drawSurface.drawText(
             UiRect{34, 64, 172, 34},
             "ΣΦΑΛΜΑ",
             UiTextStyle::Heading,
             AgendaUiColors::Error,
             UiTextAlignment::Center);
-
         drawSurface.drawText(
             UiRect{30, 108, 180, 58},
             message,
@@ -97,7 +105,7 @@ namespace

     void stopWithVisibleError(
         std::string_view message,
-        const char *serialMarker)
+        const char* serialMarker)
     {
         runtimeReady = false;
         drawFatalScreen(message);
@@ -106,7 +114,7 @@ namespace
     }

     void reportTouchReadStatus(
-        const AgendaRuntimeUpdateResult &result,
+        const AgendaRuntimeUpdateResult& result,
         std::uint32_t nowMilliseconds)
     {
         if (!result.touchPolled)
@@ -124,16 +132,274 @@ namespace
             !touchReadFailureActive ||
             static_cast<std::uint32_t>(
                 nowMilliseconds -
-                lastTouchErrorReportMilliseconds) >= TouchErrorReportIntervalMilliseconds)
+                lastTouchErrorReportMilliseconds) >=
+                TouchErrorReportIntervalMilliseconds)
         {
             Serial.println("TOUCH_READ_FAILED");
-
-            lastTouchErrorReportMilliseconds =
-                nowMilliseconds;
+            lastTouchErrorReportMilliseconds = nowMilliseconds;
         }

         touchReadFailureActive = true;
     }
+
+    void reportCoordinatorError(
+        AgendaPowerCoordinatorError error)
+    {
+        switch (error)
+        {
+        case AgendaPowerCoordinatorError::None:
+            return;
+        case AgendaPowerCoordinatorError::ClockReadFailed:
+            Serial.println("POWER_CLOCK_READ_FAILED");
+            return;
+        case AgendaPowerCoordinatorError::ReminderScheduleFailed:
+            Serial.println("REMINDER_SCHEDULE_FAILED");
+            return;
+        case AgendaPowerCoordinatorError::WakeDelayFailed:
+            Serial.println("WAKE_DELAY_FAILED");
+            return;
+        }
+    }
+
+    void reportPowerHardwareError(PowerHardwareError error)
+    {
+        switch (error)
+        {
+        case PowerHardwareError::None:
+            return;
+        case PowerHardwareError::Unavailable:
+            Serial.println("POWER_HARDWARE_UNAVAILABLE");
+            return;
+        case PowerHardwareError::TouchActive:
+            Serial.println("TOUCH_ACTIVE_SLEEP_DEFERRED");
+            return;
+        case PowerHardwareError::TouchWakeConfigurationFailed:
+            Serial.println("TOUCH_WAKE_CONFIG_FAILED");
+            return;
+        case PowerHardwareError::TimerWakeConfigurationFailed:
+            Serial.println("TIMER_WAKE_CONFIG_FAILED");
+            return;
+        case PowerHardwareError::LightSleepFailed:
+            Serial.println("LIGHT_SLEEP_FAILED");
+            return;
+        case PowerHardwareError::DeepSleepReturned:
+            Serial.println("DEEP_SLEEP_FAILED");
+            return;
+        }
+    }
+
+    void recoverFromPowerFailure(
+        PowerHardwareError error,
+        std::uint32_t nowMilliseconds)
+    {
+        reportPowerHardwareError(error);
+
+        if (error == PowerHardwareError::TouchActive)
+        {
+            runtime.suppressActionsUntilRelease();
+        }
+
+        powerCoordinator.recordActivity(nowMilliseconds);
+        showCurrentView();
+    }
+
+    std::optional<std::uint64_t> timerForSleep(
+        AgendaSleepMode mode,
+        const std::optional<std::uint64_t>& requested)
+    {
+        if (
+            mode != AgendaSleepMode::Deep ||
+            DeepSleepProbeSafetyTimerMicroseconds == 0)
+        {
+            return requested;
+        }
+
+        if (
+            !requested.has_value() ||
+            DeepSleepProbeSafetyTimerMicroseconds < *requested)
+        {
+            return DeepSleepProbeSafetyTimerMicroseconds;
+        }
+
+        return requested;
+    }
+
+    void handleLightSleepWake()
+    {
+        const PowerWakeCause cause = powerHardware.wakeCause();
+        powerHardware.releaseWakePin();
+
+        const std::uint32_t nowMilliseconds =
+            static_cast<std::uint32_t>(millis());
+
+        if (cause == PowerWakeCause::Touch)
+        {
+            Serial.println("WAKE_TOUCH");
+            runtime.suppressActionsUntilRelease();
+            powerCoordinator.recordActivity(nowMilliseconds);
+            showCurrentView();
+            return;
+        }
+
+        if (cause == PowerWakeCause::Timer)
+        {
+            Serial.println("WAKE_TIMER");
+            stateMachine.tick();
+
+            if (
+                stateMachine.view().overlay ==
+                AgendaUiOverlay::Reminder)
+            {
+                powerCoordinator.recordActivity(nowMilliseconds);
+                showCurrentView();
+            }
+
+            return;
+        }
+
+        Serial.println("WAKE_UNKNOWN");
+        powerCoordinator.recordActivity(nowMilliseconds);
+        showCurrentView();
+    }
+
+    void executePowerPlan(
+        const AgendaPowerPlan& plan,
+        std::uint32_t nowMilliseconds)
+    {
+        AgendaSleepMode mode = plan.mode;
+
+        if (
+            mode == AgendaSleepMode::Deep &&
+            !DeepSleepHardwareVerified)
+        {
+            mode = AgendaSleepMode::Light;
+        }
+
+        const std::optional<std::uint64_t> timer =
+            timerForSleep(mode, plan.timerMicroseconds);
+
+        displayHardware.disableBacklight();
+
+        const PowerHardwareError prepared =
+            powerHardware.prepareWake(timer);
+
+        if (prepared != PowerHardwareError::None)
+        {
+            recoverFromPowerFailure(prepared, nowMilliseconds);
+            return;
+        }
+
+        if (mode == AgendaSleepMode::Deep)
+        {
+            Serial.println("DEEP_SLEEP");
+            Serial.flush();
+
+            const PowerHardwareError slept =
+                powerHardware.enterDeepSleep();
+
+            recoverFromPowerFailure(
+                slept,
+                static_cast<std::uint32_t>(millis()));
+            return;
+        }
+
+        Serial.println("LIGHT_SLEEP");
+        Serial.flush();
+
+        const PowerHardwareError slept =
+            powerHardware.enterLightSleep();
+
+        if (slept != PowerHardwareError::None)
+        {
+            recoverFromPowerFailure(
+                slept,
+                static_cast<std::uint32_t>(millis()));
+            return;
+        }
+
+        handleLightSleepWake();
+    }
+
+    void processPowerPlan(
+        const AgendaPowerPlan& plan,
+        std::uint32_t nowMilliseconds)
+    {
+        reportCoordinatorError(plan.error);
+
+        if (plan.reminderDueNow)
+        {
+            stateMachine.tick();
+            powerCoordinator.recordActivity(nowMilliseconds);
+            showCurrentView();
+            return;
+        }
+
+        if (plan.mode == AgendaSleepMode::None)
+        {
+            if (plan.error != AgendaPowerCoordinatorError::None)
+            {
+                powerCoordinator.recordActivity(nowMilliseconds);
+                showCurrentView();
+            }
+
+            return;
+        }
+
+        executePowerPlan(plan, nowMilliseconds);
+    }
+
+    void handleStartupWake(
+        PowerWakeCause wakeCause,
+        std::uint32_t nowMilliseconds)
+    {
+        if (wakeCause == PowerWakeCause::Touch)
+        {
+            Serial.println("WAKE_TOUCH");
+            runtime.suppressActionsUntilRelease();
+            powerCoordinator.recordActivity(nowMilliseconds);
+            showCurrentView();
+            return;
+        }
+
+        if (wakeCause == PowerWakeCause::Timer)
+        {
+            Serial.println("WAKE_TIMER");
+            stateMachine.tick();
+
+            if (
+                stateMachine.view().overlay ==
+                AgendaUiOverlay::Reminder)
+            {
+                powerCoordinator.recordActivity(nowMilliseconds);
+                showCurrentView();
+                return;
+            }
+
+            const AgendaPowerPlan plan =
+                powerCoordinator.planImmediateDeepSleep(
+                    stateMachine.view());
+
+            if (
+                DeepSleepHardwareVerified &&
+                plan.mode == AgendaSleepMode::Deep)
+            {
+                processPowerPlan(plan, nowMilliseconds);
+                return;
+            }
+
+            reportCoordinatorError(plan.error);
+            powerCoordinator.recordActivity(nowMilliseconds);
+            showCurrentView();
+            return;
+        }
+
+        if (wakeCause == PowerWakeCause::Unknown)
+        {
+            Serial.println("WAKE_UNKNOWN");
+        }
+
+        showCurrentView();
+    }
 }

 void setup()
@@ -142,46 +408,43 @@ void setup()
     delay(250);
     Serial.println("AGENDA_START");

+    const PowerWakeCause startupWakeCause =
+        powerHardware.wakeCause();
+
+    powerHardware.releaseWakePin();
+
     if (!displayHardware.begin())
     {
         Serial.println("DISPLAY_INIT_FAILED");
         return;
     }

-    if (
-        !primaryStorage.begin() ||
-        !backupStorage.begin())
+    if (!primaryStorage.begin() || !backupStorage.begin())
     {
         stopWithVisibleError(
             "ΣΦΑΛΜΑ ΑΠΟΘΗΚΕΥΣΗΣ",
             "STORAGE_INIT_FAILED");
-
         return;
     }

-    if (
-        touchHardware.begin() !=
-        TouchInputError::None)
+    if (touchHardware.begin() != TouchInputError::None)
     {
         stopWithVisibleError(
             "ΣΦΑΛΜΑ ΑΦΗΣ",
             "TOUCH_INIT_FAILED");
-
         return;
     }

     stateMachine.start();

-    renderer.render(
-        stateMachine.view(),
-        drawSurface);
-
-    displayHardware.enableBacklight();
-
-    runtime.begin(
-        static_cast<std::uint32_t>(millis()));
+    const std::uint32_t nowMilliseconds =
+        static_cast<std::uint32_t>(millis());

+    runtime.begin(nowMilliseconds);
+    powerCoordinator.begin(nowMilliseconds);
     runtimeReady = true;
+
+    handleStartupWake(startupWakeCause, nowMilliseconds);
     Serial.println("AGENDA_READY");
 }

@@ -199,9 +462,14 @@ void loop()
     const AgendaRuntimeUpdateResult result =
         runtime.update(nowMilliseconds);

-    reportTouchReadStatus(
-        result,
-        nowMilliseconds);
+    reportTouchReadStatus(result, nowMilliseconds);

+    const AgendaPowerPlan plan =
+        powerCoordinator.update(
+            nowMilliseconds,
+            result.touchActivity,
+            stateMachine.view());
+
+    processPowerPlan(plan, nowMilliseconds);
     delay(1);
-}
\ No newline at end of file
+}
```

The composition keeps the backlight off during early timer wake, consumes touch wake through release, restores the view on failures, flushes diagnostics before sleep, and converts every deep request to light sleep while the physical gate is false.

- [ ] **Step 3: Run static checks GREEN**

```powershell
$main = Get-Content .\src\platform\esp32\main.cpp -Raw
$markers = @('LIGHT_SLEEP','DEEP_SLEEP','WAKE_TOUCH','WAKE_TIMER','TOUCH_WAKE_CONFIG_FAILED','TIMER_WAKE_CONFIG_FAILED','LIGHT_SLEEP_FAILED','DEEP_SLEEP_FAILED')
foreach ($marker in $markers) {
    if ($main -notmatch [regex]::Escape($marker)) { throw "Missing marker: $marker" }
}
if ($main -notmatch 'DeepSleepHardwareVerified\s*=\s*false') { throw 'Deep sleep is not gated.' }
if ($main -notmatch 'DeepSleepProbeSafetyTimerMicroseconds\s*=\s*15000000ULL') { throw 'Probe timer is absent.' }
Write-Host 'Integration static checks passed.'
```

- [ ] **Step 4: Build firmware**

```powershell
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
& $pio run
if ($LASTEXITCODE -ne 0) { throw 'Integrated power firmware build failed.' }
```

Expected: PlatformIO SUCCESS. Keep any ESP-IDF signature fix inside the adapter.

- [ ] **Step 5: Run safety scans**

```powershell
$srcFiles = @((Get-ChildItem .\src -Recurse -File).FullName)
$checkedFiles = $srcFiles + @((Resolve-Path .\platformio.ini).Path)
$forbidden = @(Select-String -Path $checkedFiles -Pattern 'LittleFS\.begin\(true\)|\bWiFi\b|\bNTP\b|\bBluetooth\b')
if ($forbidden.Count -ne 0) { $forbidden; throw 'Forbidden feature found.' }

$coreFiles = @((Get-ChildItem .\src\core -Recurse -File).FullName)
$coreLeaks = @(Select-String -Path $coreFiles -Pattern '\bArduino\b|\bWire\b|\bLittleFS\b|\bArduino_GFX\b|\besp_sleep\b')
if ($coreLeaks.Count -ne 0) { $coreLeaks; throw 'Platform dependency in core.' }

$touchMatches = @(Select-String -Path $srcFiles -Pattern '\bTouchInterrupt\b' -AllMatches)
$touchCount = ($touchMatches | ForEach-Object { $_.Matches.Count } | Measure-Object -Sum).Sum
if ($touchCount -ne 4) { $touchMatches; throw "Expected one definition and three adapter uses; found $touchCount." }

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Whitespace check failed.' }
```

- [ ] **Step 6: Commit the gated integration**

```powershell
git add -- src/platform/esp32/main.cpp
git diff --cached --check
git commit -m "feat: integrate gated hybrid power management"
git status --short
```


---

### Task 8: Pass USB hardware gates

**Files:**
- Modify after probes: src/platform/esp32/main.cpp
- Hardware: Rev3 board on USB; battery disconnected.

**Interfaces:**
- Produces either verified production deep sleep or permanent light-sleep fallback.

- [ ] **Step 1: Stop for explicit upload approval**

Show the user the clean commit and build evidence. Do not upload until the user explicitly approves the USB mutation.

- [ ] **Step 2: Upload and test gated light sleep**

```powershell
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
& $pio run -t upload
if ($LASTEXITCODE -ne 0) { throw 'Gated upload failed.' }
& $pio device monitor -b 115200
```

Verify: after 45 seconds the backlight turns off; first touch wakes only; second touch acts normally; an unsaved distinctive T9 draft survives more than five minutes and a light-sleep touch wake.

- [ ] **Step 3: Enable probe mode without committing**

Change exactly:

```diff
-    constexpr bool DeepSleepHardwareVerified = false;
+    constexpr bool DeepSleepHardwareVerified = true;
```

Keep the 15,000,000 microsecond safety timer.

- [ ] **Step 4: Build, upload, and run five deep-wake trials**

```powershell
& $pio run
if ($LASTEXITCODE -ne 0) { throw 'Probe build failed.' }
& $pio run -t upload
if ($LASTEXITCODE -ne 0) { throw 'Probe upload failed.' }
& $pio device monitor -b 115200
```

From Month, wait five minutes for DEEP_SLEEP. Touch-wake five times before the 15-second safety timer. Each trial must print WAKE_TOUCH, restore Month, consume the wake touch, and retain stored events.

For clock continuity, note the configured local time, remain in deep sleep for at least 60 seconds across safety-timer cycles, touch-wake, open creation of a timed event, and confirm its current-time basis advanced rather than reverting to compile time.

- [ ] **Step 5A: Passing branch — remove safety timer and commit deep sleep**

Keep DeepSleepHardwareVerified=true and change:

```diff
-    constexpr std::uint64_t DeepSleepProbeSafetyTimerMicroseconds =
-        15000000ULL;
+    constexpr std::uint64_t DeepSleepProbeSafetyTimerMicroseconds = 0;
```


```powershell
& $pio run
if ($LASTEXITCODE -ne 0) { throw 'Verified deep build failed.' }
& $pio run -t upload
if ($LASTEXITCODE -ne 0) { throw 'Verified deep upload failed.' }
git add -- src/platform/esp32/main.cpp
git diff --cached --check
git commit -m "feat: enable verified ESP32 deep sleep"
git status --short
```

- [ ] **Step 5B: Failing branch — retain light sleep and commit fallback**

Execute this branch instead of Step 5A if either physical gate fails:

```cpp
constexpr bool DeepSleepHardwareVerified = false;
constexpr std::uint64_t DeepSleepProbeSafetyTimerMicroseconds = 0;
```


```powershell
& $pio run
if ($LASTEXITCODE -ne 0) { throw 'Fallback build failed.' }
& $pio run -t upload
if ($LASTEXITCODE -ne 0) { throw 'Fallback upload failed.' }
git add -- src/platform/esp32/main.cpp
git diff --cached --check
git commit -m "fix: retain safe light-sleep fallback"
git status --short
```

Execute exactly one final branch. Never ship the recurring 15-second probe timer.

---

### Task 9: Fresh regression and final acceptance

**Files:**
- Verify all source/test files.
- No new production file is expected.

**Interfaces:**
- Verifies 30 freshly compiled host suites, clean firmware build, architecture constraints, device acceptance, and Git cleanliness.

- [ ] **Step 1: Freshly compile all 30 suites**

Run this complete block. It deletes every old executable first, rebuilds all 30 suites, and rejects any zero-byte compiler output:

```powershell
Get-ChildItem .\build\test_*.exe -ErrorAction SilentlyContinue | Remove-Item

New-Item -ItemType Directory -Force build | Out-Null

$common = @(
    '-std=c++17',
    '-Wall',
    '-Wextra',
    '-pedantic',
    '-Isrc'
)

function Build-HostTest {
    param(
        [Parameter(Mandatory)]
        [string]$Output,

        [Parameter(Mandatory)]
        [string[]]$Sources
    )

    & g++ @common @Sources -o $Output

    if ($LASTEXITCODE -ne 0)
    {
        throw "Compilation failed: $Output"
    }

    if ((Get-Item $Output).Length -eq 0)
    {
        throw "Compiler produced an empty executable: $Output"
    }
}

Build-HostTest build\test_date_time.exe @(
    'tests\test_date_time.cpp',
    'src\core\date_time.cpp'
)

Build-HostTest build\test_compile_date_time.exe @(
    'tests\test_compile_date_time.cpp',
    'src\core\compile_date_time.cpp',
    'src\core\date_time.cpp'
)

Build-HostTest build\test_calendar_engine.exe @(
    'tests\test_calendar_engine.cpp',
    'src\core\date_time.cpp',
    'src\core\calendar_engine.cpp'
)

Build-HostTest build\test_utf8.exe @(
    'tests\test_utf8.cpp',
    'src\core\utf8.cpp'
)

Build-HostTest build\test_event.exe @(
    'tests\test_event.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp'
)

Build-HostTest build\test_t9_engine.exe @(
    'tests\test_t9_engine.cpp',
    'src\core\utf8.cpp',
    'src\core\t9_engine.cpp'
)

Build-HostTest build\test_agenda_state.exe @(
    'tests\test_agenda_state.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp'
)

Build-HostTest build\test_agenda_manager.exe @(
    'tests\test_agenda_manager.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp'
)

Build-HostTest build\test_crc32.exe @(
    'tests\test_crc32.cpp',
    'src\core\crc32.cpp'
)

Build-HostTest build\test_event_codec.exe @(
    'tests\test_event_codec.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp'
)

Build-HostTest build\test_desktop_file_storage.exe @(
    'tests\test_desktop_file_storage.cpp',
    'src\platform\desktop_file_storage.cpp'
)

Build-HostTest build\test_agenda_repository.exe @(
    'tests\test_agenda_repository.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp'
)

Build-HostTest build\test_reminder_engine.exe @(
    'tests\test_reminder_engine.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\reminder_engine.cpp'
)

Build-HostTest build\test_agenda_controller.exe @(
    'tests\test_agenda_controller.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp'
)

Build-HostTest build\test_agenda_persistence.exe @(
    'tests\test_agenda_persistence.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\platform\desktop_file_storage.cpp'
)

Build-HostTest build\test_agenda_ui_state_machine.exe @(
    'tests\test_agenda_ui_state_machine.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\core\calendar_engine.cpp',
    'src\core\t9_engine.cpp',
    'src\core\agenda_ui_state_machine.cpp'
)

Build-HostTest build\test_agenda_ui_persistence.exe @(
    'tests\test_agenda_ui_persistence.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\core\calendar_engine.cpp',
    'src\core\t9_engine.cpp',
    'src\core\agenda_ui_state_machine.cpp',
    'src\platform\desktop_file_storage.cpp'
)

Build-HostTest build\test_agenda_renderer.exe @(
    'tests\test_agenda_renderer.cpp',
    'src\ui\agenda_renderer.cpp',
    'src\core\utf8.cpp',
    'src\core\date_time.cpp',
    'src\core\calendar_engine.cpp'
)

Build-HostTest build\test_agenda_touch_mapper.exe @(
    'tests\test_agenda_touch_mapper.cpp',
    'src\ui\agenda_touch_mapper.cpp'
)

Build-HostTest build\test_touch_release_detector.exe @(
    'tests\test_touch_release_detector.cpp',
    'src\ui\touch_release_detector.cpp'
)

Build-HostTest build\test_draw_surface_math.exe @(
    'tests\test_draw_surface_math.cpp'
)

Build-HostTest build\test_esp32_clock.exe @(
    'tests\test_esp32_clock.cpp',
    'src\platform\esp32\esp32_clock.cpp',
    'src\core\date_time.cpp'
)

Build-HostTest build\test_littlefs_byte_storage.exe @(
    'tests\test_littlefs_byte_storage.cpp',
    'src\platform\esp32\littlefs_byte_storage.cpp'
)

Build-HostTest build\test_cst816s_protocol.exe @(
    'tests\test_cst816s_protocol.cpp'
)

Build-HostTest build\test_touch_sample.exe @(
    'tests\test_touch_sample.cpp',
    'src\touch_sample.cpp'
)

Build-HostTest build\test_cst816s_touch_hardware.exe @(
    'tests\test_cst816s_touch_hardware.cpp',
    'src\platform\esp32\touch_hardware.cpp',
    'src\touch_sample.cpp'
)

$runtimeCore = @(
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\core\calendar_engine.cpp',
    'src\core\t9_engine.cpp',
    'src\core\agenda_ui_state_machine.cpp'
)

$runtimeUi = @(
    'src\ui\agenda_renderer.cpp',
    'src\ui\agenda_touch_mapper.cpp',
    'src\ui\touch_release_detector.cpp',
    'src\ui\agenda_runtime.cpp'
)

Build-HostTest build\test_agenda_runtime.exe @(
    'tests\test_agenda_runtime.cpp'
    $runtimeCore
    $runtimeUi
)

Build-HostTest build\test_agenda_power_manager.exe @(
    'tests\test_agenda_power_manager.cpp',
    'src\ui\agenda_power_manager.cpp'
)

$coordinatorCore = @(
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp'
)

Build-HostTest build\test_agenda_power_coordinator.exe @(
    'tests\test_agenda_power_coordinator.cpp'
    $coordinatorCore
    'src\ui\agenda_power_manager.cpp'
    'src\ui\agenda_power_coordinator.cpp'
)

Build-HostTest build\test_esp32_power_hardware.exe @(
    'tests\test_esp32_power_hardware.cpp',
    'src\platform\esp32\power_hardware.cpp'
)

$compiledTests = @(Get-ChildItem .\build\test_*.exe)

if ($compiledTests.Count -ne 30)
{
    throw "Expected 30 tests; found $($compiledTests.Count)."
}

Write-Host "Freshly compiled desktop tests: $($compiledTests.Count)"
```

Expected: the final count is 30 and every compiler invocation returns exit code 0.

- [ ] **Step 2: Run all 30 freshly compiled executables**

```powershell
$executedTests = 0
Get-ChildItem .\build\test_*.exe | Sort-Object Name | ForEach-Object {
    Write-Host "Running $($_.Name)"
    & $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "$($_.Name) failed." }
    ++$executedTests
}
if ($executedTests -ne 30) { throw "Expected 30; ran $executedTests." }
Write-Host "Executed desktop tests successfully: $executedTests"
```

- [ ] **Step 3: Clean-build firmware and run final checks**

```powershell
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
& $pio run -t clean
if ($LASTEXITCODE -ne 0) { throw 'PlatformIO clean failed.' }
& $pio run
if ($LASTEXITCODE -ne 0) { throw 'Clean PlatformIO build failed.' }

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Whitespace check failed.' }

$changes = @(git status --short)
if ($changes.Count -ne 0) { $changes; throw 'Final worktree is not clean.' }

git log --oneline -12
Write-Host 'Final Git status: clean.'
```

- [ ] **Step 4: Complete USB acceptance**

With the battery disconnected, verify:
1. 45 seconds idle gives LIGHT_SLEEP and backlight off.
2. First touch only wakes; second touch works.
3. Five minutes in Month gives DEEP_SLEEP only if Task 8 passed.
4. Touch wake reconstructs Month and retains events.
5. Editing remains light-sleep-only and preserves draft.
6. Early timer wake does not flash or show a false reminder.
7. A due reminder lights the screen and remains until OK.
8. OK persists acknowledgement and resets inactivity.
9. No unexpected storage, clock, touch, or power failure marker appears.

Record the physical result in the execution conversation. Battery current, charging, endurance, and enclosure installation remain separate battery-stage work.
