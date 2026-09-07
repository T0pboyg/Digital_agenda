# Reminder Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add fully offline, one-day-before reminders that trigger at 17:00, survive missed trigger times, and are acknowledged transactionally.

**Architecture:** A stateless reminder engine selects the first due event from the ordered `AgendaManager`. `AgendaController` exposes a safe event copy and persists user acknowledgement with the same snapshot/save/rollback transaction used by other mutations.

**Tech Stack:** C++17, standard library only, existing `AgendaManager`, `AgendaController`, `AgendaRepository`, binary codec, desktop file storage, and `assert`-based test executables built with g++.

**Spec:** `docs/superpowers/specs/2026-09-07-reminder-engine-design.md`

## Global Constraints

- Timed and all-day events both become eligible at 17:00 on the previous calendar day.
- A missed reminder remains due until the event becomes past.
- Timed events become past at their exact start minute; all-day events become past on the following day.
- Return due reminders one at a time in existing agenda order.
- Set `reminderSeen` only after explicit acknowledgement.
- A successful event edit continues to reset `reminderSeen` to `false`.
- Invalid current date or time must return an explicit error and no reminder.
- All reminder logic remains offline and independent of UI, touch, RTC, ESP32 timers, and network services.
- Do not change the binary codec or persistence format; `reminderSeen` is already encoded.
- Compile every changed test with `-std=c++17 -Wall -Wextra -pedantic -Isrc`.
- Use test-driven development and one focused commit per task.

## File Structure

- Create `src/core/reminder_engine.h`: public reminder result types and stateless lookup function.
- Create `src/core/reminder_engine.cpp`: clock validation, threshold calculation, due filtering, and ordered selection.
- Create `tests/test_reminder_engine.cpp`: timing, ordering, boundary, seen-state, and invalid-clock unit tests.
- Modify `src/core/agenda_manager.h`: declare reminder acknowledgement mutation.
- Modify `src/core/agenda_manager.cpp`: mark an existing event as seen without altering other state.
- Modify `tests/test_agenda_manager.cpp`: verify acknowledgement and missing-ID behavior.
- Modify `src/core/agenda_controller.h`: expose reminder lookup and acknowledgement APIs.
- Modify `src/core/agenda_controller.cpp`: delegate lookup and transactionally persist acknowledgement.
- Modify `tests/test_agenda_controller.cpp`: verify controller results, read-only lookup, persistence, rollback, and one-at-a-time flow.
- Modify `tests/test_agenda_persistence.cpp`: verify acknowledgement survives a real file close and reopen.

---

### Task 1: Stateless Reminder Calculation

**Files:**
- Create: `src/core/reminder_engine.h`
- Create: `src/core/reminder_engine.cpp`
- Create: `tests/test_reminder_engine.cpp`

**Interfaces:**
- Consumes: `AgendaManager::events() const`, `isValidDate(const Date&)`, `isValidTime(const TimeOfDay&)`, `serialDay(const Date&)`, `serialMinute(const DateTime&)`, and `isPast(const Event&, const DateTime&)`.
- Produces: `ReminderEngineError`, `ReminderCheckResult`, and `findNextDueReminder(const AgendaManager&, const DateTime&)` for Task 3.

- [ ] **Step 1: Write the complete failing reminder-engine test file**

Create `tests/test_reminder_engine.cpp`:

```cpp
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <vector>

#include "core/reminder_engine.h"

namespace
{
Event reminderEvent(
    std::uint32_t id,
    const Date& date,
    const std::optional<TimeOfDay>& time,
    bool seen = false
)
{
    return Event{
        id,
        date,
        time,
        "Reminder event",
        seen,
        DateTime{
            Date{2026, 9, 1},
            TimeOfDay{9, 0}
        }
    };
}

AgendaManager reminderAgenda(
    std::initializer_list<Event> events,
    std::uint32_t nextId = 100
)
{
    AgendaManager agenda;

    const AgendaState state{
        std::vector<Event>(events),
        nextId
    };

    assert(agenda.restoreState(state));
    return agenda;
}

void assertNoReminder(
    const ReminderCheckResult& result
)
{
    assert(result.error == ReminderEngineError::None);
    assert(!result.eventId.has_value());
}

void test_empty_agenda_has_no_reminder()
{
    const AgendaManager agenda;

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        )
    );
}

void test_reminder_is_not_due_before_1700()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            1,
            Date{2026, 9, 8},
            TimeOfDay{12, 30}
        )
    });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{16, 59}
            }
        )
    );
}

void test_reminder_is_due_exactly_at_1700()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            4,
            Date{2026, 9, 8},
            TimeOfDay{12, 30}
        )
    });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(result.error == ReminderEngineError::None);
    assert(result.eventId == 4);
}

void test_late_evening_reminder_is_due()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            5,
            Date{2026, 9, 8},
            std::nullopt
        )
    });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{23, 59}
            }
        );

    assert(result.error == ReminderEngineError::None);
    assert(result.eventId == 5);
}

void test_missed_timed_reminder_is_due_before_start()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            6,
            Date{2026, 9, 8},
            TimeOfDay{12, 30}
        )
    });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{12, 29}
            }
        );

    assert(result.error == ReminderEngineError::None);
    assert(result.eventId == 6);
}

void test_timed_event_is_not_due_at_or_after_start()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            7,
            Date{2026, 9, 8},
            TimeOfDay{12, 30}
        )
    });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{12, 30}
            }
        )
    );

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{12, 31}
            }
        )
    );
}

void test_all_day_event_remains_due_on_event_day()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            8,
            Date{2026, 9, 8},
            std::nullopt
        )
    });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{23, 59}
            }
        );

    assert(result.error == ReminderEngineError::None);
    assert(result.eventId == 8);
}

void test_all_day_event_is_not_due_next_day()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            9,
            Date{2026, 9, 8},
            std::nullopt
        )
    });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 9},
                TimeOfDay{0, 0}
            }
        )
    );
}

void test_seen_event_is_skipped()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            10,
            Date{2026, 9, 8},
            std::nullopt,
            true
        )
    });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{18, 0}
            }
        )
    );
}

void test_first_due_event_uses_agenda_order()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            30,
            Date{2026, 9, 8},
            TimeOfDay{9, 0}
        ),
        reminderEvent(
            20,
            Date{2026, 9, 8},
            std::nullopt
        ),
        reminderEvent(
            10,
            Date{2026, 9, 8},
            TimeOfDay{8, 0}
        )
    }, 31);

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{18, 0}
            }
        );

    assert(result.error == ReminderEngineError::None);
    assert(result.eventId == 20);
}

void test_month_year_and_leap_boundaries()
{
    const AgendaManager january = reminderAgenda({
        reminderEvent(
            40,
            Date{2027, 1, 1},
            std::nullopt
        )
    });

    assert(
        findNextDueReminder(
            january,
            DateTime{
                Date{2026, 12, 31},
                TimeOfDay{17, 0}
            }
        ).eventId == 40
    );

    const AgendaManager leapYear = reminderAgenda({
        reminderEvent(
            41,
            Date{2028, 3, 1},
            std::nullopt
        )
    });

    assert(
        findNextDueReminder(
            leapYear,
            DateTime{
                Date{2028, 2, 29},
                TimeOfDay{17, 0}
            }
        ).eventId == 41
    );
}

void test_first_supported_date_uses_numeric_threshold()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            42,
            Date{2000, 1, 1},
            std::nullopt
        )
    }, 43);

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2000, 1, 1},
                TimeOfDay{0, 0}
            }
        );

    assert(result.error == ReminderEngineError::None);
    assert(result.eventId == 42);
}

void test_invalid_current_date_or_time_is_rejected()
{
    const AgendaManager agenda = reminderAgenda({
        reminderEvent(
            50,
            Date{2026, 9, 8},
            std::nullopt
        )
    });

    const ReminderCheckResult invalidDate =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 2, 30},
                TimeOfDay{17, 0}
            }
        );

    assert(
        invalidDate.error ==
        ReminderEngineError::InvalidDateTime
    );
    assert(!invalidDate.eventId.has_value());

    const ReminderCheckResult invalidTime =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{24, 0}
            }
        );

    assert(
        invalidTime.error ==
        ReminderEngineError::InvalidDateTime
    );
    assert(!invalidTime.eventId.has_value());
}
}

int main()
{
    test_empty_agenda_has_no_reminder();
    test_reminder_is_not_due_before_1700();
    test_reminder_is_due_exactly_at_1700();
    test_late_evening_reminder_is_due();
    test_missed_timed_reminder_is_due_before_start();
    test_timed_event_is_not_due_at_or_after_start();
    test_all_day_event_remains_due_on_event_day();
    test_all_day_event_is_not_due_next_day();
    test_seen_event_is_skipped();
    test_first_due_event_uses_agenda_order();
    test_month_year_and_leap_boundaries();
    test_first_supported_date_uses_numeric_threshold();
    test_invalid_current_date_or_time_is_rejected();

    std::cout
        << "All reminder engine tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Run the focused build and confirm the RED state**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_reminder_engine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\reminder_engine.cpp -o build\test_reminder_engine.exe
```

Expected: compilation fails because `reminder_engine.h` and its implementation do not exist yet.

- [ ] **Step 3: Add the public reminder-engine interface**

Create `src/core/reminder_engine.h`:

```cpp
#pragma once

#include <cstdint>
#include <optional>

#include "agenda_manager.h"

enum class ReminderEngineError
{
    None,
    InvalidDateTime
};

struct ReminderCheckResult
{
    ReminderEngineError error;
    std::optional<std::uint32_t> eventId;
};

ReminderCheckResult findNextDueReminder(
    const AgendaManager& agenda,
    const DateTime& now
);
```

- [ ] **Step 4: Implement the minimum complete due calculation**

Create `src/core/reminder_engine.cpp`:

```cpp
#include "reminder_engine.h"

ReminderCheckResult findNextDueReminder(
    const AgendaManager& agenda,
    const DateTime& now
)
{
    if (
        !isValidDate(now.date) ||
        !isValidTime(now.time)
    )
    {
        return ReminderCheckResult{
            ReminderEngineError::InvalidDateTime,
            std::nullopt
        };
    }

    const long long nowMinute =
        serialMinute(now);

    for (const Event& event : agenda.events())
    {
        if (
            event.reminderSeen ||
            isPast(event, now)
        )
        {
            continue;
        }

        const long long reminderMinute =
            (
                static_cast<long long>(
                    serialDay(event.eventDate)
                ) - 1LL
            ) * 1440LL + 17LL * 60LL;

        if (nowMinute >= reminderMinute)
        {
            return ReminderCheckResult{
                ReminderEngineError::None,
                event.id
            };
        }
    }

    return ReminderCheckResult{
        ReminderEngineError::None,
        std::nullopt
    };
}
```

- [ ] **Step 5: Build and run the focused suite**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_reminder_engine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\reminder_engine.cpp -o build\test_reminder_engine.exe
.\build\test_reminder_engine.exe
```

Expected:

```text
All reminder engine tests passed.
```

- [ ] **Step 6: Commit the stateless engine**

```powershell
git add src/core/reminder_engine.h src/core/reminder_engine.cpp tests/test_reminder_engine.cpp
git diff --cached --check
git commit -m "feat: add reminder due calculation"
```

---

### Task 2: AgendaManager Reminder Acknowledgement

**Files:**
- Modify: `src/core/agenda_manager.h`
- Modify: `src/core/agenda_manager.cpp`
- Modify: `tests/test_agenda_manager.cpp`

**Interfaces:**
- Consumes: existing `Event::reminderSeen`, `AgendaManager::findEvent`, `AgendaManager::snapshot`, and manager-owned event storage.
- Produces: `bool AgendaManager::markReminderSeen(std::uint32_t id)` for Task 4.

- [ ] **Step 1: Add failing manager tests**

Insert these functions before `main()` in `tests/test_agenda_manager.cpp`:

```cpp
void test_mark_reminder_seen_changes_only_target()
{
    AgendaManager agenda;

    const AgendaState initial{
        {
            Event{
                3,
                Date{2026, 9, 8},
                std::nullopt,
                "All day",
                false,
                DateTime{
                    Date{2026, 9, 1},
                    TimeOfDay{8, 15}
                }
            },
            Event{
                7,
                Date{2026, 9, 8},
                TimeOfDay{12, 30},
                "Timed",
                false,
                DateTime{
                    Date{2026, 9, 2},
                    TimeOfDay{9, 45}
                }
            }
        },
        11
    };

    assert(agenda.restoreState(initial));
    assert(agenda.markReminderSeen(7));

    const AgendaState after = agenda.snapshot();

    assert(after.nextId == 11);
    assert(after.events.size() == 2);

    const Event* untouched = agenda.findEvent(3);
    const Event* changed = agenda.findEvent(7);

    assert(untouched != nullptr);
    assert(changed != nullptr);
    assert(!untouched->reminderSeen);
    assert(changed->reminderSeen);
    assert(changed->text == "Timed");
    assert(changed->eventDate.year == 2026);
    assert(changed->eventDate.month == 9);
    assert(changed->eventDate.day == 8);
    assert(changed->eventTime.has_value());
    assert(changed->eventTime->hour == 12);
    assert(changed->eventTime->minute == 30);
    assert(changed->createdAt.date.day == 2);
    assert(changed->createdAt.time.hour == 9);
    assert(changed->createdAt.time.minute == 45);
}

void test_mark_missing_reminder_does_not_change_state()
{
    AgendaManager agenda;

    const CreateEventResult created =
        agenda.createEvent(
            validDraft("Existing"),
            testNow()
        );

    assert(created.error == EventError::None);

    const AgendaState before = agenda.snapshot();

    assert(!agenda.markReminderSeen(999));

    const AgendaState after = agenda.snapshot();

    assert(after.nextId == before.nextId);
    assert(after.events.size() == before.events.size());
    assert(after.events[0].id == before.events[0].id);
    assert(after.events[0].text == before.events[0].text);
    assert(
        after.events[0].reminderSeen ==
        before.events[0].reminderSeen
    );
}
```

Register both functions in `main()` before the success message:

```cpp
test_mark_reminder_seen_changes_only_target();
test_mark_missing_reminder_does_not_change_state();
```

- [ ] **Step 2: Run the manager build and confirm RED**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_manager.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp -o build\test_agenda_manager.exe
```

Expected: compilation fails because `AgendaManager::markReminderSeen` is not declared.

- [ ] **Step 3: Declare the manager mutation**

Add after `deleteEvent` in `src/core/agenda_manager.h`:

```cpp
bool markReminderSeen(std::uint32_t id);
```

- [ ] **Step 4: Implement the manager mutation**

Add after `AgendaManager::deleteEvent` in `src/core/agenda_manager.cpp`:

```cpp
bool AgendaManager::markReminderSeen(
    std::uint32_t id
)
{
    const auto iterator = std::find_if(
        events_.begin(),
        events_.end(),
        [id](const Event& event)
        {
            return event.id == id;
        }
    );

    if (iterator == events_.end())
    {
        return false;
    }

    iterator->reminderSeen = true;
    return true;
}
```

- [ ] **Step 5: Build and run the manager and reminder suites**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_manager.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp -o build\test_agenda_manager.exe
.\build\test_agenda_manager.exe

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_reminder_engine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\reminder_engine.cpp -o build\test_reminder_engine.exe
.\build\test_reminder_engine.exe
```

Expected:

```text
All agenda manager tests passed.
All reminder engine tests passed.
```

- [ ] **Step 6: Commit manager acknowledgement**

```powershell
git add src/core/agenda_manager.h src/core/agenda_manager.cpp tests/test_agenda_manager.cpp
git diff --cached --check
git commit -m "feat: add reminder acknowledgement to agenda manager"
```

---

### Task 3: Read-Only Reminder Lookup Through AgendaController

**Files:**
- Modify: `src/core/agenda_controller.h`
- Modify: `src/core/agenda_controller.cpp`
- Modify: `tests/test_agenda_controller.cpp`

**Interfaces:**
- Consumes: `ReminderCheckResult findNextDueReminder(const AgendaManager&, const DateTime&)` from Task 1.
- Produces: `AgendaControllerReminderResult` and `AgendaController::nextReminder(const DateTime&) const` for the future UI and Task 5.

- [ ] **Step 1: Add the controller reminder result and declaration to the test compile surface**

Add these tests before the closing anonymous-namespace brace in `tests/test_agenda_controller.cpp`:

```cpp
void test_reminder_lookup_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );
    assert(
        result.reminderError ==
        ReminderEngineError::None
    );
    assert(!result.event.has_value());
    assert(storage.writeCount == 0);
}

void test_invalid_clock_is_reported_by_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{24, 0}
            }
        );

    assert(result.error == AgendaControllerError::None);
    assert(
        result.reminderError ==
        ReminderEngineError::InvalidDateTime
    );
    assert(!result.event.has_value());
    assert(storage.writeCount == 0);
}

void test_due_reminder_returns_safe_copy_without_save()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        7,
                        Date{2026, 9, 8},
                        TimeOfDay{12, 30},
                        "Dentist",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    }
                },
                8
            }
        )
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const std::size_t writesBefore =
        storage.writeCount;

    AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(result.error == AgendaControllerError::None);
    assert(
        result.reminderError ==
        ReminderEngineError::None
    );
    assert(result.event.has_value());
    assert(result.event->id == 7);
    assert(result.event->text == "Dentist");
    assert(storage.writeCount == writesBefore);

    result.event->text = "Changed copy";

    assert(
        controller.agenda().findEvent(7)->text ==
        "Dentist"
    );
}

void test_controller_reports_no_due_reminder()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(result.error == AgendaControllerError::None);
    assert(
        result.reminderError ==
        ReminderEngineError::None
    );
    assert(!result.event.has_value());
    assert(storage.writeCount == 0);
}
```

Register the four tests in `main()`:

```cpp
test_reminder_lookup_requires_ready_controller();
test_invalid_clock_is_reported_by_controller();
test_due_reminder_returns_safe_copy_without_save();
test_controller_reports_no_due_reminder();
```

- [ ] **Step 2: Build and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
```

Expected: compilation fails because `AgendaControllerReminderResult` and `nextReminder` do not exist.

- [ ] **Step 3: Add the controller reminder interface**

Add `#include <optional>` and `#include "reminder_engine.h"` to `src/core/agenda_controller.h`.

Add after `AgendaControllerChangeResult`:

```cpp
struct AgendaControllerReminderResult
{
    AgendaControllerError error;
    ReminderEngineError reminderError;
    std::optional<Event> event;
};
```

Add after `agenda() const`:

```cpp
AgendaControllerReminderResult nextReminder(
    const DateTime& now
) const;
```

- [ ] **Step 4: Implement read-only lookup**

Add after `AgendaController::agenda()` in `src/core/agenda_controller.cpp`:

```cpp
AgendaControllerReminderResult
AgendaController::nextReminder(
    const DateTime& now
) const
{
    if (!ready_)
    {
        return AgendaControllerReminderResult{
            AgendaControllerError::NotReady,
            ReminderEngineError::None,
            std::nullopt
        };
    }

    const ReminderCheckResult checked =
        findNextDueReminder(agenda_, now);

    if (checked.error != ReminderEngineError::None)
    {
        return AgendaControllerReminderResult{
            AgendaControllerError::None,
            checked.error,
            std::nullopt
        };
    }

    if (!checked.eventId.has_value())
    {
        return AgendaControllerReminderResult{
            AgendaControllerError::None,
            ReminderEngineError::None,
            std::nullopt
        };
    }

    const Event* event =
        agenda_.findEvent(*checked.eventId);

    if (event == nullptr)
    {
        return AgendaControllerReminderResult{
            AgendaControllerError::InvalidData,
            ReminderEngineError::None,
            std::nullopt
        };
    }

    return AgendaControllerReminderResult{
        AgendaControllerError::None,
        ReminderEngineError::None,
        *event
    };
}
```

- [ ] **Step 5: Build and run controller, engine, and manager suites**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
.\build\test_agenda_controller.exe
.\build\test_reminder_engine.exe
.\build\test_agenda_manager.exe
```

Expected:

```text
All agenda controller tests passed.
All reminder engine tests passed.
All agenda manager tests passed.
```

- [ ] **Step 6: Commit read-only controller lookup**

```powershell
git add src/core/agenda_controller.h src/core/agenda_controller.cpp tests/test_agenda_controller.cpp
git diff --cached --check
git commit -m "feat: expose due reminders through controller"
```

---

### Task 4: Transactional Reminder Acknowledgement

**Files:**
- Modify: `src/core/agenda_controller.h`
- Modify: `src/core/agenda_controller.cpp`
- Modify: `tests/test_agenda_controller.cpp`

**Interfaces:**
- Consumes: `bool AgendaManager::markReminderSeen(std::uint32_t)` from Task 2 and existing repository error mapping and rollback pattern.
- Produces: `AgendaController::acknowledgeReminder(std::uint32_t)` for the future UI and Task 5.

- [ ] **Step 1: Add failing acknowledgement tests**

Add these tests before the closing anonymous-namespace brace in `tests/test_agenda_controller.cpp`:

```cpp
void test_acknowledgement_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(1);

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );
    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 0);
}

void test_missing_acknowledgement_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(999);

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::EventNotFound);
    assert(storage.writeCount == 0);
}

void test_successful_acknowledgement_is_saved()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        7,
                        Date{2026, 9, 8},
                        std::nullopt,
                        "All day",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    }
                },
                8
            }
        )
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(7);

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 1);
    assert(controller.agenda().findEvent(7)->reminderSeen);

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(decoded.error == EventCodecError::None);
    assert(decoded.state.events.size() == 1);
    assert(decoded.state.events[0].reminderSeen);
}

void test_acknowledgement_write_failure_restores_exact_state()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(rollbackControllerState())
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(7);

    assert(
        result.error ==
        AgendaControllerError::WriteFailed
    );
    assert(result.eventError == EventError::None);

    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
}

void test_acknowledgement_read_failure_restores_state()
{
    TestByteStorage primary;
    TestByteStorage backup;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        7,
                        Date{2026, 9, 8},
                        std::nullopt,
                        "All day",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    }
                },
                8
            }
        )
    };

    AgendaRepository repository(primary, backup);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    primary.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(7);

    assert(
        result.error ==
        AgendaControllerError::ReadFailed
    );
    assert(result.eventError == EventError::None);
    assert(primary.writeCount == 0);
    assert(backup.writeCount == 0);

    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
}
```

Register all five tests in `main()`:

```cpp
test_acknowledgement_requires_ready_controller();
test_missing_acknowledgement_does_not_save();
test_successful_acknowledgement_is_saved();
test_acknowledgement_write_failure_restores_exact_state();
test_acknowledgement_read_failure_restores_state();
```

- [ ] **Step 2: Build and confirm RED**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
```

Expected: compilation fails because `acknowledgeReminder` is not declared.

- [ ] **Step 3: Declare acknowledgement**

Add after `deleteEvent` in `src/core/agenda_controller.h`:

```cpp
AgendaControllerChangeResult acknowledgeReminder(
    std::uint32_t eventId
);
```

- [ ] **Step 4: Implement the transactional mutation**

Add after `AgendaController::deleteEvent` in `src/core/agenda_controller.cpp`:

```cpp
AgendaControllerChangeResult
AgendaController::acknowledgeReminder(
    std::uint32_t eventId
)
{
    if (!ready_)
    {
        return AgendaControllerChangeResult{
            AgendaControllerError::NotReady,
            EventError::None
        };
    }

    const AgendaState previousState =
        agenda_.snapshot();

    if (!agenda_.markReminderSeen(eventId))
    {
        return AgendaControllerChangeResult{
            AgendaControllerError::None,
            EventError::EventNotFound
        };
    }

    const AgendaRepositoryError saveError =
        repository_.save(agenda_.snapshot());

    if (saveError != AgendaRepositoryError::None)
    {
        const bool restored =
            agenda_.restoreState(previousState);

        assert(restored);
        (void)restored;

        return AgendaControllerChangeResult{
            mapRepositoryError(saveError),
            EventError::None
        };
    }

    return AgendaControllerChangeResult{
        AgendaControllerError::None,
        EventError::None
    };
}
```

- [ ] **Step 5: Build and run the controller suite**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
.\build\test_agenda_controller.exe
```

Expected:

```text
All agenda controller tests passed.
```

- [ ] **Step 6: Commit transactional acknowledgement**

```powershell
git add src/core/agenda_controller.h src/core/agenda_controller.cpp tests/test_agenda_controller.cpp
git diff --cached --check
git commit -m "feat: persist reminder acknowledgement"
```

---

### Task 5: One-at-a-Time Flow and Real Persistence

**Files:**
- Modify: `tests/test_agenda_controller.cpp`
- Modify: `tests/test_agenda_persistence.cpp`

**Interfaces:**
- Consumes: `nextReminder(const DateTime&) const` and `acknowledgeReminder(std::uint32_t)` from Tasks 3 and 4.
- Produces: end-to-end evidence that reminders advance one at a time and an acknowledged reminder stays hidden after reopening real storage.

- [ ] **Step 1: Add the one-at-a-time controller test**

Add before the closing anonymous-namespace brace in `tests/test_agenda_controller.cpp`:

```cpp
void test_due_reminders_advance_one_at_a_time()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        1,
                        Date{2026, 9, 8},
                        TimeOfDay{9, 0},
                        "Timed",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    },
                    Event{
                        2,
                        Date{2026, 9, 8},
                        std::nullopt,
                        "All day",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{11, 0}
                        }
                    }
                },
                3
            }
        )
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const DateTime now{
        Date{2026, 9, 7},
        TimeOfDay{18, 0}
    };

    const AgendaControllerReminderResult first =
        controller.nextReminder(now);

    assert(first.event.has_value());
    assert(first.event->id == 2);

    assert(
        controller.acknowledgeReminder(2).error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult second =
        controller.nextReminder(now);

    assert(second.event.has_value());
    assert(second.event->id == 1);

    assert(
        controller.acknowledgeReminder(1).error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult finished =
        controller.nextReminder(now);

    assert(finished.error == AgendaControllerError::None);
    assert(
        finished.reminderError ==
        ReminderEngineError::None
    );
    assert(!finished.event.has_value());
    assert(storage.writeCount == 2);
}
```

Register it in `main()`:

```cpp
test_due_reminders_advance_one_at_a_time();
```

- [ ] **Step 2: Extend the persistence test dependencies and add a real reopen scenario**

Replace the `AgendaManager` include in `tests/test_agenda_persistence.cpp` with both controller and manager includes:

```cpp
#include "core/agenda_controller.h"
#include "core/agenda_manager.h"
```

Add before `main()`:

```cpp
void test_acknowledged_reminder_stays_hidden_after_reopen()
{
    resetTestFiles();

    const std::filesystem::path primaryPath =
        TEST_ROOT / "agenda.bin";

    const std::filesystem::path backupPath =
        TEST_ROOT / "agenda.bak";

    DesktopFileStorage primary(primaryPath);
    DesktopFileStorage backup(backupPath);
    AgendaRepository repository(primary, backup);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            persistenceDraft("Reminder"),
            persistenceNow()
        );

    assert(created.error == AgendaControllerError::None);
    assert(created.eventError == EventError::None);

    const DateTime reminderNow{
        Date{2026, 9, 7},
        TimeOfDay{17, 0}
    };

    const AgendaControllerReminderResult due =
        controller.nextReminder(reminderNow);

    assert(due.error == AgendaControllerError::None);
    assert(
        due.reminderError ==
        ReminderEngineError::None
    );
    assert(due.event.has_value());
    assert(due.event->id == created.eventId);

    const AgendaControllerChangeResult acknowledged =
        controller.acknowledgeReminder(created.eventId);

    assert(
        acknowledged.error ==
        AgendaControllerError::None
    );
    assert(acknowledged.eventError == EventError::None);

    DesktopFileStorage reopenedPrimary(primaryPath);
    DesktopFileStorage reopenedBackup(backupPath);

    AgendaRepository reopenedRepository(
        reopenedPrimary,
        reopenedBackup
    );

    AgendaController reopenedController(
        reopenedRepository
    );

    assert(
        reopenedController.start().error ==
        AgendaControllerError::None
    );

    const Event* reopenedEvent =
        reopenedController.agenda().findEvent(
            created.eventId
        );

    assert(reopenedEvent != nullptr);
    assert(reopenedEvent->reminderSeen);

    const AgendaControllerReminderResult afterReopen =
        reopenedController.nextReminder(reminderNow);

    assert(afterReopen.error == AgendaControllerError::None);
    assert(
        afterReopen.reminderError ==
        ReminderEngineError::None
    );
    assert(!afterReopen.event.has_value());

    resetTestFiles();
}
```

Register it in the persistence `main()`:

```cpp
test_acknowledged_reminder_stays_hidden_after_reopen();
```

- [ ] **Step 3: Build and run the two integration-facing suites**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_persistence.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\platform\desktop_file_storage.cpp -o build\test_agenda_persistence.exe

.\build\test_agenda_controller.exe
.\build\test_agenda_persistence.exe
```

Expected:

```text
All agenda controller tests passed.
All agenda persistence tests passed.
```

- [ ] **Step 4: Commit the flow and persistence coverage**

```powershell
git add tests/test_agenda_controller.cpp tests/test_agenda_persistence.cpp
git diff --cached --check
git commit -m "test: add reminder persistence coverage"
```

---

### Task 6: Full Regression Verification

**Files:**
- Verify only; no planned source changes.

**Interfaces:**
- Consumes: all completed reminder tasks and every pre-existing core/storage module.
- Produces: warning-free build and runtime evidence for all 14 suites.

- [ ] **Step 1: Rebuild every suite with warnings enabled**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_date_time.cpp src\core\date_time.cpp -o build\test_date_time.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_calendar_engine.cpp src\core\date_time.cpp src\core\calendar_engine.cpp -o build\test_calendar_engine.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_utf8.cpp src\core\utf8.cpp -o build\test_utf8.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_event.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp -o build\test_event.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_t9_engine.cpp src\core\utf8.cpp src\core\t9_engine.cpp -o build\test_t9_engine.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_manager.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp -o build\test_agenda_manager.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_state.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp -o build\test_agenda_state.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_crc32.cpp src\core\crc32.cpp -o build\test_crc32.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_event_codec.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\crc32.cpp src\core\event_codec.cpp -o build\test_event_codec.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_desktop_file_storage.cpp src\platform\desktop_file_storage.cpp -o build\test_desktop_file_storage.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_repository.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp -o build\test_agenda_repository.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_reminder_engine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\reminder_engine.cpp -o build\test_reminder_engine.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_persistence.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\platform\desktop_file_storage.cpp -o build\test_agenda_persistence.exe
```

Expected: all 14 commands exit successfully with no compiler warnings.

- [ ] **Step 2: Run all 14 executables**

```powershell
.\build\test_date_time.exe
.\build\test_calendar_engine.exe
.\build\test_utf8.exe
.\build\test_event.exe
.\build\test_t9_engine.exe
.\build\test_agenda_manager.exe
.\build\test_agenda_state.exe
.\build\test_crc32.exe
.\build\test_event_codec.exe
.\build\test_desktop_file_storage.exe
.\build\test_agenda_repository.exe
.\build\test_reminder_engine.exe
.\build\test_agenda_controller.exe
.\build\test_agenda_persistence.exe
```

Expected: every executable prints its `All ... tests passed.` message and returns exit code `0`.

- [ ] **Step 3: Verify the final repository state**

```powershell
git diff --check
git status --short
git log --oneline -8
```

Expected:

- `git diff --check` prints nothing.
- `git status --short` prints nothing.
- The log contains separate commits for due calculation, manager acknowledgement, controller lookup, transactional acknowledgement, and persistence coverage.

## Execution Notes

- Execute tasks sequentially because later tasks consume interfaces created by earlier tasks.
- Stop immediately on a compiler warning, failed assertion, or unexpected Git change.
- Do not add polling, popup state, RTC access, or platform code while implementing this plan.
- When a task requires editing an existing test file, preserve every existing test and register each new test explicitly in `main()`.
- After all tasks pass, perform a code review against the approved spec before merging into `main`.
