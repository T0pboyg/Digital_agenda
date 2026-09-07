# Agenda Controller Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an offline transactional `AgendaController` that owns `AgendaManager`, loads through `AgendaRepository`, automatically saves successful mutations, and rolls back the complete agenda state when persistence fails.

**Architecture:** `AgendaController` is the only mutation boundary used by the future UI. It owns the in-memory `AgendaManager`, receives `AgendaRepository` by reference, exposes the manager only as `const AgendaManager&`, and maps repository failures into controller-level errors. Each create, update, and delete operation follows snapshot, domain mutation, save, and exact rollback on save failure.

**Tech Stack:** C++17, standard library, existing assertion-based test executables, MinGW-w64 `g++`, Git.

**Spec:** `docs/superpowers/specs/2026-09-07-agenda-controller-design.md`

## Global Constraints

- The core must remain fully offline and contain no networking code.
- The future UI must not modify `AgendaManager` or access storage directly.
- Mutations are rejected until `start()` succeeds.
- Domain failures must not call `AgendaRepository::save()`.
- Persistence failures must restore events, ordering, IDs, dates, times, text, `reminderSeen`, `createdAt`, and `nextId` exactly.
- A failed creation must return ID `0` and must not consume an ID.
- First boot with no stored files must succeed with an empty agenda and `nextId == 1` without creating a file.
- Backup recovery must be reported through `recoveredFromBackup == true`.
- `createEvent()` and `updateEvent()` receive the current time as `const DateTime& now`.
- A `false` result from `AgendaManager::deleteEvent()` maps to `EventError::EventNotFound` and does not trigger persistence.
- No existing core module should change unless a failing integration test proves that a change is necessary.

## File Map

- Create `src/core/agenda_controller.h`: public controller errors, result structs, API, owned manager, repository reference, and readiness state.
- Create `src/core/agenda_controller.cpp`: startup, error mapping, transactional create, update, and delete behavior.
- Create `tests/test_agenda_controller.cpp`: controllable byte storage plus startup, persistence, rollback, ID, and read-only API coverage.
- Do not modify `AgendaManager`, `AgendaRepository`, codec, or storage production files during this implementation.

## Controller Test Build Command

Use this command after every test or implementation change:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
```

Run the suite with:

```powershell
.\build\test_agenda_controller.exe
```

---

### Task 1: Controller startup and read-only state

**Files:**
- Create: `src/core/agenda_controller.h`
- Create: `src/core/agenda_controller.cpp`
- Create: `tests/test_agenda_controller.cpp`

**Interfaces:**
- Consumes: `AgendaRepository::load() const`, `AgendaManager::restoreState(const AgendaState&)`, `AgendaManager::snapshot() const`.
- Produces: `AgendaControllerError`, `AgendaStartResult`, `AgendaControllerCreateResult`, `AgendaControllerChangeResult`, `AgendaController::start()`, `AgendaController::isReady() const`, and `AgendaController::agenda() const`.

- [ ] **Step 1: Write the startup tests**

Create `tests/test_agenda_controller.cpp` with:

```cpp
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/agenda_controller.h"
#include "core/event_codec.h"

namespace
{
class TestByteStorage : public ByteStorage
{
public:
    ByteReadResult readResult{
        ByteStorageError::NotFound,
        {}
    };

    ByteStorageError writeResult =
        ByteStorageError::None;

    mutable std::vector<std::uint8_t>
        writtenBytes;

    mutable std::size_t readCount = 0;
    mutable std::size_t writeCount = 0;

    ByteReadResult read() const override
    {
        ++readCount;
        return readResult;
    }

    ByteStorageError write(
        const std::vector<std::uint8_t>& bytes
    ) const override
    {
        ++writeCount;
        writtenBytes = bytes;
        return writeResult;
    }
};

AgendaState storedControllerState()
{
    return AgendaState{
        {
            Event{
                7,
                Date{2026, 9, 12},
                std::optional<TimeOfDay>{
                    TimeOfDay{12, 30}
                },
                "Dentist",
                true,
                DateTime{
                    Date{2026, 9, 7},
                    TimeOfDay{10, 0}
                }
            }
        },
        8
    };
}

std::vector<std::uint8_t> encodedBytes(
    const AgendaState& state
)
{
    const EventEncodeResult encoded =
        encodeAgendaState(state);

    assert(
        encoded.error ==
        EventCodecError::None
    );

    return encoded.bytes;
}

void test_new_controller_is_not_ready()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(!controller.isReady());
    assert(controller.agenda().empty());
}

void test_first_boot_starts_empty()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaStartResult result =
        controller.start();

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(!result.recoveredFromBackup);
    assert(controller.isReady());
    assert(controller.agenda().empty());
    assert(controller.agenda().snapshot().nextId == 1);
    assert(storage.readCount == 1);
    assert(storage.writeCount == 0);
}

void test_existing_state_loads_on_start()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(storedControllerState())
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaStartResult result =
        controller.start();

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(!result.recoveredFromBackup);
    assert(controller.isReady());
    assert(controller.agenda().size() == 1);

    const Event* event =
        controller.agenda().findEvent(7);

    assert(event != nullptr);
    assert(event->text == "Dentist");
    assert(controller.agenda().snapshot().nextId == 8);
}

void test_backup_recovery_is_reported()
{
    TestByteStorage primary;
    TestByteStorage backup;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        {0x01, 0x02}
    };

    backup.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(storedControllerState())
    };

    AgendaRepository repository(
        primary,
        backup
    );

    AgendaController controller(repository);

    const AgendaStartResult result =
        controller.start();

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(result.recoveredFromBackup);
    assert(controller.isReady());
    assert(controller.agenda().size() == 1);
    assert(primary.writeCount == 1);
}

void test_read_failure_prevents_startup()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaStartResult result =
        controller.start();

    assert(
        result.error ==
        AgendaControllerError::ReadFailed
    );

    assert(!result.recoveredFromBackup);
    assert(!controller.isReady());
}

void test_invalid_data_prevents_startup()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        {0x01, 0x02}
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaStartResult result =
        controller.start();

    assert(
        result.error ==
        AgendaControllerError::InvalidData
    );

    assert(!result.recoveredFromBackup);
    assert(!controller.isReady());
}

static_assert(
    std::is_same_v<
        decltype(
            std::declval<
                const AgendaController&
            >().agenda()
        ),
        const AgendaManager&
    >,
    "AgendaController must expose read-only agenda access"
);
}

int main()
{
    test_new_controller_is_not_ready();
    test_first_boot_starts_empty();
    test_existing_state_loads_on_start();
    test_backup_recovery_is_reported();
    test_read_failure_prevents_startup();
    test_invalid_data_prevents_startup();

    std::cout
        << "All agenda controller tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Compile to verify the test fails**

Run the controller test build command.

Expected: compilation fails because `core/agenda_controller.h` does not exist.

- [ ] **Step 3: Add the public controller interface**

Create `src/core/agenda_controller.h` with:

```cpp
#pragma once

#include <cstdint>

#include "agenda_manager.h"
#include "agenda_repository.h"

enum class AgendaControllerError
{
    None,
    NotReady,
    ReadFailed,
    WriteFailed,
    InvalidData
};

struct AgendaStartResult
{
    AgendaControllerError error;
    bool recoveredFromBackup;
};

struct AgendaControllerCreateResult
{
    AgendaControllerError error;
    EventError eventError;
    std::uint32_t eventId;
};

struct AgendaControllerChangeResult
{
    AgendaControllerError error;
    EventError eventError;
};

class AgendaController
{
public:
    explicit AgendaController(
        AgendaRepository& repository
    );

    AgendaStartResult start();

    bool isReady() const;

    const AgendaManager& agenda() const;

    AgendaControllerCreateResult createEvent(
        const EventDraft& draft,
        const DateTime& now
    );

    AgendaControllerChangeResult updateEvent(
        std::uint32_t eventId,
        const EventDraft& draft,
        const DateTime& now
    );

    AgendaControllerChangeResult deleteEvent(
        std::uint32_t eventId
    );

private:
    AgendaManager agenda_;
    AgendaRepository& repository_;
    bool ready_;
};
```

- [ ] **Step 4: Implement startup and read-only access**

Create `src/core/agenda_controller.cpp` with:

```cpp
#include "agenda_controller.h"

namespace
{
AgendaControllerError mapRepositoryError(
    AgendaRepositoryError error
)
{
    switch (error)
    {
        case AgendaRepositoryError::None:
            return AgendaControllerError::None;

        case AgendaRepositoryError::ReadFailed:
            return AgendaControllerError::ReadFailed;

        case AgendaRepositoryError::WriteFailed:
            return AgendaControllerError::WriteFailed;

        case AgendaRepositoryError::InvalidData:
            return AgendaControllerError::InvalidData;
    }

    return AgendaControllerError::InvalidData;
}
}

AgendaController::AgendaController(
    AgendaRepository& repository
)
    : repository_(repository),
      ready_(false)
{
}

AgendaStartResult AgendaController::start()
{
    ready_ = false;

    const AgendaLoadResult loaded =
        repository_.load();

    if (
        loaded.error !=
        AgendaRepositoryError::None
    )
    {
        return AgendaStartResult{
            mapRepositoryError(loaded.error),
            false
        };
    }

    if (!agenda_.restoreState(loaded.state))
    {
        return AgendaStartResult{
            AgendaControllerError::InvalidData,
            false
        };
    }

    ready_ = true;

    return AgendaStartResult{
        AgendaControllerError::None,
        loaded.recoveredFromBackup
    };
}

bool AgendaController::isReady() const
{
    return ready_;
}

const AgendaManager& AgendaController::agenda() const
{
    return agenda_;
}
```

- [ ] **Step 5: Compile and run the startup suite**

Run the controller test build command and then run `build\test_agenda_controller.exe`.

Expected output:

```text
All agenda controller tests passed.
```

- [ ] **Step 6: Check and commit the startup slice**

```powershell
git diff --check
git add src/core/agenda_controller.h src/core/agenda_controller.cpp tests/test_agenda_controller.cpp
git diff --cached --check
git commit -m "feat: add agenda controller startup"
```

---

### Task 2: Transactional event creation

**Files:**
- Modify: `src/core/agenda_controller.cpp`
- Modify: `tests/test_agenda_controller.cpp`

**Interfaces:**
- Consumes: `AgendaManager::createEvent(const EventDraft&, const DateTime&)`, `AgendaManager::snapshot() const`, `AgendaManager::restoreState(const AgendaState&)`, and `AgendaRepository::save(const AgendaState&) const`.
- Produces: `AgendaController::createEvent(const EventDraft&, const DateTime&)` with automatic save, error separation, exact rollback, and ID preservation.

- [ ] **Step 1: Add creation helpers and failing tests**

Inside the anonymous namespace in `tests/test_agenda_controller.cpp`, immediately before its closing brace, add:

```cpp
DateTime controllerNow()
{
    return DateTime{
        Date{2026, 9, 7},
        TimeOfDay{10, 0}
    };
}

EventDraft futureDraft(
    const std::string& text
)
{
    return EventDraft{
        Date{2026, 9, 8},
        std::optional<TimeOfDay>{
            TimeOfDay{12, 30}
        },
        text
    };
}

void test_create_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(result.eventError == EventError::None);
    assert(result.eventId == 0);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 0);
}

void test_successful_creation_is_saved()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::None);
    assert(result.eventId == 1);
    assert(controller.agenda().size() == 1);
    assert(storage.writeCount == 1);

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(decoded.error == EventCodecError::None);
    assert(decoded.state.events.size() == 1);
    assert(decoded.state.events[0].id == 1);
    assert(decoded.state.events[0].text == "Dentist");
    assert(decoded.state.nextId == 2);
}

void test_invalid_creation_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft(""),
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::EmptyText);
    assert(result.eventId == 0);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 0);
}

void test_create_write_failure_rolls_back_and_preserves_id()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const AgendaControllerCreateResult failed =
        controller.createEvent(
            futureDraft("First attempt"),
            controllerNow()
        );

    assert(
        failed.error ==
        AgendaControllerError::WriteFailed
    );

    assert(failed.eventError == EventError::None);
    assert(failed.eventId == 0);
    assert(controller.agenda().empty());
    assert(controller.agenda().snapshot().nextId == 1);

    storage.writeResult =
        ByteStorageError::None;

    const AgendaControllerCreateResult saved =
        controller.createEvent(
            futureDraft("Second attempt"),
            controllerNow()
        );

    assert(saved.error == AgendaControllerError::None);
    assert(saved.eventError == EventError::None);
    assert(saved.eventId == 1);
    assert(controller.agenda().size() == 1);
    assert(controller.agenda().snapshot().nextId == 2);
}

void test_create_read_failure_rolls_back()
{
    TestByteStorage primary;
    TestByteStorage backup;

    AgendaRepository repository(
        primary,
        backup
    );

    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    primary.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(
        result.error ==
        AgendaControllerError::ReadFailed
    );

    assert(result.eventError == EventError::None);
    assert(result.eventId == 0);
    assert(controller.agenda().empty());
    assert(controller.agenda().snapshot().nextId == 1);
    assert(primary.writeCount == 0);
    assert(backup.writeCount == 0);
}
```

Add these calls to `main()` before the success message:

```cpp
test_create_requires_ready_controller();
test_successful_creation_is_saved();
test_invalid_creation_does_not_save();
test_create_write_failure_rolls_back_and_preserves_id();
test_create_read_failure_rolls_back();
```

- [ ] **Step 2: Build to verify the creation tests fail**

Run the controller test build command.

Expected: linking fails with an undefined reference to `AgendaController::createEvent(...)`.

- [ ] **Step 3: Implement transactional creation**

Add `#include <cassert>` after the include in `src/core/agenda_controller.cpp`:

```cpp
#include "agenda_controller.h"

#include <cassert>
```

Append this method to `src/core/agenda_controller.cpp`:

```cpp
AgendaControllerCreateResult
AgendaController::createEvent(
    const EventDraft& draft,
    const DateTime& now
)
{
    if (!ready_)
    {
        return AgendaControllerCreateResult{
            AgendaControllerError::NotReady,
            EventError::None,
            0
        };
    }

    const AgendaState previousState =
        agenda_.snapshot();

    const CreateEventResult created =
        agenda_.createEvent(draft, now);

    if (created.error != EventError::None)
    {
        return AgendaControllerCreateResult{
            AgendaControllerError::None,
            created.error,
            0
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

        return AgendaControllerCreateResult{
            mapRepositoryError(saveError),
            EventError::None,
            0
        };
    }

    return AgendaControllerCreateResult{
        AgendaControllerError::None,
        EventError::None,
        created.eventId
    };
}
```

- [ ] **Step 4: Build and run the creation suite**

Run the controller test build command and then run `build\test_agenda_controller.exe`.

Expected output:

```text
All agenda controller tests passed.
```

- [ ] **Step 5: Check and commit transactional creation**

```powershell
git diff --check
git add src/core/agenda_controller.cpp tests/test_agenda_controller.cpp
git diff --cached --check
git commit -m "feat: add transactional agenda creation"
```

---

### Task 3: Transactional event updates

**Files:**
- Modify: `src/core/agenda_controller.cpp`
- Modify: `tests/test_agenda_controller.cpp`

**Interfaces:**
- Consumes: `AgendaManager::updateEvent(std::uint32_t, const EventDraft&, const DateTime&)`, `AgendaState`, and the controller transaction established in Task 2.
- Produces: `AgendaController::updateEvent(std::uint32_t, const EventDraft&, const DateTime&)` with domain-error separation and exact rollback.

- [ ] **Step 1: Add exact state-comparison helpers**

Inside the anonymous namespace in `tests/test_agenda_controller.cpp`, before the test functions added in Task 2, add:

```cpp
void assertDateEqual(
    const Date& actual,
    const Date& expected
)
{
    assert(actual.year == expected.year);
    assert(actual.month == expected.month);
    assert(actual.day == expected.day);
}

void assertTimeEqual(
    const TimeOfDay& actual,
    const TimeOfDay& expected
)
{
    assert(actual.hour == expected.hour);
    assert(actual.minute == expected.minute);
}

void assertDateTimeEqual(
    const DateTime& actual,
    const DateTime& expected
)
{
    assertDateEqual(actual.date, expected.date);
    assertTimeEqual(actual.time, expected.time);
}

void assertEventEqual(
    const Event& actual,
    const Event& expected
)
{
    assert(actual.id == expected.id);
    assertDateEqual(actual.eventDate, expected.eventDate);

    assert(
        actual.eventTime.has_value() ==
        expected.eventTime.has_value()
    );

    if (actual.eventTime.has_value())
    {
        assertTimeEqual(
            *actual.eventTime,
            *expected.eventTime
        );
    }

    assert(actual.text == expected.text);
    assert(actual.reminderSeen == expected.reminderSeen);
    assertDateTimeEqual(actual.createdAt, expected.createdAt);
}

void assertAgendaStateEqual(
    const AgendaState& actual,
    const AgendaState& expected
)
{
    assert(actual.nextId == expected.nextId);
    assert(actual.events.size() == expected.events.size());

    for (std::size_t index = 0;
         index < actual.events.size();
         ++index)
    {
        assertEventEqual(
            actual.events[index],
            expected.events[index]
        );
    }
}
```

- [ ] **Step 2: Add the failing update tests**

Inside the anonymous namespace, before its closing brace, add:

```cpp
void test_update_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            1,
            futureDraft("Updated"),
            controllerNow()
        );

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 0);
}

void test_successful_update_is_saved()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Original"),
            controllerNow()
        );

    assert(created.eventId == 1);
    storage.writeCount = 0;

    const EventDraft updatedDraft{
        Date{2026, 9, 9},
        std::optional<TimeOfDay>{
            TimeOfDay{14, 45}
        },
        "Updated"
    };

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            created.eventId,
            updatedDraft,
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 1);

    const Event* updated =
        controller.agenda().findEvent(created.eventId);

    assert(updated != nullptr);
    assert(updated->text == "Updated");
    assert(updated->eventDate.day == 9);
    assert(updated->eventTime.has_value());
    assert(updated->eventTime->hour == 14);
    assert(updated->eventTime->minute == 45);

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(decoded.error == EventCodecError::None);
    assert(decoded.state.events.size() == 1);
    assert(decoded.state.events[0].text == "Updated");
}

void test_missing_update_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            77,
            futureDraft("Updated"),
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::EventNotFound);
    assert(storage.writeCount == 0);
}

void test_past_update_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Original"),
            controllerNow()
        );

    assert(created.eventId == 1);
    storage.writeCount = 0;

    const EventDraft pastDraft{
        Date{2026, 9, 6},
        std::optional<TimeOfDay>{},
        "Past"
    };

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            created.eventId,
            pastDraft,
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::PastEvent);
    assert(storage.writeCount == 0);
    assert(
        controller.agenda()
            .findEvent(created.eventId)
            ->text == "Original"
    );
}

void test_update_write_failure_restores_exact_state()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Original"),
            controllerNow()
        );

    assert(created.eventId == 1);

    const AgendaState before =
        controller.agenda().snapshot();

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const EventDraft changedDraft{
        Date{2026, 9, 10},
        std::optional<TimeOfDay>{},
        "Changed"
    };

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            created.eventId,
            changedDraft,
            controllerNow()
        );

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
```

Add these calls to `main()` before the success message:

```cpp
test_update_requires_ready_controller();
test_successful_update_is_saved();
test_missing_update_does_not_save();
test_past_update_does_not_save();
test_update_write_failure_restores_exact_state();
```

- [ ] **Step 3: Build to verify the update tests fail**

Run the controller test build command.

Expected: linking fails with an undefined reference to `AgendaController::updateEvent(...)`.

- [ ] **Step 4: Implement transactional updates**

Append to `src/core/agenda_controller.cpp`:

```cpp
AgendaControllerChangeResult
AgendaController::updateEvent(
    std::uint32_t eventId,
    const EventDraft& draft,
    const DateTime& now
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

    const EventError eventError =
        agenda_.updateEvent(
            eventId,
            draft,
            now
        );

    if (eventError != EventError::None)
    {
        return AgendaControllerChangeResult{
            AgendaControllerError::None,
            eventError
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

- [ ] **Step 5: Build and run the update suite**

Run the controller test build command and then run `build\test_agenda_controller.exe`.

Expected output:

```text
All agenda controller tests passed.
```

- [ ] **Step 6: Check and commit transactional updates**

```powershell
git diff --check
git add src/core/agenda_controller.cpp tests/test_agenda_controller.cpp
git diff --cached --check
git commit -m "feat: add transactional agenda updates"
```

---

### Task 4: Transactional event deletion

**Files:**
- Modify: `src/core/agenda_controller.cpp`
- Modify: `tests/test_agenda_controller.cpp`

**Interfaces:**
- Consumes: `AgendaManager::deleteEvent(std::uint32_t)`, `AgendaManager::snapshot() const`, `AgendaManager::restoreState(const AgendaState&)`, and `AgendaRepository::save(const AgendaState&) const`.
- Produces: `AgendaController::deleteEvent(std::uint32_t)` with not-ready protection, `EventNotFound` mapping, automatic save, and exact rollback.

- [ ] **Step 1: Add the failing delete tests**

Inside the anonymous namespace in `tests/test_agenda_controller.cpp`, before its closing brace, add:

```cpp
void test_delete_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerChangeResult result =
        controller.deleteEvent(1);

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 0);
}

void test_successful_delete_is_saved()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(created.eventId == 1);
    storage.writeCount = 0;

    const AgendaControllerChangeResult result =
        controller.deleteEvent(created.eventId);

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::None);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 1);

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(decoded.error == EventCodecError::None);
    assert(decoded.state.events.empty());
    assert(decoded.state.nextId == 2);
}

void test_missing_delete_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.deleteEvent(77);

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::EventNotFound);
    assert(storage.writeCount == 0);
}

void test_delete_write_failure_restores_exact_state()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(created.eventId == 1);

    const AgendaState before =
        controller.agenda().snapshot();

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const AgendaControllerChangeResult result =
        controller.deleteEvent(created.eventId);

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
```

Add these calls to `main()` before the success message:

```cpp
test_delete_requires_ready_controller();
test_successful_delete_is_saved();
test_missing_delete_does_not_save();
test_delete_write_failure_restores_exact_state();
```

- [ ] **Step 2: Build to verify the deletion tests fail**

Run the controller test build command.

Expected: linking fails with an undefined reference to `AgendaController::deleteEvent(std::uint32_t)`.

- [ ] **Step 3: Implement transactional deletion**

Append to `src/core/agenda_controller.cpp`:

```cpp
AgendaControllerChangeResult
AgendaController::deleteEvent(
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

    if (!agenda_.deleteEvent(eventId))
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

- [ ] **Step 4: Build and run the complete controller suite**

Run the controller test build command and then run `build\test_agenda_controller.exe`.

Expected output:

```text
All agenda controller tests passed.
```

- [ ] **Step 5: Check and commit transactional deletion**

```powershell
git diff --check
git add src/core/agenda_controller.cpp tests/test_agenda_controller.cpp
git diff --cached --check
git commit -m "feat: add transactional agenda deletion"
```

---

### Task 5: Full regression and repository verification

**Files:**
- Verify: `src/core/agenda_controller.h`
- Verify: `src/core/agenda_controller.cpp`
- Verify: `tests/test_agenda_controller.cpp`
- Verify: all existing core and persistence test executables.

**Interfaces:**
- Consumes: the completed controller and all existing project modules.
- Produces: evidence that the controller feature and all previous modules pass together with a clean Git worktree.

- [ ] **Step 1: Rebuild the final controller suite**

Run the controller test build command.

Expected: compilation succeeds with no warnings.

- [ ] **Step 2: Run every test executable**

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
.\build\test_agenda_persistence.exe
.\build\test_agenda_controller.exe
```

Expected: all thirteen executables report their success message and exit successfully.

- [ ] **Step 3: Verify formatting and repository state**

```powershell
git diff --check
git status --short
git log --oneline -8
```

Expected:

- `git diff --check` prints nothing.
- `git status --short` prints nothing.
- The log contains the separate startup, creation, update, and deletion commits.

## Completion Criteria

The feature is complete only when:

- startup behavior passes for first boot, valid storage, backup recovery, read failure, and invalid data;
- the public agenda accessor is compile-time verified as read-only;
- create, update, and delete are rejected before successful startup;
- every successful mutation writes the encoded new state;
- every domain failure avoids storage writes;
- every tested persistence failure restores the previous state;
- failed creation preserves the next available ID;
- the controller and all existing test suites pass;
- `git diff --check` and `git status --short` are clean.
