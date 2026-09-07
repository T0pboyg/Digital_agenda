# Reminder Engine Design

Date: 2026-09-07

Status: Approved

## Goal

Add a fully offline reminder subsystem for agenda events.

Every event becomes eligible for a reminder at 17:00 on the previous
calendar day. The reminder remains available if the device was off at
that moment, provided that the event has not passed and the reminder
has not already been acknowledged.

The subsystem returns one reminder at a time. A reminder is marked as
seen only when the user explicitly dismisses it with the future UI.
Acknowledgement is persisted transactionally through
`AgendaController`.

## Approved Product Decisions

- Timed and all-day events use the same reminder time: 17:00 on the
  previous calendar day.
- A missed reminder is shown when the device next runs, as long as the
  event has not passed.
- Multiple due reminders are shown one at a time.
- Each reminder is acknowledged independently.
- `reminderSeen` changes to `true` only after the user dismisses the
  reminder.
- Editing an event resets `reminderSeen` to `false`, so its reminder
  may appear again.
- The subsystem remains independent of internet access, display code,
  touch input, and a specific hardware clock.

## Scope

This stage includes:

- calculating whether a reminder is due,
- selecting the next due reminder,
- rejecting an invalid current date or time,
- exposing reminder lookup through `AgendaController`,
- marking a reminder as seen through `AgendaManager`,
- persisting acknowledgement through `AgendaController`,
- exact in-memory rollback after a persistence failure,
- unit and desktop persistence integration tests.

This stage does not include:

- drawing a notification popup,
- handling touch input for an OK button,
- polling frequency or an application event loop,
- ESP32 timers, RTC drivers, or manual clock-setting UI,
- snooze or repeated reminders,
- configurable reminder times,
- sound, vibration, or network notifications.

## Architecture

Reminder calculation is stateless and belongs to the new
`reminder_engine` core module. It receives read-only agenda state and a
caller-supplied `DateTime`. It neither changes events nor accesses
storage.

`AgendaManager` remains the only owner of mutable agenda state. It
gains a small operation that marks one existing event as seen.

`AgendaController` remains the persistence boundary. It asks the
reminder engine for the next due event and returns a safe copy for the
future UI. When the UI acknowledges that reminder, the controller
updates the manager, saves the complete agenda state, and restores the
previous snapshot if saving fails.

The dependency flow is:

1. The future UI supplies the current local `DateTime` to
   `AgendaController`.
2. `AgendaController` asks the reminder engine for the next due event.
3. The reminder engine reads the ordered events from `AgendaManager`.
4. `AgendaController` returns one event copy to the UI.
5. The UI displays it and later acknowledges its ID.
6. `AgendaController` mutates `AgendaManager` and persists through
   `AgendaRepository`.

No component in this flow uses Wi-Fi, internet access, or an external
service.

## Reminder Engine API

The reminder engine is stateless, so its public API is a free function
in the same style as the existing calendar engine:

```cpp
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

The result meanings are:

- `error == None` and `eventId` has a value: one reminder is due.
- `error == None` and `eventId` is empty: no reminder is due.
- `error == InvalidDateTime` and `eventId` is empty: the supplied
  current date or time is invalid, so no reminder may be shown.

The engine returns an event ID rather than a pointer. This avoids
exposing a reference whose lifetime could end after a controller
mutation.

## Due-Time Calculation

For every event, the reminder threshold is calculated numerically as:

```text
(serialDay(event.eventDate) - 1) * 1440 + 17 * 60
```

Using a serial-minute threshold avoids constructing an out-of-range
date at month, year, and leap-year boundaries.

An event is due when all of the following are true:

1. `reminderSeen == false`.
2. The current `DateTime` is valid.
3. The current serial minute is greater than or equal to the reminder
   threshold.
4. `isPast(event, now) == false`.

The fourth rule uses the existing event semantics:

- A timed event becomes past at its exact start minute.
- An all-day event remains current throughout its event date and
  becomes past on the following day.

Consequently, a timed event may produce a late reminder until the
minute before it begins. An all-day event may produce a late reminder
throughout its event date.

For a valid event on 2000-01-01, the numerical threshold may be
negative. This is valid and allows the reminder to be due when the
device first has a valid time on the event date, without constructing
an unsupported 1999 `Date`.

## Reminder Ordering

`AgendaManager` maintains its events in deterministic agenda order:

1. earlier event date,
2. all-day events before timed events on the same date,
3. earlier event time,
4. lower event ID.

The reminder engine scans that ordered collection and returns the
first due, unseen event.

After that event is acknowledged, a new lookup skips it and returns
the next due event. This produces one-at-a-time display without storing
a separate queue that could become stale after an edit or deletion.

## AgendaManager Change

`AgendaManager` gains:

```cpp
bool markReminderSeen(std::uint32_t id);
```

Behavior:

- Return `false` without changing state if the ID is missing.
- Set `reminderSeen = true` and return `true` for an existing event.
- Preserve every other field, event ordering, and `nextId`.

The existing `updateEvent()` behavior remains unchanged: every
successful edit resets `reminderSeen` to `false`.

## AgendaController API

The controller gains a read result that keeps controller and reminder
errors separate:

```cpp
struct AgendaControllerReminderResult
{
    AgendaControllerError error;
    ReminderEngineError reminderError;
    std::optional<Event> event;
};
```

It also gains:

```cpp
AgendaControllerReminderResult nextReminder(
    const DateTime& now
) const;

AgendaControllerChangeResult acknowledgeReminder(
    std::uint32_t eventId
);
```

`nextReminder()` behavior:

- Before successful startup, return `NotReady`, no reminder error, and
  no event.
- For an invalid current date or time, return no controller error,
  `InvalidDateTime`, and no event.
- When nothing is due, return no errors and no event.
- When a reminder is due, return no errors and a copy of the complete
  event.
- Never mutate state and never call `save()`.

The event copy gives the UI stable data for a popup while preserving
the controller's read-only state boundary.

`acknowledgeReminder()` behavior:

- Before successful startup, return `NotReady` without mutation or
  persistence.
- For a missing ID, return `EventNotFound` without persistence.
- For an existing ID, snapshot the complete agenda, mark the reminder
  as seen, and save exactly once.
- On successful save, return no errors.
- On any repository failure, restore the exact previous state and map
  the repository error through the controller's existing mapping.

The method accepts an event ID rather than keeping an active-popup ID
inside the controller. The future single-threaded UI is responsible
for acknowledging the ID it is currently displaying.

## Persistence and Compatibility

`Event::reminderSeen` already exists in `AgendaState` and is already
encoded and decoded by the binary event codec.

Therefore this stage does not change:

- the binary file header,
- the codec version,
- CRC32 behavior,
- primary/backup rotation,
- existing `agenda.bin` compatibility.

After a successful acknowledgement, closing and reopening the
application restores `reminderSeen == true`, so the reminder is not
shown again.

If acknowledgement persistence fails, in-memory rollback restores
`reminderSeen == false`. The reminder remains eligible and can be
shown again instead of being silently lost.

## Invalid Clock Handling

The reminder engine validates both parts of `now` with
`isValidDate()` and `isValidTime()` before serial calculations.

An invalid time produces `ReminderEngineError::InvalidDateTime` and no
reminder. This is a fail-safe result: the application must not display
a reminder based on an uninitialized or corrupted clock.

The source of `now` is deliberately outside this subsystem. Desktop
tests supply it directly. A later ESP32 integration stage will provide
it from the selected offline clock strategy and may use the explicit
error to request manual time setup.

## Error Separation

The future UI distinguishes outcomes as follows:

| Controller error | Reminder error | Event | Meaning |
|---|---|---|---|
| `NotReady` | `None` | empty | Agenda startup has not succeeded. |
| `None` | `InvalidDateTime` | empty | Device time is invalid. |
| `None` | `None` | empty | No reminder is currently due. |
| `None` | `None` | present | Display this event. |

Acknowledgement continues using `AgendaControllerChangeResult`:

- `error` reports readiness or repository failure.
- `eventError` reports `EventNotFound`.
- Success requires both values to be `None`.

## Testing Strategy

### Reminder engine unit tests

Tests cover:

- no reminder before 17:00 on the previous day,
- reminder exactly at 17:00,
- reminder later that evening,
- missed reminder on the event day,
- timed event immediately before its start,
- no reminder at or after a timed event's start,
- all-day reminder throughout its event date,
- no all-day reminder on the following day,
- seen reminders being skipped,
- multiple due reminders using agenda order,
- month, year, and leap-year boundaries,
- an event on 2000-01-01,
- invalid current date,
- invalid current time,
- an empty agenda.

### AgendaManager tests

Tests cover:

- marking an existing reminder as seen,
- preserving every unrelated event field and `nextId`,
- returning `false` for a missing ID,
- successful event edits resetting the flag to `false`.

### AgendaController tests

Tests cover:

- lookup before startup,
- lookup with invalid current time,
- no due reminder,
- returning a safe event copy,
- lookup performing no writes,
- acknowledgement before startup,
- missing acknowledgement ID performing no write,
- successful acknowledgement saving exactly once,
- decoding saved bytes to verify `reminderSeen == true`,
- read and write failures during save,
- exact rollback of all agenda fields and `nextId`,
- sequential lookup and acknowledgement of multiple reminders.

### Desktop persistence integration

An integration test uses real temporary `agenda.bin` and `agenda.bak`
files to verify:

1. Create and save a future event.
2. Find and acknowledge its reminder.
3. Close and reconstruct storage, repository, and controller objects.
4. Load the agenda again.
5. Confirm that the acknowledged reminder is not returned.

All existing test suites must continue to compile warning-free and
pass after the change.

## Files Expected to Change

New files:

- `src/core/reminder_engine.h`
- `src/core/reminder_engine.cpp`
- `tests/test_reminder_engine.cpp`

Existing files:

- `src/core/agenda_manager.h`
- `src/core/agenda_manager.cpp`
- `src/core/agenda_controller.h`
- `src/core/agenda_controller.cpp`
- `tests/test_agenda_manager.cpp`
- `tests/test_agenda_controller.cpp`
- `tests/test_agenda_persistence.cpp`

No platform driver, UI, touch, T9, codec, repository, or network file
is changed in this stage.

## Completion Criteria

This stage is complete when:

- every approved reminder timing rule is implemented,
- reminders are returned one at a time in agenda order,
- acknowledgement occurs only through the controller,
- acknowledgement survives reopening the application,
- a failed acknowledgement save restores the exact previous state,
- invalid clock input cannot produce a reminder,
- all new and existing tests compile warning-free and pass,
- `git diff --check` reports no formatting errors,
- the working tree contains only the intended changes before commit.
