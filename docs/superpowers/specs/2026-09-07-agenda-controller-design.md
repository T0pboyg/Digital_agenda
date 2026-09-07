# Agenda Controller Design

Date: 2026-09-07

Status: Approved

## Goal

Add a central application controller between the future user
interface, `AgendaManager`, and `AgendaRepository`.

The controller guarantees that every successful event mutation is
persisted. If persistence fails, the complete previous in-memory state
is restored.

The controller and the existing core remain fully offline. They do not
depend on Wi-Fi, internet access, or external services.

## Scope

This stage includes:

- loading the agenda during application startup,
- owning the in-memory `AgendaManager`,
- read-only access to the loaded agenda,
- creating, updating, and deleting events,
- automatic persistence after successful mutations,
- exact rollback after persistence failure,
- reporting startup, domain, and persistence errors,
- reporting successful recovery from backup.

This stage does not include:

- reminder calculation,
- display rendering,
- touch input,
- T9 screen integration,
- LittleFS implementation,
- ESP32 clock implementation,
- network functionality.

Those features will use the controller later without changing its
transactional persistence rules.

## Architecture

`AgendaController` owns one `AgendaManager` and receives an
`AgendaRepository` by reference.

The future UI communicates with `AgendaController`. It never modifies
`AgendaManager` directly and never reads or writes storage directly.

The dependency direction is:

1. UI calls `AgendaController`.
2. `AgendaController` coordinates `AgendaManager`.
3. `AgendaController` loads and saves through `AgendaRepository`.
4. `AgendaRepository` uses the selected `ByteStorage` implementation.
5. Desktop storage and ESP32 LittleFS remain replaceable platform
   adapters.

The controller exposes the manager only through a constant reference.
This allows the UI to read events while preventing it from bypassing
automatic persistence.

## Controller State

The controller starts with:

- an empty internal `AgendaManager`,
- `ready_ == false`.

Mutating operations are allowed only after a successful call to
`start()`.

A failed startup leaves `ready_ == false`. Creation, update, and
deletion then return `AgendaControllerError::NotReady` without
changing memory or calling `save()`.

## Public API

The intended public interface is:

```cpp
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

`AgendaControllerError` reports controller and storage failures.
`EventError` reports domain validation failures such as invalid input,
missing event, or full agenda.

A result is successful only when both errors are `None`.

For a failed creation, `eventId` is `0`.

## Startup Flow

`start()` performs these steps:

1. Call `repository_.load()`.
2. If loading fails, map the repository error to
   `AgendaControllerError`, keep `ready_ == false`, and return.
3. Pass the loaded state to `agenda_.restoreState()`.
4. If restoration fails, return
   `AgendaControllerError::InvalidData` and keep the controller not
   ready.
5. Set `ready_ = true`.
6. Return whether the repository recovered the data from backup.

Startup outcomes are:

- Missing primary and backup files are a valid first boot.
- First boot produces an empty agenda with `nextId == 1`.
- First boot does not create a storage file.
- Valid primary data loads normally.
- Valid backup data may recover a missing or corrupted primary.
- Backup recovery is successful startup with
  `recoveredFromBackup == true`.
- Read failure or invalid data without a valid recovery copy is a
  blocking startup error.

## Mutation Transaction

Creation, update, and deletion use the same transaction:

1. Reject the operation with `NotReady` if startup has not succeeded.
2. Capture the previous `AgendaState` using `snapshot()`.
3. Perform the requested operation through `AgendaManager`.
4. If the domain operation fails, return its `EventError` without
   calling `save()`.
5. If the domain operation succeeds, save the new snapshot through
   `AgendaRepository`.
6. If saving succeeds, keep the new in-memory state and return success.
7. If saving fails, restore the exact previous snapshot and return the
   mapped controller error.

For deletion, `AgendaManager::deleteEvent()` returning `false` maps to
`EventError::EventNotFound`. This follows the domain failure path and
does not call `save()`.

The previous snapshot includes:

- all events,
- event ordering,
- event IDs,
- event dates and times,
- event text,
- creation timestamps,
- reminder state,
- `nextId`.

Therefore, a persistence failure cannot leave an uncommitted event in
memory.

A failed creation returns `eventId == 0`. Because `nextId` is restored,
the failed creation does not consume an ID.

A snapshot produced by the same `AgendaManager` is always expected to
pass `restoreState()`. Failure during rollback represents an internal
programming invariant violation rather than a normal runtime outcome.

## Error Semantics

Repository errors map as follows:

- `AgendaRepositoryError::None` becomes
  `AgendaControllerError::None`.
- `AgendaRepositoryError::ReadFailed` becomes
  `AgendaControllerError::ReadFailed`.
- `AgendaRepositoryError::WriteFailed` becomes
  `AgendaControllerError::WriteFailed`.
- `AgendaRepositoryError::InvalidData` becomes
  `AgendaControllerError::InvalidData`.

If a domain operation fails:

- controller error is `None`,
- `eventError` contains the reason,
- storage is not written.

If persistence fails after a valid domain operation:

- controller error contains the persistence failure,
- `eventError` is `None`,
- the previous state is restored.

This distinction lets the future UI show an appropriate validation or
storage message.

## Offline Operation

The controller contains no networking code.

Agenda data is stored locally:

- in desktop files during development,
- in ESP32 LittleFS on the final device.

The current time is passed to `createEvent()` and `updateEvent()` as
`const DateTime& now`.

A replaceable offline clock implementation will be connected in a
later stage.

Internet access is not required for event creation, storage, calendar
display, T9 input, or reminders.

Maintaining the correct time after complete battery disconnection is a
separate hardware concern. It will be handled through an offline clock
strategy, such as persistent RTC hardware or manual time adjustment.

## Files

Implementation will add:

- `src/core/agenda_controller.h`
- `src/core/agenda_controller.cpp`
- `tests/test_agenda_controller.cpp`

Existing modules should require no behavioural changes unless a test
reveals an integration defect.

## Test Strategy

Controller tests will use the existing repository with controllable
test storage.

The suite will verify:

1. A new controller is not ready before startup.
2. First boot starts successfully with an empty agenda.
3. Existing persisted events load successfully.
4. Backup recovery is reported to the caller.
5. Read failure prevents startup.
6. Invalid stored data prevents startup.
7. Mutations are rejected while the controller is not ready.
8. Successful creation saves the new state.
9. Invalid creation does not call storage.
10. Creation save failure restores the exact previous state.
11. A failed creation does not consume an event ID.
12. Successful update saves the modified state.
13. Invalid update does not call storage.
14. Update save failure restores the previous event exactly.
15. Successful deletion saves the new state.
16. Invalid deletion does not call storage.
17. Delete save failure restores the deleted event exactly.
18. The agenda exposed by the controller is read-only.

After the controller suite passes, all existing test executables and
the persistence integration test must also pass.