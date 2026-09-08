# Agenda UI State Machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify a hardware-independent UI state machine for offline clock setup, calendar browsing, event editing, persistence, deletion, and reminder presentation.

**Architecture:** `AgendaUiStateMachine` receives semantic actions and owns only transient navigation/editor state. It reads and mutates agenda data through `AgendaController`, obtains local time through a new `Clock` port, reuses `CalendarEngine` and `T9Engine`, and exposes owned read-only view models for a future LCD renderer.

**Tech Stack:** C++17, standard library only, existing assert-based test executables, MSYS2 MinGW64 `g++`, Git, PowerShell.

**Spec:** `docs/superpowers/specs/2026-09-08-agenda-ui-state-machine-design.md`

## Global Constraints

- Work directly on `main`; this execution mode was explicitly selected for this project stage.
- Execute tasks inline; do not dispatch subagents or create a worktree.
- Keep all UI-core logic independent of GC9A01A, CST816S, LVGL, Wi-Fi, NTP, and ESP32 headers.
- Use Greek semantic UI labels, 24-hour time, and `DD/MM/YYYY` formatting in the future renderer; core logic stores enums and values rather than display strings.
- Supported dates remain 2000-01-01 through 2099-12-31.
- New events start all-day; enabling time initializes 12:00; time changes in five-minute increments.
- Each editor session starts in Greek lowercase letter mode and reuses `T9Engine`.
- Maximum event text remains 500 Unicode code points.
- Day pages expose three event rows initially and use explicit previous/next page actions.
- All agenda mutations use `AgendaController` so persistence and rollback remain transactional.
- No production change is written before its failing test has been run and verified for the expected reason.
- Compile every touched test with `-std=c++17 -Wall -Wextra -pedantic -Isrc` and require warning-free output.
- Stage only the files listed by each task and run `git diff --cached --check` before every commit.

## File Structure

### New files

- `src/core/clock.h` — platform-neutral current-time read/set interface.
- `src/core/agenda_ui_types.h` — UI screens, overlays, actions, editor draft, and owned view-model structures.
- `src/core/agenda_ui_state_machine.h` — public UI state-machine API and private state declarations.
- `src/core/agenda_ui_state_machine.cpp` — deterministic screen transitions, controller calls, clock handling, and view construction.
- `tests/test_agenda_ui_state_machine.cpp` — fake-clock and in-memory-storage unit coverage for all UI transitions.
- `tests/test_agenda_ui_persistence.cpp` — real desktop-file persistence coverage driven through the UI state machine.

### Modified files

- `src/core/date_time.h` — declare bounded previous/next-date helpers.
- `src/core/date_time.cpp` — implement previous/next-date helpers.
- `src/core/t9_engine.h` — expose the current uppercase/lowercase state read-only.
- `src/core/t9_engine.cpp` — implement the read-only case accessor.
- `tests/test_date_time.cpp` — verify date stepping at normal, leap, year, and supported-range boundaries.
- `tests/test_t9_engine.cpp` — verify the case accessor follows case toggles.

---

### Task 1: Clock Port and UI Startup States

**Files:**
- Create: `src/core/clock.h`
- Create: `src/core/agenda_ui_types.h`
- Create: `src/core/agenda_ui_state_machine.h`
- Create: `src/core/agenda_ui_state_machine.cpp`
- Create: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: `AgendaController::start()`, `AgendaController::isReady()`, `DateTime`, `YearMonth`, `MonthGrid`, `T9Engine`.
- Produces: `Clock`, `ClockReadResult`, `AgendaUiStateMachine::start()`, `AgendaUiStateMachine::handle()`, `AgendaUiStateMachine::tick()`, and `AgendaUiStateMachine::view()`.

- [ ] **Step 1: Create the failing startup tests**

Create `tests/test_agenda_ui_state_machine.cpp` with in-memory storage and clock doubles. The tests must exercise real `AgendaRepository` and `AgendaController` instances.

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/agenda_controller.h"
#include "core/agenda_ui_state_machine.h"
#include "core/clock.h"

class UiTestStorage : public ByteStorage
{
public:
    mutable ByteStorageError readError =
        ByteStorageError::NotFound;

    mutable ByteStorageError writeError =
        ByteStorageError::None;

    mutable std::vector<std::uint8_t> bytes;
    mutable std::size_t writeCount = 0;

    ByteReadResult read() const override
    {
        if (readError != ByteStorageError::None)
        {
            return ByteReadResult{readError, {}};
        }

        return ByteReadResult{
            ByteStorageError::None,
            bytes
        };
    }

    ByteStorageError write(
        const std::vector<std::uint8_t>& newBytes
    ) const override
    {
        ++writeCount;

        if (writeError == ByteStorageError::None)
        {
            bytes = newBytes;
            readError = ByteStorageError::None;
        }

        return writeError;
    }
};

class UiTestClock : public Clock
{
public:
    ClockReadResult readResult{
        ClockError::None,
        DateTime{
            Date{2026, 9, 8},
            TimeOfDay{10, 0}
        }
    };

    ClockError setResult = ClockError::None;
    std::size_t setCount = 0;
    DateTime lastSet{
        Date{2000, 1, 1},
        TimeOfDay{0, 0}
    };

    ClockReadResult read() const override
    {
        return readResult;
    }

    ClockError set(
        const DateTime& value
    ) override
    {
        ++setCount;
        lastSet = value;

        if (setResult == ClockError::None)
        {
            readResult = ClockReadResult{
                ClockError::None,
                value
            };
        }

        return setResult;
    }
};

DateTime uiTestNow()
{
    return DateTime{
        Date{2026, 9, 8},
        TimeOfDay{10, 0}
    };
}

void test_valid_startup_opens_current_month()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        uiTestNow()
    );

    ui.start();

    const AgendaUiView view = ui.view();

    assert(controller.isReady());
    assert(view.screen == AgendaUiScreen::Month);
    assert(view.displayedMonth.year == 2026);
    assert(view.displayedMonth.month == 9);
}

void test_unavailable_clock_opens_manual_setup()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error = ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        DateTime{
            Date{2026, 9, 1},
            TimeOfDay{12, 0}
        }
    );

    ui.start();

    const AgendaUiView view = ui.view();

    assert(controller.isReady());
    assert(view.screen == AgendaUiScreen::ClockSetup);
    assert(view.clockError == ClockError::Unavailable);
    assert(view.clockDraft.date.year == 2026);
}

void test_invalid_or_failed_clock_read_opens_manual_setup()
{
    UiTestStorage invalidStorage;
    UiTestClock invalidClock;
    invalidClock.readResult.value = DateTime{
        Date{2026, 2, 30},
        TimeOfDay{10, 0}
    };

    AgendaRepository invalidRepository(invalidStorage);
    AgendaController invalidController(invalidRepository);
    AgendaUiStateMachine invalidUi(
        invalidController,
        invalidClock,
        uiTestNow()
    );

    invalidUi.start();
    assert(
        invalidUi.view().screen ==
        AgendaUiScreen::ClockSetup
    );
    assert(
        invalidUi.view().clockError ==
        ClockError::InvalidDateTime
    );

    UiTestStorage failedStorage;
    UiTestClock failedClock;
    failedClock.readResult.error = ClockError::ReadFailed;
    AgendaRepository failedRepository(failedStorage);
    AgendaController failedController(failedRepository);
    AgendaUiStateMachine failedUi(
        failedController,
        failedClock,
        uiTestNow()
    );

    failedUi.start();
    assert(
        failedUi.view().screen ==
        AgendaUiScreen::ClockSetup
    );
    assert(
        failedUi.view().clockError ==
        ClockError::ReadFailed
    );
}

void test_agenda_load_failure_opens_startup_error()
{
    UiTestStorage storage;
    UiTestClock clock;

    storage.readError = ByteStorageError::ReadFailed;

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        uiTestNow()
    );

    ui.start();

    const AgendaUiView view = ui.view();

    assert(!controller.isReady());
    assert(view.screen == AgendaUiScreen::StartupError);
    assert(
        view.controllerError ==
        AgendaControllerError::ReadFailed
    );
}

void test_startup_retry_loads_after_storage_recovers()
{
    UiTestStorage storage;
    UiTestClock clock;
    storage.readError = ByteStorageError::ReadFailed;

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    assert(ui.view().screen == AgendaUiScreen::StartupError);

    storage.readError = ByteStorageError::NotFound;
    ui.handle({AgendaUiActionType::RetryStartup});

    assert(controller.isReady());
    assert(ui.view().screen == AgendaUiScreen::Month);
    assert(
        ui.view().controllerError ==
        AgendaControllerError::None
    );
}

int main()
{
    test_valid_startup_opens_current_month();
    test_unavailable_clock_opens_manual_setup();
    test_invalid_or_failed_clock_read_opens_manual_setup();
    test_agenda_load_failure_opens_startup_error();
    test_startup_retry_loads_after_storage_recovers();

    std::cout
        << "All agenda UI state machine tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Run the tests and verify the missing-interface failure**

Run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
```

Expected: compilation fails because `clock.h`, `agenda_ui_types.h`, and `agenda_ui_state_machine.h/.cpp` do not exist.

- [ ] **Step 3: Add the clock and UI type interfaces**

Create `src/core/clock.h`:

```cpp
#pragma once

#include "date_time.h"

enum class ClockError
{
    None,
    Unavailable,
    ReadFailed,
    WriteFailed,
    InvalidDateTime
};

struct ClockReadResult
{
    ClockError error;
    DateTime value;
};

class Clock
{
public:
    virtual ~Clock() = default;

    virtual ClockReadResult read() const = 0;

    virtual ClockError set(
        const DateTime& value
    ) = 0;
};
```

Create `src/core/agenda_ui_types.h` with these final public data shapes:

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "agenda_controller.h"
#include "calendar_engine.h"
#include "clock.h"
#include "t9_engine.h"

constexpr std::size_t AGENDA_UI_EVENTS_PER_PAGE = 3;

enum class AgendaUiScreen
{
    ClockSetup,
    Month,
    Day,
    EventDetails,
    EditorDate,
    EditorTime,
    EditorText,
    EditorReview,
    StartupError
};

enum class AgendaUiOverlay
{
    None,
    Reminder,
    ConfirmDelete,
    ConfirmDiscard,
    OperationError,
    ClockError
};

enum class AgendaUiEditorMode
{
    Create,
    Update
};

enum class AgendaUiPendingOperation
{
    None,
    CreateEvent,
    UpdateEvent,
    DeleteEvent
};

enum class AgendaUiActionType
{
    RetryStartup,
    ClockPreviousDay,
    ClockNextDay,
    ClockPreviousHour,
    ClockNextHour,
    ClockPreviousMinute,
    ClockNextMinute,
    ConfirmClock,
    PreviousMonth,
    NextMonth,
    SelectDate,
    PreviousDayPage,
    NextDayPage,
    SelectEvent,
    Back,
    StartCreate,
    StartEdit,
    EditorPreviousDay,
    EditorNextDay,
    ToggleEventTime,
    EditorPreviousHour,
    EditorNextHour,
    EditorPreviousMinute,
    EditorNextMinute,
    EditorNext,
    EditorBack,
    EditorCancel,
    ConfirmDiscard,
    CancelDiscard,
    T9Digit,
    ToggleT9Language,
    ToggleT9Mode,
    ToggleT9Case,
    T9Backspace,
    SaveEditor,
    RetryOperation,
    CancelOperation,
    RequestDelete,
    ConfirmDelete,
    CancelDelete,
    AcknowledgeReminder
};

struct AgendaUiAction
{
    AgendaUiActionType type;
    std::optional<Date> date;
    std::optional<std::uint32_t> eventId;
    char digit = '\0';
    std::uint32_t timestampMilliseconds = 0;
};

struct AgendaUiMonthCell
{
    std::optional<Date> date;
    bool inDisplayedMonth = false;
    bool isToday = false;
    bool isSelected = false;
    bool hasEvents = false;
};

struct AgendaUiEventRow
{
    std::uint32_t eventId = 0;
    std::optional<TimeOfDay> time;
    std::string text;
};

struct AgendaUiEditorDraft
{
    AgendaUiEditorMode mode =
        AgendaUiEditorMode::Create;

    std::optional<std::uint32_t> eventId;
    Date date{2000, 1, 1};
    std::optional<TimeOfDay> time;
    std::string text;
    bool dirty = false;
};

struct AgendaUiView
{
    AgendaUiScreen screen = AgendaUiScreen::StartupError;
    AgendaUiOverlay overlay = AgendaUiOverlay::None;

    AgendaControllerError controllerError =
        AgendaControllerError::None;

    EventError eventError = EventError::None;
    ClockError clockError = ClockError::None;

    YearMonth displayedMonth{2000, 1};
    bool monthPreviousEnabled = false;
    bool monthNextEnabled = false;

    DateTime clockDraft{
        Date{2000, 1, 1},
        TimeOfDay{0, 0}
    };

    std::array<AgendaUiMonthCell, 42> monthCells{};
    std::optional<Date> selectedDate;
    std::vector<AgendaUiEventRow> dayEvents;
    std::size_t dayPageIndex = 0;
    std::size_t dayPageCount = 0;
    bool dayPreviousEnabled = false;
    bool dayNextEnabled = false;

    std::optional<Event> selectedEvent;
    AgendaUiEditorDraft editor;

    T9Language t9Language = T9Language::Greek;
    T9Mode t9Mode = T9Mode::Letters;
    bool t9Uppercase = false;

    std::optional<Event> reminder;
};
```

- [ ] **Step 4: Add the minimal startup state machine**

Create `src/core/agenda_ui_state_machine.h`. Declare the final public API now and keep behavior-specific helpers private:

```cpp
#pragma once

#include "agenda_ui_types.h"

class AgendaUiStateMachine
{
public:
    AgendaUiStateMachine(
        AgendaController& controller,
        Clock& clock,
        const DateTime& clockSetupDefault
    );

    void start();
    void tick();

    void handle(
        const AgendaUiAction& action
    );

    AgendaUiView view() const;

private:
    void startOrRetry();
    void resetErrors();
    void rebuildMonthCells(AgendaUiView& result) const;
    void rebuildDayRows(AgendaUiView& result) const;
    void clampDayPage();
    void beginCreate();
    void beginEdit();
    void leaveEditorAfterDiscard();
    void saveEditor();
    void retryPendingOperation();
    void confirmDelete();
    void checkReminder();

    AgendaController& controller_;
    Clock& clock_;

    AgendaUiScreen screen_;
    AgendaUiOverlay overlay_;
    AgendaControllerError controllerError_;
    EventError eventError_;
    ClockError clockError_;

    DateTime clockSetupDefault_;
    DateTime clockDraft_;
    DateTime lastNow_;

    YearMonth displayedMonth_;
    std::optional<Date> selectedDate_;
    std::optional<std::uint32_t> selectedEventId_;
    std::size_t dayPageIndex_;

    AgendaUiEditorDraft editor_;
    AgendaUiScreen editorReturnScreen_;
    T9Engine t9_;

    AgendaUiPendingOperation pendingOperation_;
    std::optional<Event> reminder_;
};
```

Create `src/core/agenda_ui_state_machine.cpp`. Implement the constructor, `start()`, startup retry, empty `tick()`, startup-only `handle()`, and a basic `view()`.

```cpp
#include "agenda_ui_state_machine.h"

namespace
{
bool validDateTime(const DateTime& value)
{
    return
        isValidDate(value.date) &&
        isValidTime(value.time);
}
}

AgendaUiStateMachine::AgendaUiStateMachine(
    AgendaController& controller,
    Clock& clock,
    const DateTime& clockSetupDefault
)
    : controller_(controller),
      clock_(clock),
      screen_(AgendaUiScreen::StartupError),
      overlay_(AgendaUiOverlay::None),
      controllerError_(AgendaControllerError::None),
      eventError_(EventError::None),
      clockError_(ClockError::None),
      clockSetupDefault_(clockSetupDefault),
      clockDraft_(clockSetupDefault),
      lastNow_(clockSetupDefault),
      displayedMonth_{
          clockSetupDefault.date.year,
          clockSetupDefault.date.month
      },
      selectedDate_(),
      selectedEventId_(),
      dayPageIndex_(0),
      editor_(),
      editorReturnScreen_(AgendaUiScreen::Day),
      t9_(),
      pendingOperation_(AgendaUiPendingOperation::None),
      reminder_()
{
}

void AgendaUiStateMachine::start()
{
    startOrRetry();
}

void AgendaUiStateMachine::startOrRetry()
{
    resetErrors();
    overlay_ = AgendaUiOverlay::None;

    const AgendaStartResult started =
        controller_.start();

    if (started.error != AgendaControllerError::None)
    {
        controllerError_ = started.error;
        screen_ = AgendaUiScreen::StartupError;
        return;
    }

    const ClockReadResult current = clock_.read();

    if (
        current.error != ClockError::None ||
        !validDateTime(current.value)
    )
    {
        clockError_ =
            current.error == ClockError::None
                ? ClockError::InvalidDateTime
                : current.error;

        clockDraft_ = clockSetupDefault_;
        screen_ = AgendaUiScreen::ClockSetup;
        return;
    }

    lastNow_ = current.value;
    displayedMonth_ = YearMonth{
        current.value.date.year,
        current.value.date.month
    };

    screen_ = AgendaUiScreen::Month;
}

void AgendaUiStateMachine::tick()
{
}

void AgendaUiStateMachine::handle(
    const AgendaUiAction& action
)
{
    if (
        action.type == AgendaUiActionType::RetryStartup &&
        screen_ == AgendaUiScreen::StartupError
    )
    {
        startOrRetry();
    }
}

AgendaUiView AgendaUiStateMachine::view() const
{
    AgendaUiView result;
    result.screen = screen_;
    result.overlay = overlay_;
    result.controllerError = controllerError_;
    result.eventError = eventError_;
    result.clockError = clockError_;
    result.displayedMonth = displayedMonth_;
    result.clockDraft = clockDraft_;
    result.selectedDate = selectedDate_;
    result.dayPageIndex = dayPageIndex_;
    result.editor = editor_;
    result.t9Language = t9_.language();
    result.t9Mode = t9_.mode();
    result.reminder = reminder_;
    return result;
}

void AgendaUiStateMachine::resetErrors()
{
    controllerError_ = AgendaControllerError::None;
    eventError_ = EventError::None;
    clockError_ = ClockError::None;
}
```

The unused private declarations need no definition in this commit because C++ does not require a definition until a method is odr-used.

- [ ] **Step 5: Run the startup tests and verify green**

Run the compile command from Step 2, then:

```powershell
.\build\test_agenda_ui_state_machine.exe
```

Expected: `All agenda UI state machine tests passed.` with no compiler warnings.

- [ ] **Step 6: Stage and commit**

```powershell
git add src/core/clock.h src/core/agenda_ui_types.h src/core/agenda_ui_state_machine.h src/core/agenda_ui_state_machine.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: add agenda UI startup states"
```

---

### Task 2: Manual Clock Setup and Month Navigation

**Files:**
- Modify: `src/core/agenda_ui_state_machine.cpp`
- Modify: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: `Clock::set()`, `previousMonth()`, `nextMonth()`, `buildMonthGrid()`, and `AgendaController::agenda()`.
- Produces: clock setup actions, month navigation, owned month cells, event-dot flags, and neighbouring-date selection.

- [ ] **Step 1: Add failing clock and month tests**

Add tests before `main()` and register them in `main()`:

```cpp
void test_clock_setup_success_opens_selected_month()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.error = ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    ui.handle({AgendaUiActionType::ClockNextDay});
    ui.handle({AgendaUiActionType::ConfirmClock});

    const AgendaUiView view = ui.view();

    assert(clock.setCount == 1);
    assert(view.screen == AgendaUiScreen::Month);
    assert(view.clockError == ClockError::None);
}

void test_clock_set_failure_stays_in_setup()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.error = ClockError::Unavailable;
    clock.setResult = ClockError::WriteFailed;

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    ui.handle({AgendaUiActionType::ConfirmClock});

    const AgendaUiView view = ui.view();

    assert(view.screen == AgendaUiScreen::ClockSetup);
    assert(view.clockError == ClockError::WriteFailed);
}

void test_invalid_manual_clock_draft_is_not_written()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.error = ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(
        controller,
        clock,
        DateTime{
            Date{2026, 2, 30},
            TimeOfDay{10, 0}
        }
    );

    ui.start();
    ui.handle({AgendaUiActionType::ConfirmClock});

    assert(ui.view().screen == AgendaUiScreen::ClockSetup);
    assert(
        ui.view().clockError ==
        ClockError::InvalidDateTime
    );
    assert(clock.setCount == 0);
}

void test_month_navigation_and_neighbour_selection()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    ui.handle({AgendaUiActionType::PreviousMonth});

    assert(ui.view().displayedMonth.month == 8);

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectDate,
            Date{2026, 9, 1}
        }
    );

    const AgendaUiView view = ui.view();

    assert(view.screen == AgendaUiScreen::Day);
    assert(view.displayedMonth.month == 9);
    assert(view.selectedDate->day == 1);
}
```

Add this lookup helper and the remaining concrete cases:

```cpp
const AgendaUiMonthCell& monthCellFor(
    const AgendaUiView& view,
    const Date& date
)
{
    for (const AgendaUiMonthCell& cell : view.monthCells)
    {
        if (
            cell.date.has_value() &&
            cell.date->year == date.year &&
            cell.date->month == date.month &&
            cell.date->day == date.day
        )
        {
            return cell;
        }
    }

    assert(false);
    return view.monthCells[0];
}

void test_month_boundaries_ignore_unavailable_direction()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.value = DateTime{
        Date{2000, 1, 1},
        TimeOfDay{0, 0}
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, clock.readResult.value);

    ui.start();
    ui.handle({AgendaUiActionType::PreviousMonth});
    assert(ui.view().displayedMonth.year == 2000);
    assert(ui.view().displayedMonth.month == 1);
    assert(!ui.view().monthPreviousEnabled);

    for (int index = 0; index < 1199; ++index)
    {
        ui.handle({AgendaUiActionType::NextMonth});
    }

    assert(ui.view().displayedMonth.year == 2099);
    assert(ui.view().displayedMonth.month == 12);

    ui.handle({AgendaUiActionType::NextMonth});
    assert(ui.view().displayedMonth.year == 2099);
    assert(ui.view().displayedMonth.month == 12);
    assert(!ui.view().monthNextEnabled);
}

void test_month_cells_mark_today_selected_and_event_dates()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();

    const AgendaControllerCreateResult created =
        controller.createEvent(
            EventDraft{
                Date{2026, 9, 10},
                std::nullopt,
                "Marked"
            },
            uiTestNow()
        );

    assert(created.error == AgendaControllerError::None);
    assert(created.eventError == EventError::None);

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectDate,
            Date{2026, 9, 10}
        }
    );

    const AgendaUiView view = ui.view();
    const AgendaUiMonthCell& today =
        monthCellFor(view, Date{2026, 9, 8});
    const AgendaUiMonthCell& selected =
        monthCellFor(view, Date{2026, 9, 10});

    assert(today.isToday);
    assert(selected.isSelected);
    assert(selected.hasEvents);
}

void test_invalid_action_does_not_change_month_state()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    const AgendaUiView before = ui.view();

    ui.handle({AgendaUiActionType::NextDayPage});

    const AgendaUiView after = ui.view();
    assert(after.screen == before.screen);
    assert(after.displayedMonth.year == before.displayedMonth.year);
    assert(after.displayedMonth.month == before.displayedMonth.month);
    assert(storage.writeCount == 0);
}
```

Register all seven Task 2 tests in `main()`:

```cpp
test_clock_setup_success_opens_selected_month();
test_clock_set_failure_stays_in_setup();
test_invalid_manual_clock_draft_is_not_written();
test_month_navigation_and_neighbour_selection();
test_month_boundaries_ignore_unavailable_direction();
test_month_cells_mark_today_selected_and_event_dates();
test_invalid_action_does_not_change_month_state();
```

- [ ] **Step 2: Run and verify the expected assertion failures**

Compile and run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
```

Expected: the new tests fail because clock actions, month actions, and month-cell construction are not handled.

- [ ] **Step 3: Implement clock actions and month navigation**

Add this date helper in the anonymous namespace, then extend `handle()` only for actions valid in `ClockSetup` or `Month`:

```cpp
bool sameDate(
    const Date& left,
    const Date& right
)
{
    return
        left.year == right.year &&
        left.month == right.month &&
        left.day == right.day;
}

bool stepClockDate(
    Date& value,
    int direction
)
{
    if (!isValidDate(value) || direction == 0)
    {
        return false;
    }

    if (direction < 0)
    {
        if (
            value.year == 2000 &&
            value.month == 1 &&
            value.day == 1
        )
        {
            return false;
        }

        if (value.day > 1)
        {
            --value.day;
        }
        else
        {
            if (value.month == 1)
            {
                --value.year;
                value.month = 12;
            }
            else
            {
                --value.month;
            }

            value.day =
                daysInMonth(value.year, value.month);
        }

        return true;
    }

    if (
        value.year == 2099 &&
        value.month == 12 &&
        value.day == 31
    )
    {
        return false;
    }

    if (value.day < daysInMonth(value.year, value.month))
    {
        ++value.day;
    }
    else
    {
        value.day = 1;

        if (value.month == 12)
        {
            ++value.year;
            value.month = 1;
        }
        else
        {
            ++value.month;
        }
    }

    return true;
}
```

Handle `ClockSetup` with this complete branch before the month branch:

```cpp
if (screen_ == AgendaUiScreen::ClockSetup)
{
    switch (action.type)
    {
        case AgendaUiActionType::ClockPreviousDay:
            stepClockDate(clockDraft_.date, -1);
            return;

        case AgendaUiActionType::ClockNextDay:
            stepClockDate(clockDraft_.date, 1);
            return;

        case AgendaUiActionType::ClockPreviousHour:
            clockDraft_.time.hour =
                (clockDraft_.time.hour + 23) % 24;
            return;

        case AgendaUiActionType::ClockNextHour:
            clockDraft_.time.hour =
                (clockDraft_.time.hour + 1) % 24;
            return;

        case AgendaUiActionType::ClockPreviousMinute:
            clockDraft_.time.minute =
                (clockDraft_.time.minute + 59) % 60;
            return;

        case AgendaUiActionType::ClockNextMinute:
            clockDraft_.time.minute =
                (clockDraft_.time.minute + 1) % 60;
            return;

        case AgendaUiActionType::ConfirmClock:
            break;

        default:
            return;
    }

    if (
        !isValidDate(clockDraft_.date) ||
        !isValidTime(clockDraft_.time)
    )
    {
        clockError_ = ClockError::InvalidDateTime;
        return;
    }

    const ClockError setError =
        clock_.set(clockDraft_);

    if (setError != ClockError::None)
    {
        clockError_ = setError;
        return;
    }

    lastNow_ = clockDraft_;
    displayedMonth_ = YearMonth{
        clockDraft_.date.year,
        clockDraft_.date.month
    };
    clockError_ = ClockError::None;
    screen_ = AgendaUiScreen::Month;
    return;
}
```

Clock minutes change by one minute because manual clock accuracy is distinct from five-minute event selection. The date helper stops at the supported boundaries, while hours and minutes wrap within their own fields.

Handle `Month` with this switch. If a month result is empty, keep the same month:

```cpp
if (screen_ == AgendaUiScreen::Month)
{
    switch (action.type)
    {
        case AgendaUiActionType::PreviousMonth:
        {
            const std::optional<YearMonth> previous =
                previousMonth(displayedMonth_);

            if (previous.has_value())
            {
                displayedMonth_ = *previous;
            }

            return;
        }

        case AgendaUiActionType::NextMonth:
        {
            const std::optional<YearMonth> next =
                nextMonth(displayedMonth_);

            if (next.has_value())
            {
                displayedMonth_ = *next;
            }

            return;
        }

        case AgendaUiActionType::SelectDate:
            if (
                !action.date.has_value() ||
                !isValidDate(*action.date)
            )
            {
                return;
            }

            selectedDate_ = *action.date;
            displayedMonth_ = YearMonth{
                action.date->year,
                action.date->month
            };
            dayPageIndex_ = 0;
            screen_ = AgendaUiScreen::Day;
            return;

        default:
            return;
    }
}
```

- [ ] **Step 4: Build complete month cells in `view()`**

Implement `rebuildMonthCells()`:

```cpp
void AgendaUiStateMachine::rebuildMonthCells(
    AgendaUiView& result
) const
{
    result.monthPreviousEnabled =
        previousMonth(displayedMonth_).has_value();
    result.monthNextEnabled =
        nextMonth(displayedMonth_).has_value();

    const MonthGrid grid =
        buildMonthGrid(displayedMonth_);

    for (std::size_t index = 0;
         index < grid.size();
         ++index)
    {
        AgendaUiMonthCell& target =
            result.monthCells[index];

        target.date = grid[index].date;
        target.inDisplayedMonth =
            grid[index].inDisplayedMonth;

        if (!target.date.has_value())
        {
            continue;
        }

        target.isToday =
            sameDate(*target.date, lastNow_.date);

        target.isSelected =
            selectedDate_.has_value() &&
            sameDate(*target.date, *selectedDate_);

        for (const Event& event :
             controller_.agenda().events())
        {
            if (sameDate(event.eventDate, *target.date))
            {
                target.hasEvents = true;
                break;
            }
        }
    }
}
```

Call it from `view()` only after successful controller startup.

- [ ] **Step 5: Run UI and calendar regressions**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
.\build\test_calendar_engine.exe
```

Expected: both suites pass with warning-free compilation.

- [ ] **Step 6: Stage and commit**

```powershell
git add src/core/agenda_ui_state_machine.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: add agenda UI clock and month navigation"
```

---

### Task 3: Day Pagination and Event Details

**Files:**
- Modify: `src/core/agenda_ui_state_machine.cpp`
- Modify: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: the sorted `AgendaController::agenda().events()` collection and `AgendaUiAction` date/event payloads.
- Produces: three-row day pages, deterministic page counts, safe details copies, and valid Back transitions.

- [ ] **Step 1: Add failing browsing tests**

Add `#include "core/event_codec.h"` and `#include "core/utf8.h"` to the UI test. Add these deterministic fixtures:

```cpp
Event uiStoredEvent(
    std::uint32_t id,
    const Date& date,
    const std::string& text
)
{
    return Event{
        id,
        date,
        std::nullopt,
        text,
        false,
        DateTime{
            Date{2026, 9, 1},
            TimeOfDay{9, 0}
        }
    };
}

void storeUiState(
    UiTestStorage& storage,
    const AgendaState& state
)
{
    const EventEncodeResult encoded =
        encodeAgendaState(state);

    assert(encoded.error == EventCodecError::None);
    storage.bytes = encoded.bytes;
    storage.readError = ByteStorageError::None;
}

void openUiDay(
    AgendaUiStateMachine& ui,
    const Date& date
)
{
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectDate,
            date
        }
    );
}
```

Add and register these three complete cases:

```cpp
void test_day_filtering_pagination_and_boundaries()
{
    UiTestStorage storage;
    UiTestClock clock;
    std::vector<Event> events;

    for (std::uint32_t id = 1; id <= 7; ++id)
    {
        events.push_back(
            uiStoredEvent(
                id,
                Date{2026, 9, 9},
                "Event " + std::to_string(id)
            )
        );
    }

    events.push_back(
        uiStoredEvent(
            8,
            Date{2026, 9, 10},
            "Other date"
        )
    );

    storeUiState(storage, AgendaState{events, 9});

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 9});

    const AgendaUiView first = ui.view();
    assert(first.dayPageIndex == 0);
    assert(first.dayPageCount == 3);
    assert(first.dayEvents.size() == 3);
    assert(first.dayEvents[0].eventId == 1);
    assert(!first.dayPreviousEnabled);
    assert(first.dayNextEnabled);

    ui.handle({AgendaUiActionType::NextDayPage});
    ui.handle({AgendaUiActionType::NextDayPage});

    const AgendaUiView last = ui.view();
    assert(last.dayPageIndex == 2);
    assert(last.dayEvents.size() == 1);
    assert(last.dayEvents[0].eventId == 7);
    assert(last.dayPreviousEnabled);
    assert(!last.dayNextEnabled);

    ui.handle({AgendaUiActionType::NextDayPage});
    assert(ui.view().dayPageIndex == 2);

    ui.handle({AgendaUiActionType::PreviousDayPage});
    ui.handle({AgendaUiActionType::PreviousDayPage});
    ui.handle({AgendaUiActionType::PreviousDayPage});
    assert(ui.view().dayPageIndex == 0);

    ui.handle({AgendaUiActionType::Back});
    openUiDay(ui, Date{2026, 9, 11});
    assert(ui.view().dayPageCount == 1);
    assert(ui.view().dayEvents.empty());
    assert(!ui.view().dayPreviousEnabled);
    assert(!ui.view().dayNextEnabled);
}

void test_day_rows_are_utf8_shortened_copies()
{
    UiTestStorage storage;
    UiTestClock clock;
    std::string longText;

    for (int index = 0; index < 40; ++index)
    {
        longText += u8"α";
    }

    storeUiState(
        storage,
        AgendaState{
            {
                uiStoredEvent(
                    1,
                    Date{2026, 9, 9},
                    longText
                )
            },
            2
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 9});

    const AgendaUiView view = ui.view();
    assert(view.dayEvents.size() == 1);
    assert(utf8CodePointCount(view.dayEvents[0].text) == 32);
    assert(utf8CodePointCount(longText) == 40);
}

void test_details_selection_is_valid_owned_and_returns_to_same_page()
{
    UiTestStorage storage;
    UiTestClock clock;
    storeUiState(
        storage,
        AgendaState{
            {
                uiStoredEvent(1, Date{2026, 9, 9}, "One"),
                uiStoredEvent(2, Date{2026, 9, 9}, "Two"),
                uiStoredEvent(3, Date{2026, 9, 9}, "Three"),
                uiStoredEvent(4, Date{2026, 9, 9}, "Original"),
                uiStoredEvent(5, Date{2026, 9, 10}, "Other")
            },
            6
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 9});
    ui.handle({AgendaUiActionType::NextDayPage});

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            5
        }
    );
    assert(ui.view().screen == AgendaUiScreen::Day);

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            4
        }
    );

    const AgendaUiView before = ui.view();
    assert(before.screen == AgendaUiScreen::EventDetails);
    assert(before.selectedEvent->text == "Original");

    const AgendaControllerChangeResult updated =
        controller.updateEvent(
            4,
            EventDraft{
                Date{2026, 9, 9},
                std::nullopt,
                "Changed"
            },
            uiTestNow()
        );

    assert(updated.error == AgendaControllerError::None);
    assert(updated.eventError == EventError::None);
    assert(before.selectedEvent->text == "Original");
    assert(ui.view().selectedEvent->text == "Changed");

    ui.handle({AgendaUiActionType::Back});
    assert(ui.view().screen == AgendaUiScreen::Day);
    assert(ui.view().dayPageIndex == 1);
}
```

Add these calls to `main()`:

```cpp
test_day_filtering_pagination_and_boundaries();
test_day_rows_are_utf8_shortened_copies();
test_details_selection_is_valid_owned_and_returns_to_same_page();
```

- [ ] **Step 2: Run and verify red**

Compile and run the UI suite.

Expected: assertions fail because `Day` currently exposes no rows or details and ignores pagination actions.

- [ ] **Step 3: Implement day filtering and pagination**

Implement a private filtered-event helper inside the `.cpp` anonymous namespace. It returns event pointers only for immediate internal calculation; no pointer enters the public view model.

```cpp
std::vector<const Event*> eventsForDate(
    const AgendaManager& agenda,
    const Date& date
)
{
    std::vector<const Event*> result;

    for (const Event& event : agenda.events())
    {
        if (sameDate(event.eventDate, date))
        {
            result.push_back(&event);
        }
    }

    return result;
}

std::string firstUiCodePoints(
    const std::string& text
)
{
    constexpr std::size_t limit = 32;
    std::size_t position = 0;
    std::size_t count = 0;

    while (position < text.size() && count < limit)
    {
        ++position;

        while (
            position < text.size() &&
            (
                static_cast<unsigned char>(text[position]) &
                0xC0
            ) == 0x80
        )
        {
            ++position;
        }

        ++count;
    }

    return text.substr(0, position);
}
```

Define an empty day as one page so the renderer can display stable page metadata:

```cpp
const std::size_t pageCount =
    events.empty()
        ? 1
        : (events.size() + AGENDA_UI_EVENTS_PER_PAGE - 1) /
              AGENDA_UI_EVENTS_PER_PAGE;
```

Implement both view rebuilding and clamping exactly once:

```cpp
void AgendaUiStateMachine::clampDayPage()
{
    if (!selectedDate_.has_value())
    {
        dayPageIndex_ = 0;
        return;
    }

    const std::size_t count =
        eventsForDate(
            controller_.agenda(),
            *selectedDate_
        ).size();

    const std::size_t pages =
        count == 0
            ? 1
            : (count + AGENDA_UI_EVENTS_PER_PAGE - 1) /
                  AGENDA_UI_EVENTS_PER_PAGE;

    if (dayPageIndex_ >= pages)
    {
        dayPageIndex_ = pages - 1;
    }
}

void AgendaUiStateMachine::rebuildDayRows(
    AgendaUiView& result
) const
{
    result.dayEvents.clear();

    if (!selectedDate_.has_value())
    {
        result.dayPageCount = 1;
        return;
    }

    const std::vector<const Event*> events =
        eventsForDate(
            controller_.agenda(),
            *selectedDate_
        );

    result.dayPageCount =
        events.empty()
            ? 1
            : (events.size() + AGENDA_UI_EVENTS_PER_PAGE - 1) /
                  AGENDA_UI_EVENTS_PER_PAGE;

    result.dayPreviousEnabled =
        dayPageIndex_ > 0;
    result.dayNextEnabled =
        dayPageIndex_ + 1 < result.dayPageCount;

    const std::size_t first =
        dayPageIndex_ * AGENDA_UI_EVENTS_PER_PAGE;

    const std::size_t last =
        std::min(
            first + AGENDA_UI_EVENTS_PER_PAGE,
            events.size()
        );

    for (std::size_t index = first; index < last; ++index)
    {
        result.dayEvents.push_back(
            AgendaUiEventRow{
                events[index]->id,
                events[index]->eventTime,
                firstUiCodePoints(events[index]->text)
            }
        );
    }
}
```

Add `#include <algorithm>` and `#include <vector>` to the `.cpp`. Call `rebuildDayRows(result)` from `view()` when the controller is ready. Handle pagination only on `Day` with this code:

```cpp
if (
    screen_ == AgendaUiScreen::Day &&
    action.type == AgendaUiActionType::PreviousDayPage
)
{
    clampDayPage();

    if (dayPageIndex_ > 0)
    {
        --dayPageIndex_;
    }

    return;
}

if (
    screen_ == AgendaUiScreen::Day &&
    action.type == AgendaUiActionType::NextDayPage
)
{
    clampDayPage();
    AgendaUiView current;
    current.dayPageIndex = dayPageIndex_;
    rebuildDayRows(current);

    if (dayPageIndex_ + 1 < current.dayPageCount)
    {
        ++dayPageIndex_;
    }

    return;
}
```

- [ ] **Step 4: Implement details selection and Back**

`SelectEvent` requires `screen_ == Day`, a supplied event ID, and an event whose date matches `selectedDate_`. Use the concrete guard below, store only the ID, and open `EventDetails`:

```cpp
const Event* selected =
    controller_.agenda().findEvent(
        *action.eventId
    );

if (
    selected == nullptr ||
    !selectedDate_.has_value() ||
    !sameDate(selected->eventDate, *selectedDate_)
)
{
    return;
}

selectedEventId_ = selected->id;
screen_ = AgendaUiScreen::EventDetails;
```

In `view()`, copy the selected event into `result.selectedEvent`:

```cpp
if (selectedEventId_.has_value())
{
    const Event* selected =
        controller_.agenda().findEvent(
            *selectedEventId_
        );

    if (selected != nullptr)
    {
        result.selectedEvent = *selected;
    }
}
```

If it no longer exists, return no details; the next handled action normalizes the screen to `Day`.

Implement the two Back transitions:

```cpp
if (action.type == AgendaUiActionType::Back)
{
    if (screen_ == AgendaUiScreen::EventDetails)
    {
        screen_ = AgendaUiScreen::Day;
        return;
    }

    if (screen_ == AgendaUiScreen::Day)
    {
        screen_ = AgendaUiScreen::Month;
        return;
    }
}
```

Other Back behavior is added with the editor task.

- [ ] **Step 5: Verify UI suite**

Compile and run:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
```

Expected: all UI tests pass with no warnings.

- [ ] **Step 6: Stage and commit**

```powershell
git add src/core/agenda_ui_state_machine.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: add agenda UI day browsing"
```

---

### Task 4: Editor Navigation, Date/Time Controls, and Discard Safety

**Files:**
- Modify: `src/core/date_time.h`
- Modify: `src/core/date_time.cpp`
- Modify: `src/core/agenda_ui_state_machine.cpp`
- Modify: `tests/test_date_time.cpp`
- Modify: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: selected date/event and supported date range.
- Produces: `previousDate()`, `nextDate()`, create/update drafts, four editor screens, five-minute time controls, and dirty-discard confirmation.

- [ ] **Step 1: Add failing bounded-date tests**

Add to `tests/test_date_time.cpp` and register in `main()`:

```cpp
void test_previous_and_next_date_cross_boundaries()
{
    const auto normalNext =
        nextDate(Date{2026, 9, 8});

    assert(normalNext.has_value());
    assert(normalNext->year == 2026);
    assert(normalNext->month == 9);
    assert(normalNext->day == 9);

    const auto leapNext =
        nextDate(Date{2028, 2, 28});

    assert(leapNext.has_value());
    assert(leapNext->year == 2028);
    assert(leapNext->month == 2);
    assert(leapNext->day == 29);

    const auto yearNext =
        nextDate(Date{2026, 12, 31});

    assert(yearNext.has_value());
    assert(yearNext->year == 2027);
    assert(yearNext->month == 1);
    assert(yearNext->day == 1);

    const auto yearPrevious =
        previousDate(Date{2026, 1, 1});

    assert(yearPrevious.has_value());
    assert(yearPrevious->year == 2025);
    assert(yearPrevious->month == 12);
    assert(yearPrevious->day == 31);

    const auto monthPrevious =
        previousDate(Date{2026, 3, 1});

    assert(monthPrevious.has_value());
    assert(monthPrevious->year == 2026);
    assert(monthPrevious->month == 2);
    assert(monthPrevious->day == 28);
}

void test_date_steps_stop_at_supported_range()
{
    assert(
        !previousDate(Date{2000, 1, 1}).has_value()
    );

    assert(
        !nextDate(Date{2099, 12, 31}).has_value()
    );

    assert(
        !nextDate(Date{2026, 2, 30}).has_value()
    );
}
```

Add these calls to `tests/test_date_time.cpp` `main()`:

```cpp
test_previous_and_next_date_cross_boundaries();
test_date_steps_stop_at_supported_range();
```

- [ ] **Step 2: Verify date-time compilation fails**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_date_time.cpp src\core\date_time.cpp -o build\test_date_time.exe
```

Expected: compilation fails because `previousDate()` and `nextDate()` are undeclared.

- [ ] **Step 3: Implement bounded date stepping**

Add `#include <optional>` and these declarations to `date_time.h`:

```cpp
std::optional<Date> previousDate(
    const Date& value
);

std::optional<Date> nextDate(
    const Date& value
);
```

Add `#include <optional>` to `date_time.cpp` and implement the functions without unchecked serial-date conversion:

```cpp
std::optional<Date> previousDate(
    const Date& value
)
{
    if (
        !isValidDate(value) ||
        (
            value.year == 2000 &&
            value.month == 1 &&
            value.day == 1
        )
    )
    {
        return std::nullopt;
    }

    Date result = value;

    if (result.day > 1)
    {
        --result.day;
        return result;
    }

    if (result.month > 1)
    {
        --result.month;
    }
    else
    {
        --result.year;
        result.month = 12;
    }

    result.day =
        daysInMonth(result.year, result.month);

    return result;
}

std::optional<Date> nextDate(
    const Date& value
)
{
    if (
        !isValidDate(value) ||
        (
            value.year == 2099 &&
            value.month == 12 &&
            value.day == 31
        )
    )
    {
        return std::nullopt;
    }

    Date result = value;
    const int lastDay =
        daysInMonth(result.year, result.month);

    if (result.day < lastDay)
    {
        ++result.day;
        return result;
    }

    result.day = 1;

    if (result.month < 12)
    {
        ++result.month;
    }
    else
    {
        ++result.year;
        result.month = 1;
    }

    return result;
}
```

Replace the private `stepClockDate()` body in `agenda_ui_state_machine.cpp` so both clock setup and the editor share the tested date rules:

```cpp
bool stepClockDate(
    Date& value,
    int direction
)
{
    const std::optional<Date> moved =
        direction < 0
            ? previousDate(value)
            : nextDate(value);

    if (!moved.has_value())
    {
        return false;
    }

    value = *moved;
    return true;
}
```

- [ ] **Step 4: Run date tests and verify green**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_date_time.cpp src\core\date_time.cpp -o build\test_date_time.exe
.\build\test_date_time.exe
```

Expected: `All date/time tests passed.`

- [ ] **Step 5: Add failing editor-state tests**

Add and register these complete editor cases:

```cpp
void test_create_editor_navigation_and_time_controls()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openUiDay(ui, Date{2026, 9, 9});
    ui.handle({AgendaUiActionType::StartCreate});

    AgendaUiView view = ui.view();
    assert(view.screen == AgendaUiScreen::EditorDate);
    assert(view.editor.mode == AgendaUiEditorMode::Create);
    assert(view.editor.date.day == 9);
    assert(!view.editor.time.has_value());
    assert(view.editor.text.empty());
    assert(!view.editor.dirty);

    ui.handle({AgendaUiActionType::EditorNext});
    assert(ui.view().screen == AgendaUiScreen::EditorTime);

    ui.handle({AgendaUiActionType::ToggleEventTime});
    assert(ui.view().editor.time->hour == 12);
    assert(ui.view().editor.time->minute == 0);

    ui.handle({AgendaUiActionType::EditorPreviousMinute});
    assert(ui.view().editor.time->hour == 11);
    assert(ui.view().editor.time->minute == 55);

    ui.handle({AgendaUiActionType::EditorNextMinute});
    assert(ui.view().editor.time->hour == 12);
    assert(ui.view().editor.time->minute == 0);

    for (int index = 0; index < 13; ++index)
    {
        ui.handle({AgendaUiActionType::EditorPreviousHour});
    }

    assert(ui.view().editor.time->hour == 23);

    for (int index = 0; index < 11; ++index)
    {
        ui.handle({AgendaUiActionType::EditorNextMinute});
    }

    assert(ui.view().editor.time->hour == 23);
    assert(ui.view().editor.time->minute == 55);
    ui.handle({AgendaUiActionType::EditorNextMinute});
    assert(ui.view().editor.time->hour == 0);
    assert(ui.view().editor.time->minute == 0);

    ui.handle({AgendaUiActionType::EditorNext});
    assert(ui.view().screen == AgendaUiScreen::EditorText);
    ui.handle({AgendaUiActionType::EditorNext});
    assert(ui.view().screen == AgendaUiScreen::EditorReview);
    ui.handle({AgendaUiActionType::EditorBack});
    assert(ui.view().screen == AgendaUiScreen::EditorText);
    ui.handle({AgendaUiActionType::EditorBack});
    assert(ui.view().screen == AgendaUiScreen::EditorTime);
    ui.handle({AgendaUiActionType::EditorBack});
    assert(ui.view().screen == AgendaUiScreen::EditorDate);
}

void test_editor_date_boundaries_and_discard_flow()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openUiDay(ui, Date{2099, 12, 31});
    ui.handle({AgendaUiActionType::StartCreate});
    ui.handle({AgendaUiActionType::EditorNextDay});
    assert(ui.view().editor.date.year == 2099);
    assert(ui.view().editor.date.month == 12);
    assert(ui.view().editor.date.day == 31);
    assert(!ui.view().editor.dirty);

    ui.handle({AgendaUiActionType::EditorPreviousDay});
    assert(ui.view().editor.date.day == 30);
    assert(ui.view().editor.dirty);

    ui.handle({AgendaUiActionType::EditorCancel});
    assert(ui.view().overlay == AgendaUiOverlay::ConfirmDiscard);
    assert(ui.view().screen == AgendaUiScreen::EditorDate);

    ui.handle({AgendaUiActionType::CancelDiscard});
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(ui.view().screen == AgendaUiScreen::EditorDate);
    assert(ui.view().editor.date.day == 30);

    ui.handle({AgendaUiActionType::EditorCancel});
    ui.handle({AgendaUiActionType::ConfirmDiscard});
    assert(ui.view().screen == AgendaUiScreen::Day);
    assert(ui.view().overlay == AgendaUiOverlay::None);
}

void test_update_editor_copies_event_and_discard_returns_to_details()
{
    UiTestStorage storage;
    UiTestClock clock;
    storeUiState(
        storage,
        AgendaState{
            {
                Event{
                    7,
                    Date{2026, 9, 10},
                    std::optional<TimeOfDay>{
                        TimeOfDay{14, 25}
                    },
                    "Appointment",
                    false,
                    uiTestNow()
                }
            },
            8
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 10});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            7
        }
    );
    ui.handle({AgendaUiActionType::StartEdit});

    const AgendaUiView opened = ui.view();
    assert(opened.screen == AgendaUiScreen::EditorDate);
    assert(opened.editor.mode == AgendaUiEditorMode::Update);
    assert(opened.editor.eventId == 7);
    assert(opened.editor.time->hour == 14);
    assert(opened.editor.time->minute == 25);
    assert(opened.editor.text == "Appointment");
    assert(!opened.editor.dirty);

    ui.handle({AgendaUiActionType::EditorNextDay});
    ui.handle({AgendaUiActionType::EditorCancel});
    ui.handle({AgendaUiActionType::ConfirmDiscard});

    assert(ui.view().screen == AgendaUiScreen::EventDetails);
    assert(ui.view().selectedEvent->id == 7);
    assert(controller.agenda().findEvent(7)->text == "Appointment");
}

void test_clean_cancel_and_invalid_editor_action_are_side_effect_free()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openUiDay(ui, Date{2026, 9, 9});
    ui.handle({AgendaUiActionType::StartCreate});
    ui.handle({AgendaUiActionType::NextMonth});
    assert(ui.view().screen == AgendaUiScreen::EditorDate);
    assert(!ui.view().editor.dirty);
    assert(storage.writeCount == 0);

    ui.handle({AgendaUiActionType::EditorCancel});
    assert(ui.view().screen == AgendaUiScreen::Day);
    assert(ui.view().overlay == AgendaUiOverlay::None);
}
```

Add these calls to `main()`:

```cpp
test_create_editor_navigation_and_time_controls();
test_editor_date_boundaries_and_discard_flow();
test_update_editor_copies_event_and_discard_returns_to_details();
test_clean_cancel_and_invalid_editor_action_are_side_effect_free();
```

- [ ] **Step 6: Run and verify editor tests fail**

Compile and run the UI suite.

Expected: the first new editor assertion fails because create/edit actions are not handled.

- [ ] **Step 7: Implement the editor state transitions**

Implement the two editor entry helpers with owned values:

```cpp
void AgendaUiStateMachine::beginCreate()
{
    if (!selectedDate_.has_value())
    {
        return;
    }

    editor_ = AgendaUiEditorDraft{};
    editor_.mode = AgendaUiEditorMode::Create;
    editor_.date = *selectedDate_;
    editor_.time = std::nullopt;
    editor_.text.clear();
    editor_.dirty = false;
    editorReturnScreen_ = AgendaUiScreen::Day;
    screen_ = AgendaUiScreen::EditorDate;
}

void AgendaUiStateMachine::beginEdit()
{
    if (!selectedEventId_.has_value())
    {
        return;
    }

    const Event* event =
        controller_.agenda().findEvent(
            *selectedEventId_
        );

    if (event == nullptr)
    {
        screen_ = AgendaUiScreen::Day;
        selectedEventId_.reset();
        return;
    }

    editor_.mode = AgendaUiEditorMode::Update;
    editor_.eventId = event->id;
    editor_.date = event->eventDate;
    editor_.time = event->eventTime;
    editor_.text = event->text;
    editor_.dirty = false;
    editorReturnScreen_ = AgendaUiScreen::EventDetails;
    screen_ = AgendaUiScreen::EditorDate;
}
```

Every actual field change sets `editor_.dirty = true`. Merely moving between wizard steps does not.

Open editors only from their valid origin:

```cpp
if (
    screen_ == AgendaUiScreen::Day &&
    action.type == AgendaUiActionType::StartCreate
)
{
    beginCreate();
    return;
}

if (
    screen_ == AgendaUiScreen::EventDetails &&
    action.type == AgendaUiActionType::StartEdit
)
{
    beginEdit();
    return;
}
```

Apply the four-step wizard transitions with this code:

```cpp
if (action.type == AgendaUiActionType::EditorNext)
{
    if (screen_ == AgendaUiScreen::EditorDate)
    {
        screen_ = AgendaUiScreen::EditorTime;
        return;
    }

    if (screen_ == AgendaUiScreen::EditorTime)
    {
        screen_ = AgendaUiScreen::EditorText;
        return;
    }

    if (screen_ == AgendaUiScreen::EditorText)
    {
        screen_ = AgendaUiScreen::EditorReview;
        return;
    }
}

if (action.type == AgendaUiActionType::EditorBack)
{
    if (screen_ == AgendaUiScreen::EditorReview)
    {
        screen_ = AgendaUiScreen::EditorText;
        return;
    }

    if (screen_ == AgendaUiScreen::EditorText)
    {
        screen_ = AgendaUiScreen::EditorTime;
        return;
    }

    if (screen_ == AgendaUiScreen::EditorTime)
    {
        screen_ = AgendaUiScreen::EditorDate;
        return;
    }
}
```

Use one exit helper and one cancel branch for every editor screen:

```cpp
void AgendaUiStateMachine::leaveEditorAfterDiscard()
{
    screen_ = editorReturnScreen_;
    overlay_ = AgendaUiOverlay::None;
    editor_ = AgendaUiEditorDraft{};
    t9_ = T9Engine{};
}

if (
    action.type == AgendaUiActionType::EditorCancel ||
    (
        action.type == AgendaUiActionType::EditorBack &&
        screen_ == AgendaUiScreen::EditorDate
    )
)
{
    if (editor_.dirty)
    {
        overlay_ = AgendaUiOverlay::ConfirmDiscard;
    }
    else
    {
        leaveEditorAfterDiscard();
    }

    return;
}
```

Handle the discard overlay before ordinary screen actions so nothing underneath it can change:

```cpp
if (overlay_ == AgendaUiOverlay::ConfirmDiscard)
{
    if (action.type == AgendaUiActionType::CancelDiscard)
    {
        overlay_ = AgendaUiOverlay::None;
    }
    else if (action.type == AgendaUiActionType::ConfirmDiscard)
    {
        leaveEditorAfterDiscard();
    }

    return;
}

if (overlay_ != AgendaUiOverlay::None)
{
    return;
}
```

Time rules:

```cpp
// all-day -> timed
editor_.time = TimeOfDay{12, 0};

// timed -> all-day
editor_.time = std::nullopt;
```

Use minute-of-day arithmetic for all timed adjustments:

```cpp
void adjustEditorMinutes(
    std::optional<TimeOfDay>& value,
    int delta
)
{
    if (!value.has_value())
    {
        return;
    }

    const int current =
        value->hour * 60 + value->minute;

    const int wrapped =
        (current + delta + 1440) % 1440;

    value = TimeOfDay{
        wrapped / 60,
        wrapped % 60
    };
}
```

Hour actions pass `-60` or `60`; minute actions pass `-5` or `5`. Set `dirty` only when the optional time actually changes. Date actions assign the optional result from `previousDate()`/`nextDate()` only when it exists. Implement `EditorCancel`, `CancelDiscard`, and `ConfirmDiscard` with the exact transitions exercised above; `leaveEditorAfterDiscard()` clears the draft and returns to `editorReturnScreen_`.

Use this field-action structure so actions on the wrong wizard step remain no-ops:

```cpp
if (screen_ == AgendaUiScreen::EditorDate)
{
    if (
        action.type == AgendaUiActionType::EditorPreviousDay ||
        action.type == AgendaUiActionType::EditorNextDay
    )
    {
        const std::optional<Date> moved =
            action.type == AgendaUiActionType::EditorPreviousDay
                ? previousDate(editor_.date)
                : nextDate(editor_.date);

        if (moved.has_value())
        {
            editor_.date = *moved;
            editor_.dirty = true;
        }

        return;
    }
}

if (screen_ == AgendaUiScreen::EditorTime)
{
    if (action.type == AgendaUiActionType::ToggleEventTime)
    {
        editor_.time =
            editor_.time.has_value()
                ? std::optional<TimeOfDay>{}
                : std::optional<TimeOfDay>{
                      TimeOfDay{12, 0}
                  };
        editor_.dirty = true;
        return;
    }

    int delta = 0;

    if (action.type == AgendaUiActionType::EditorPreviousHour)
    {
        delta = -60;
    }
    else if (action.type == AgendaUiActionType::EditorNextHour)
    {
        delta = 60;
    }
    else if (action.type == AgendaUiActionType::EditorPreviousMinute)
    {
        delta = -5;
    }
    else if (action.type == AgendaUiActionType::EditorNextMinute)
    {
        delta = 5;
    }

    if (delta != 0 && editor_.time.has_value())
    {
        adjustEditorMinutes(editor_.time, delta);
        editor_.dirty = true;
        return;
    }
}
```

- [ ] **Step 8: Run editor and date regressions**

```powershell
.\build\test_date_time.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
```

Expected: both suites pass without warnings.

- [ ] **Step 9: Stage and commit**

```powershell
git add src/core/date_time.h src/core/date_time.cpp src/core/agenda_ui_state_machine.cpp tests/test_date_time.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: add agenda UI editor navigation"
```

---

### Task 5: T9 Editor Integration

**Files:**
- Modify: `src/core/t9_engine.h`
- Modify: `src/core/t9_engine.cpp`
- Modify: `src/core/agenda_ui_state_machine.cpp`
- Modify: `tests/test_t9_engine.cpp`
- Modify: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: `T9Engine` digit, language, mode, case, pending-character, text, and backspace behavior.
- Produces: read-only `T9Engine::uppercase()` and semantic T9 actions reflected in the editor view.

- [ ] **Step 1: Add the failing T9 case-accessor test**

Add and register in `tests/test_t9_engine.cpp`:

```cpp
void test_case_accessor_tracks_toggle()
{
    T9Engine engine;

    assert(!engine.uppercase());

    engine.toggleCase();
    assert(engine.uppercase());

    engine.toggleCase();
    assert(!engine.uppercase());
}
```

Add this call to `tests/test_t9_engine.cpp` `main()`:

```cpp
test_case_accessor_tracks_toggle();
```

- [ ] **Step 2: Verify red and add the accessor**

Compile the T9 suite and confirm failure because `uppercase()` is missing. Then add:

```cpp
// t9_engine.h, public section
bool uppercase() const;
```

```cpp
// t9_engine.cpp
bool T9Engine::uppercase() const
{
    return uppercase_;
}
```

Recompile and run `test_t9_engine.exe`; require a clean pass.

- [ ] **Step 3: Add failing UI T9 tests**

Add this navigation helper and register both complete cases below:

```cpp
void openCreateTextEditor(
    AgendaUiStateMachine& ui,
    const Date& date
)
{
    openUiDay(ui, date);
    ui.handle({AgendaUiActionType::StartCreate});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::EditorNext});
    assert(ui.view().screen == AgendaUiScreen::EditorText);
}

void test_t9_editor_uses_real_modes_cycles_space_and_backspace()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openCreateTextEditor(ui, Date{2026, 9, 9});

    assert(ui.view().t9Language == T9Language::Greek);
    assert(ui.view().t9Mode == T9Mode::Letters);
    assert(!ui.view().t9Uppercase);

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            100
        }
    );
    assert(ui.view().editor.text == u8"α");
    assert(ui.view().editor.dirty);

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            500
        }
    );
    assert(ui.view().editor.text == u8"ά");

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            1500
        }
    );
    assert(ui.view().editor.text == u8"άα");

    ui.handle({AgendaUiActionType::ToggleT9Case});
    assert(ui.view().t9Uppercase);
    ui.handle({AgendaUiActionType::ToggleT9Language});
    assert(ui.view().t9Language == T9Language::English);
    ui.handle({AgendaUiActionType::ToggleT9Mode});
    assert(ui.view().t9Mode == T9Mode::Numbers);

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '5',
            1600
        }
    );
    assert(ui.view().editor.text == u8"άα5");

    ui.handle({AgendaUiActionType::ToggleT9Mode});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '0',
            1700
        }
    );
    assert(ui.view().editor.text == u8"άα5 ");

    ui.handle({AgendaUiActionType::T9Backspace});
    assert(ui.view().editor.text == u8"άα5");

    ui.handle({AgendaUiActionType::EditorNext});
    assert(ui.view().screen == AgendaUiScreen::EditorReview);
    assert(ui.view().editor.text == u8"άα5");
}

void test_t9_limit_and_screen_guard_preserve_text()
{
    UiTestStorage storage;
    UiTestClock clock;
    const std::string fullText(500, 'x');

    storeUiState(
        storage,
        AgendaState{
            {
                uiStoredEvent(
                    7,
                    Date{2026, 9, 9},
                    fullText
                )
            },
            8
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            100
        }
    );
    assert(ui.view().screen == AgendaUiScreen::Month);
    assert(ui.view().editor.text.empty());

    openUiDay(ui, Date{2026, 9, 9});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            7
        }
    );
    ui.handle({AgendaUiActionType::StartEdit});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::EditorNext});

    assert(ui.view().editor.text == fullText);
    assert(!ui.view().editor.dirty);

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            200
        }
    );

    assert(ui.view().editor.text == fullText);
    assert(!ui.view().editor.dirty);
}
```

Add these calls to `main()`:

```cpp
test_t9_editor_uses_real_modes_cycles_space_and_backspace();
test_t9_limit_and_screen_guard_preserve_text();
```

- [ ] **Step 4: Run UI tests and verify red**

Expected: the editor text remains empty because T9 actions are not connected.

- [ ] **Step 5: Connect `T9Engine` to the editor**

At every `beginCreate()` or `beginEdit()`:

```cpp
t9_ = T9Engine{};
t9_.toggleLanguage();
t9_.setText(editor_.text);
```

The default `T9Engine` is English lowercase, so exactly one language toggle produces Greek lowercase.

Handle T9 actions only when `screen_ == AgendaUiScreen::EditorText` and no overlay is active. Use these exact branches in the editor-text action switch:

```cpp
case AgendaUiActionType::T9Digit:
    if (
        t9_.pressDigit(
            action.digit,
            action.timestampMilliseconds
        )
    )
    {
        editor_.text = t9_.text();
        editor_.dirty = true;
    }
    break;

case AgendaUiActionType::ToggleT9Language:
    t9_.toggleLanguage();
    break;

case AgendaUiActionType::ToggleT9Mode:
    t9_.toggleMode();
    break;

case AgendaUiActionType::ToggleT9Case:
    t9_.toggleCase();
    break;

case AgendaUiActionType::T9Backspace:
    if (t9_.backspace())
    {
        editor_.text = t9_.text();
        editor_.dirty = true;
    }
    break;

default:
    break;
```

Use the action timestamp directly for `pressDigit()`. Do not synthesize or read wall-clock milliseconds inside the state machine.

Before leaving `EditorText`, call `t9_.commitPending()` and copy `t9_.text()` into the draft.

Set `result.t9Uppercase = t9_.uppercase()` in `view()`.

- [ ] **Step 6: Run T9 and UI suites**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_t9_engine.cpp src\core\utf8.cpp src\core\t9_engine.cpp -o build\test_t9_engine.exe
.\build\test_t9_engine.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
```

Expected: both suites pass with no warnings.

- [ ] **Step 7: Stage and commit**

```powershell
git add src/core/t9_engine.h src/core/t9_engine.cpp src/core/agenda_ui_state_machine.cpp tests/test_t9_engine.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: integrate T9 with agenda UI"
```

---

### Task 6: Transactional Create and Update from Editor

**Files:**
- Modify: `src/core/agenda_ui_state_machine.cpp`
- Modify: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: `AgendaController::createEvent()`, `AgendaController::updateEvent()`, and `Clock::read()`.
- Produces: review/save behavior, domain-error presentation, storage-error retry, exact draft preservation, and successful return to month.

- [ ] **Step 1: Add failing save tests**

Add the helper and all six complete save/error cases:

```cpp
void enterEnglishA(
    AgendaUiStateMachine& ui
)
{
    ui.handle({AgendaUiActionType::ToggleT9Language});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            100
        }
    );
    ui.handle({AgendaUiActionType::EditorNext});
    assert(ui.view().screen == AgendaUiScreen::EditorReview);
}

void test_successful_create_and_update_are_saved()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openCreateTextEditor(ui, Date{2026, 9, 9});
    enterEnglishA(ui);
    ui.handle({AgendaUiActionType::SaveEditor});

    AgendaUiView view = ui.view();
    assert(view.screen == AgendaUiScreen::Month);
    assert(view.overlay == AgendaUiOverlay::None);
    assert(controller.agenda().size() == 1);
    assert(storage.writeCount == 1);

    const std::uint32_t id =
        controller.agenda().events().front().id;

    openUiDay(ui, Date{2026, 9, 9});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            id
        }
    );
    ui.handle({AgendaUiActionType::StartEdit});
    ui.handle({AgendaUiActionType::EditorNextDay});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::SaveEditor});

    view = ui.view();
    assert(view.screen == AgendaUiScreen::Month);
    assert(view.displayedMonth.month == 9);
    assert(controller.agenda().findEvent(id)->eventDate.day == 10);
    assert(storage.writeCount == 2);
}

void test_domain_errors_keep_the_complete_draft()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openCreateTextEditor(ui, Date{2026, 9, 9});
    ui.handle({AgendaUiActionType::EditorNext});

    const AgendaUiEditorDraft before =
        ui.view().editor;

    ui.handle({AgendaUiActionType::SaveEditor});

    const AgendaUiView after = ui.view();
    assert(after.screen == AgendaUiScreen::EditorReview);
    assert(after.overlay == AgendaUiOverlay::None);
    assert(after.eventError == EventError::EmptyText);
    assert(after.editor.date.year == before.date.year);
    assert(after.editor.date.month == before.date.month);
    assert(after.editor.date.day == before.date.day);
    assert(
        after.editor.time.has_value() ==
        before.time.has_value()
    );
    assert(after.editor.text == before.text);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 0);
}

void test_agenda_full_keeps_create_editor_open()
{
    UiTestStorage storage;
    UiTestClock clock;
    std::vector<Event> events;

    for (std::uint32_t id = 1; id <= 500; ++id)
    {
        events.push_back(
            uiStoredEvent(
                id,
                Date{2026, 9, 10},
                "Stored " + std::to_string(id)
            )
        );
    }

    storeUiState(storage, AgendaState{events, 501});

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openCreateTextEditor(ui, Date{2026, 9, 11});
    enterEnglishA(ui);
    ui.handle({AgendaUiActionType::SaveEditor});

    const AgendaUiView view = ui.view();
    assert(view.screen == AgendaUiScreen::EditorReview);
    assert(view.eventError == EventError::AgendaFull);
    assert(view.editor.text == "a");
    assert(controller.agenda().size() == 500);
}

void test_past_event_error_keeps_update_draft()
{
    UiTestStorage storage;
    UiTestClock clock;
    storeUiState(
        storage,
        AgendaState{
            {
                uiStoredEvent(
                    7,
                    Date{2026, 9, 8},
                    "Move me"
                )
            },
            8
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 8});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            7
        }
    );
    ui.handle({AgendaUiActionType::StartEdit});
    ui.handle({AgendaUiActionType::EditorPreviousDay});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::SaveEditor});

    assert(ui.view().screen == AgendaUiScreen::EditorReview);
    assert(ui.view().eventError == EventError::PastEvent);
    assert(ui.view().editor.date.day == 7);
    assert(controller.agenda().findEvent(7)->eventDate.day == 8);
}

void test_storage_and_clock_failures_preserve_draft_and_retry()
{
    UiTestStorage storage;
    UiTestClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openCreateTextEditor(ui, Date{2026, 9, 9});
    enterEnglishA(ui);
    const AgendaUiEditorDraft before = ui.view().editor;

    clock.readResult.error = ClockError::ReadFailed;
    ui.handle({AgendaUiActionType::SaveEditor});
    assert(ui.view().screen == AgendaUiScreen::EditorReview);
    assert(ui.view().overlay == AgendaUiOverlay::ClockError);
    assert(ui.view().clockError == ClockError::ReadFailed);
    assert(ui.view().editor.text == before.text);
    assert(controller.agenda().empty());

    ui.handle({AgendaUiActionType::CancelOperation});
    assert(ui.view().overlay == AgendaUiOverlay::None);

    clock.readResult = ClockReadResult{
        ClockError::None,
        uiTestNow()
    };
    storage.writeError = ByteStorageError::WriteFailed;
    ui.handle({AgendaUiActionType::SaveEditor});

    AgendaUiView failed = ui.view();
    assert(failed.overlay == AgendaUiOverlay::OperationError);
    assert(failed.controllerError == AgendaControllerError::WriteFailed);
    assert(failed.editor.text == before.text);
    assert(controller.agenda().empty());

    ui.handle({AgendaUiActionType::CancelOperation});
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(ui.view().screen == AgendaUiScreen::EditorReview);

    ui.handle({AgendaUiActionType::SaveEditor});
    assert(ui.view().overlay == AgendaUiOverlay::OperationError);

    storage.writeError = ByteStorageError::None;
    ui.handle({AgendaUiActionType::RetryOperation});

    assert(ui.view().screen == AgendaUiScreen::Month);
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(controller.agenda().size() == 1);
    assert(controller.agenda().events().front().text == "a");
}

void test_primary_read_failure_with_backup_preserves_create_draft()
{
    UiTestStorage primary;
    UiTestStorage backup;
    UiTestClock clock;
    AgendaRepository repository(primary, backup);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());

    ui.start();
    openCreateTextEditor(ui, Date{2026, 9, 9});
    enterEnglishA(ui);
    primary.readError = ByteStorageError::ReadFailed;

    ui.handle({AgendaUiActionType::SaveEditor});

    const AgendaUiView view = ui.view();
    assert(view.screen == AgendaUiScreen::EditorReview);
    assert(view.overlay == AgendaUiOverlay::OperationError);
    assert(
        view.controllerError ==
        AgendaControllerError::ReadFailed
    );
    assert(view.editor.text == "a");
    assert(controller.agenda().empty());
    assert(primary.writeCount == 0);
    assert(backup.writeCount == 0);
}
```

Register these six Task 6 tests in `main()`:

```cpp
test_successful_create_and_update_are_saved();
test_domain_errors_keep_the_complete_draft();
test_agenda_full_keeps_create_editor_open();
test_past_event_error_keeps_update_draft();
test_storage_and_clock_failures_preserve_draft_and_retry();
test_primary_read_failure_with_backup_preserves_create_draft();
```

- [ ] **Step 2: Run and verify red**

Expected: Save is ignored and the editor remains on `EditorReview`.

- [ ] **Step 3: Implement create/update Save**

Add the complete save helper. A domain error keeps the review open; a controller error opens a retryable overlay; success clears the editor and returns to the event month:

```cpp
void AgendaUiStateMachine::saveEditor()
{
    if (screen_ != AgendaUiScreen::EditorReview)
    {
        return;
    }

    resetErrors();
    t9_.commitPending();
    editor_.text = t9_.text();

    const ClockReadResult current = clock_.read();

    if (current.error != ClockError::None)
    {
        clockError_ = current.error;
        pendingOperation_ =
            editor_.mode == AgendaUiEditorMode::Create
                ? AgendaUiPendingOperation::CreateEvent
                : AgendaUiPendingOperation::UpdateEvent;
        overlay_ = AgendaUiOverlay::ClockError;
        return;
    }

    if (!validDateTime(current.value))
    {
        clockError_ = ClockError::InvalidDateTime;
        pendingOperation_ =
            editor_.mode == AgendaUiEditorMode::Create
                ? AgendaUiPendingOperation::CreateEvent
                : AgendaUiPendingOperation::UpdateEvent;
        overlay_ = AgendaUiOverlay::ClockError;
        return;
    }

    lastNow_ = current.value;

    const EventDraft draft{
        editor_.date,
        editor_.time,
        editor_.text
    };

    AgendaControllerError controllerError =
        AgendaControllerError::None;
    EventError eventError = EventError::None;

    if (editor_.mode == AgendaUiEditorMode::Create)
    {
        const AgendaControllerCreateResult created =
            controller_.createEvent(
                draft,
                current.value
            );

        controllerError = created.error;
        eventError = created.eventError;
        pendingOperation_ =
            AgendaUiPendingOperation::CreateEvent;
    }
    else
    {
        if (!editor_.eventId.has_value())
        {
            eventError_ = EventError::EventNotFound;
            return;
        }

        const AgendaControllerChangeResult updated =
            controller_.updateEvent(
                *editor_.eventId,
                draft,
                current.value
            );

        controllerError = updated.error;
        eventError = updated.eventError;
        pendingOperation_ =
            AgendaUiPendingOperation::UpdateEvent;
    }

    if (eventError != EventError::None)
    {
        eventError_ = eventError;
        pendingOperation_ =
            AgendaUiPendingOperation::None;
        return;
    }

    if (controllerError != AgendaControllerError::None)
    {
        controllerError_ = controllerError;
        overlay_ = AgendaUiOverlay::OperationError;
        return;
    }

    const Date savedDate = editor_.date;
    editor_ = AgendaUiEditorDraft{};
    pendingOperation_ = AgendaUiPendingOperation::None;
    selectedDate_ = savedDate;
    selectedEventId_.reset();
    displayedMonth_ = YearMonth{
        savedDate.year,
        savedDate.month
    };
    screen_ = AgendaUiScreen::Month;
    overlay_ = AgendaUiOverlay::None;
    resetErrors();
}
```

Route Save and operation-error actions as follows:

```cpp
if (
    overlay_ == AgendaUiOverlay::OperationError ||
    overlay_ == AgendaUiOverlay::ClockError
)
{
    if (action.type == AgendaUiActionType::RetryOperation)
    {
        overlay_ = AgendaUiOverlay::None;
        retryPendingOperation();
    }
    else if (action.type == AgendaUiActionType::CancelOperation)
    {
        overlay_ = AgendaUiOverlay::None;
        pendingOperation_ = AgendaUiPendingOperation::None;
        resetErrors();
    }

    return;
}

if (
    screen_ == AgendaUiScreen::EditorReview &&
    action.type == AgendaUiActionType::SaveEditor
)
{
    saveEditor();
    return;
}

void AgendaUiStateMachine::retryPendingOperation()
{
    if (
        pendingOperation_ == AgendaUiPendingOperation::CreateEvent ||
        pendingOperation_ == AgendaUiPendingOperation::UpdateEvent
    )
    {
        saveEditor();
    }
}
```

The reminder-orchestration task adds the immediate lookup to this success path after `checkReminder()` has a real definition; do not call an undefined helper in this commit.

- [ ] **Step 4: Run controller and UI regressions**

```powershell
.\build\test_agenda_controller.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
```

Expected: both suites pass.

- [ ] **Step 5: Stage and commit**

```powershell
git add src/core/agenda_ui_state_machine.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: save agenda events through UI"
```

---

### Task 7: Confirmed Transactional Deletion

**Files:**
- Modify: `src/core/agenda_ui_state_machine.cpp`
- Modify: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: `AgendaController::deleteEvent()` and current details selection.
- Produces: confirm/cancel deletion, retry after persistence failure, and page clamping after success.

- [ ] **Step 1: Add failing deletion tests**

Add and register these complete cases:

```cpp
void test_confirmed_delete_is_transactional_and_clamps_page()
{
    UiTestStorage storage;
    UiTestClock clock;
    storeUiState(
        storage,
        AgendaState{
            {
                uiStoredEvent(1, Date{2026, 9, 9}, "One"),
                uiStoredEvent(2, Date{2026, 9, 9}, "Two"),
                uiStoredEvent(3, Date{2026, 9, 9}, "Three"),
                uiStoredEvent(4, Date{2026, 9, 9}, "Four")
            },
            5
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 9});
    ui.handle({AgendaUiActionType::NextDayPage});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            4
        }
    );

    const std::size_t writesBefore =
        storage.writeCount;

    ui.handle({AgendaUiActionType::RequestDelete});
    assert(ui.view().overlay == AgendaUiOverlay::ConfirmDelete);
    assert(ui.view().screen == AgendaUiScreen::EventDetails);
    assert(ui.view().selectedEvent->id == 4);
    assert(controller.agenda().size() == 4);
    assert(storage.writeCount == writesBefore);

    ui.handle({AgendaUiActionType::CancelDelete});
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(ui.view().screen == AgendaUiScreen::EventDetails);

    ui.handle({AgendaUiActionType::RequestDelete});
    ui.handle({AgendaUiActionType::ConfirmDelete});

    assert(ui.view().screen == AgendaUiScreen::Day);
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(ui.view().dayPageIndex == 0);
    assert(controller.agenda().findEvent(4) == nullptr);
    assert(controller.agenda().size() == 3);
    assert(storage.writeCount == writesBefore + 1);
}

void test_delete_failure_preserves_details_and_retries_same_id()
{
    UiTestStorage storage;
    UiTestClock clock;
    storeUiState(
        storage,
        AgendaState{
            {
                uiStoredEvent(
                    7,
                    Date{2026, 9, 9},
                    "Keep until saved"
                )
            },
            8
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 9});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            7
        }
    );

    storage.writeError = ByteStorageError::WriteFailed;
    ui.handle({AgendaUiActionType::RequestDelete});
    ui.handle({AgendaUiActionType::ConfirmDelete});

    assert(ui.view().overlay == AgendaUiOverlay::OperationError);
    assert(ui.view().screen == AgendaUiScreen::EventDetails);
    assert(ui.view().selectedEvent->id == 7);
    assert(controller.agenda().findEvent(7) != nullptr);

    storage.writeError = ByteStorageError::None;
    ui.handle({AgendaUiActionType::RetryOperation});

    assert(ui.view().screen == AgendaUiScreen::Day);
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(controller.agenda().findEvent(7) == nullptr);
}

void test_stale_details_selection_returns_to_day()
{
    UiTestStorage storage;
    UiTestClock clock;
    storeUiState(
        storage,
        AgendaState{
            {
                uiStoredEvent(
                    7,
                    Date{2026, 9, 9},
                    "External delete"
                )
            },
            8
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 9});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            7
        }
    );
    assert(
        controller.deleteEvent(7).error ==
        AgendaControllerError::None
    );

    ui.handle({AgendaUiActionType::RequestDelete});
    assert(ui.view().screen == AgendaUiScreen::Day);
    assert(!ui.view().selectedEvent.has_value());
}
```

Add these calls to `main()`:

```cpp
test_confirmed_delete_is_transactional_and_clamps_page();
test_delete_failure_preserves_details_and_retries_same_id();
test_stale_details_selection_returns_to_day();
```

- [ ] **Step 2: Run and verify red**

Expected: `RequestDelete` does not open `ConfirmDelete`.

- [ ] **Step 3: Implement confirmation and deletion**

Insert this overlay routing before the generic `overlay_ != None` return:

```cpp
if (overlay_ == AgendaUiOverlay::ConfirmDelete)
{
    if (action.type == AgendaUiActionType::CancelDelete)
    {
        overlay_ = AgendaUiOverlay::None;
    }
    else if (action.type == AgendaUiActionType::ConfirmDelete)
    {
        overlay_ = AgendaUiOverlay::None;
        confirmDelete();
    }

    return;
}
```

Allow `RequestDelete` only from valid details:

```cpp
if (
    screen_ == AgendaUiScreen::EventDetails &&
    action.type == AgendaUiActionType::RequestDelete
)
{
    if (
        !selectedEventId_.has_value() ||
        controller_.agenda().findEvent(
            *selectedEventId_
        ) == nullptr
    )
    {
        selectedEventId_.reset();
        screen_ = AgendaUiScreen::Day;
        return;
    }

    overlay_ = AgendaUiOverlay::ConfirmDelete;
    return;
}
```

Implement deletion with exact rollback-aware result handling:

```cpp
void AgendaUiStateMachine::confirmDelete()
{
    if (!selectedEventId_.has_value())
    {
        screen_ = AgendaUiScreen::Day;
        return;
    }

    resetErrors();

    const AgendaControllerChangeResult result =
        controller_.deleteEvent(
            *selectedEventId_
        );

    if (result.eventError == EventError::EventNotFound)
    {
        selectedEventId_.reset();
        pendingOperation_ = AgendaUiPendingOperation::None;
        screen_ = AgendaUiScreen::Day;
        clampDayPage();
        return;
    }

    if (result.eventError != EventError::None)
    {
        eventError_ = result.eventError;
        return;
    }

    if (result.error != AgendaControllerError::None)
    {
        controllerError_ = result.error;
        pendingOperation_ =
            AgendaUiPendingOperation::DeleteEvent;
        overlay_ = AgendaUiOverlay::OperationError;
        return;
    }

    selectedEventId_.reset();
    pendingOperation_ = AgendaUiPendingOperation::None;
    overlay_ = AgendaUiOverlay::None;
    screen_ = AgendaUiScreen::Day;
    clampDayPage();
}
```

Replace `retryPendingOperation()` with this extended version. Cancel closes the error and leaves the underlying details screen unchanged:

```cpp
void AgendaUiStateMachine::retryPendingOperation()
{
    if (pendingOperation_ == AgendaUiPendingOperation::DeleteEvent)
    {
        confirmDelete();
        return;
    }

    if (
        pendingOperation_ == AgendaUiPendingOperation::CreateEvent ||
        pendingOperation_ == AgendaUiPendingOperation::UpdateEvent
    )
    {
        saveEditor();
    }
}
```

The reminder-orchestration task adds `checkReminder()` after successful deletion; this commit does not call an undefined method.

- [ ] **Step 4: Run UI and controller suites**

Compile the changed UI target, then run both suites:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
.\build\test_agenda_controller.exe
```

Expected: both pass with no warnings.

- [ ] **Step 5: Stage and commit**

```powershell
git add src/core/agenda_ui_state_machine.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: add confirmed agenda UI deletion"
```

---

### Task 8: Reminder Overlay Orchestration

**Files:**
- Modify: `src/core/agenda_ui_state_machine.cpp`
- Modify: `tests/test_agenda_ui_state_machine.cpp`

**Interfaces:**
- Consumes: `AgendaController::nextReminder()`, `AgendaController::acknowledgeReminder()`, and `Clock::read()`.
- Produces: deferred modal reminders, persisted acknowledgement, immediate one-at-a-time progression, and acknowledgement retry.

- [ ] **Step 1: Add failing reminder tests**

Add this due-event fixture and register the four complete cases:

```cpp
Event uiDueEvent(
    std::uint32_t id,
    const std::string& text
)
{
    return Event{
        id,
        Date{2026, 9, 8},
        std::nullopt,
        text,
        false,
        DateTime{
            Date{2026, 9, 1},
            TimeOfDay{9, 0}
        }
    };
}

DateTime uiReminderNow()
{
    return DateTime{
        Date{2026, 9, 7},
        TimeOfDay{17, 0}
    };
}

void test_tick_shows_owned_due_reminder_on_month()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.value = uiReminderNow();
    storeUiState(
        storage,
        AgendaState{{uiDueEvent(1, "Original")}, 2}
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    ui.tick();

    const AgendaUiView before = ui.view();
    assert(before.screen == AgendaUiScreen::Month);
    assert(before.overlay == AgendaUiOverlay::Reminder);
    assert(before.reminder->id == 1);
    assert(before.reminder->text == "Original");
    assert(storage.writeCount == 0);

    const AgendaControllerChangeResult updated =
        controller.updateEvent(
            1,
            EventDraft{
                Date{2026, 9, 8},
                std::nullopt,
                "Changed"
            },
            uiReminderNow()
        );

    assert(updated.error == AgendaControllerError::None);
    assert(before.reminder->text == "Original");
}

void test_reminder_is_deferred_by_editor_and_blocking_overlay()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.value = uiReminderNow();
    storeUiState(
        storage,
        AgendaState{
            {
                uiDueEvent(1, "Due"),
                uiStoredEvent(2, Date{2026, 9, 9}, "Selected")
            },
            3
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    openUiDay(ui, Date{2026, 9, 9});
    ui.handle({AgendaUiActionType::StartCreate});
    ui.tick();

    assert(ui.view().screen == AgendaUiScreen::EditorDate);
    assert(ui.view().overlay == AgendaUiOverlay::None);

    ui.handle({AgendaUiActionType::EditorCancel});
    assert(ui.view().screen == AgendaUiScreen::Day);
    assert(ui.view().overlay == AgendaUiOverlay::Reminder);

    ui.handle({AgendaUiActionType::AcknowledgeReminder});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectEvent,
            std::nullopt,
            2
        }
    );
    ui.handle({AgendaUiActionType::RequestDelete});
    assert(ui.view().overlay == AgendaUiOverlay::ConfirmDelete);

    ui.tick();
    assert(ui.view().overlay == AgendaUiOverlay::ConfirmDelete);
}

void test_acknowledgement_persists_and_advances_one_at_a_time()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.value = uiReminderNow();
    storeUiState(
        storage,
        AgendaState{
            {
                uiDueEvent(1, "First"),
                uiDueEvent(2, "Second")
            },
            3
        }
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();
    ui.tick();
    assert(ui.view().reminder->id == 1);

    ui.handle({AgendaUiActionType::AcknowledgeReminder});
    assert(controller.agenda().findEvent(1)->reminderSeen);
    assert(ui.view().overlay == AgendaUiOverlay::Reminder);
    assert(ui.view().reminder->id == 2);

    ui.handle({AgendaUiActionType::AcknowledgeReminder});
    assert(controller.agenda().findEvent(2)->reminderSeen);
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(!ui.view().reminder.has_value());
}

void test_acknowledgement_and_clock_failures_are_retryable()
{
    UiTestStorage storage;
    UiTestClock clock;
    clock.readResult.value = uiReminderNow();
    storeUiState(
        storage,
        AgendaState{{uiDueEvent(7, "Retry")}, 8}
    );

    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine ui(controller, clock, uiTestNow());
    ui.start();

    clock.readResult.error = ClockError::ReadFailed;
    ui.tick();
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(ui.view().clockError == ClockError::ReadFailed);

    clock.readResult = ClockReadResult{
        ClockError::None,
        uiReminderNow()
    };
    ui.tick();
    assert(ui.view().reminder->id == 7);

    storage.writeError = ByteStorageError::WriteFailed;
    ui.handle({AgendaUiActionType::AcknowledgeReminder});
    assert(ui.view().overlay == AgendaUiOverlay::Reminder);
    assert(ui.view().reminder->id == 7);
    assert(!controller.agenda().findEvent(7)->reminderSeen);
    assert(
        ui.view().controllerError ==
        AgendaControllerError::WriteFailed
    );

    storage.writeError = ByteStorageError::None;
    ui.handle({AgendaUiActionType::AcknowledgeReminder});
    assert(ui.view().overlay == AgendaUiOverlay::None);
    assert(controller.agenda().findEvent(7)->reminderSeen);
}
```

Add these calls to `main()`:

```cpp
test_tick_shows_owned_due_reminder_on_month();
test_reminder_is_deferred_by_editor_and_blocking_overlay();
test_acknowledgement_persists_and_advances_one_at_a_time();
test_acknowledgement_and_clock_failures_are_retryable();
```

- [ ] **Step 2: Run and verify red**

Expected: `tick()` remains a no-op and no reminder overlay appears.

- [ ] **Step 3: Implement `checkReminder()` and `tick()`**

Add the editor-screen predicate in the anonymous namespace:

```cpp
bool isEditorScreen(
    AgendaUiScreen screen
)
{
    return
        screen == AgendaUiScreen::EditorDate ||
        screen == AgendaUiScreen::EditorTime ||
        screen == AgendaUiScreen::EditorText ||
        screen == AgendaUiScreen::EditorReview;
}
```

Implement the complete lookup helper. It owns no queue and copies at most one event:

```cpp
void AgendaUiStateMachine::checkReminder()
{
    if (
        !controller_.isReady() ||
        overlay_ != AgendaUiOverlay::None ||
        isEditorScreen(screen_)
    )
    {
        return;
    }

    const ClockReadResult current = clock_.read();

    if (current.error != ClockError::None)
    {
        clockError_ = current.error;
        return;
    }

    if (!validDateTime(current.value))
    {
        clockError_ = ClockError::InvalidDateTime;
        return;
    }

    lastNow_ = current.value;
    clockError_ = ClockError::None;

    const AgendaControllerReminderResult result =
        controller_.nextReminder(current.value);

    if (result.error != AgendaControllerError::None)
    {
        controllerError_ = result.error;
        return;
    }

    if (result.reminderError != ReminderEngineError::None)
    {
        clockError_ = ClockError::InvalidDateTime;
        return;
    }

    controllerError_ = AgendaControllerError::None;
    reminder_ = result.event;

    if (reminder_.has_value())
    {
        overlay_ = AgendaUiOverlay::Reminder;
    }
}

void AgendaUiStateMachine::tick()
{
    checkReminder();
}
```

Add `checkReminder();` at the end of the successful create/update path, the successful delete path, and `leaveEditorAfterDiscard()` after changing away from the editor. Both clean cancel and confirmed discard pass through that exit helper, satisfying the immediate post-editor lookup requirement.

- [ ] **Step 4: Implement acknowledgement and retry**

Insert the reminder overlay branch before the generic overlay return. Persistence failure preserves the same copy; success immediately asks the controller for the next due event:

```cpp
if (overlay_ == AgendaUiOverlay::Reminder)
{
    if (
        action.type !=
        AgendaUiActionType::AcknowledgeReminder ||
        !reminder_.has_value()
    )
    {
        return;
    }

    resetErrors();

    const AgendaControllerChangeResult result =
        controller_.acknowledgeReminder(
            reminder_->id
        );

    if (result.error != AgendaControllerError::None)
    {
        controllerError_ = result.error;
        return;
    }

    if (
        result.eventError != EventError::None &&
        result.eventError != EventError::EventNotFound
    )
    {
        eventError_ = result.eventError;
        return;
    }

    reminder_.reset();
    overlay_ = AgendaUiOverlay::None;
    checkReminder();
    return;
}
```

`EventNotFound` is treated as a stale reminder copy: it is dropped and a fresh lookup replaces it if another reminder is due. No UI-owned queue is introduced.

- [ ] **Step 5: Run reminder, controller, and UI suites**

```powershell
.\build\test_reminder_engine.exe
.\build\test_agenda_controller.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_state_machine.exe
```

Expected: all three suites pass with no warnings.

- [ ] **Step 6: Stage and commit**

```powershell
git add src/core/agenda_ui_state_machine.cpp tests/test_agenda_ui_state_machine.cpp
git diff --cached --check
git commit -m "feat: present reminders through agenda UI"
```

---

### Task 9: Real Persistence Integration and Final Regression

**Files:**
- Create: `tests/test_agenda_ui_persistence.cpp`
- Modify only if a real integration defect is found: the smallest responsible production file and its focused unit test.

**Interfaces:**
- Consumes: all UI-core interfaces and `DesktopFileStorage`.
- Produces: end-to-end evidence that a UI-created event and UI acknowledgement survive process-style reopen.

- [ ] **Step 1: Write the desktop persistence test**

Create `tests/test_agenda_ui_persistence.cpp` with this complete content:

```cpp
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>

#include "core/agenda_controller.h"
#include "core/agenda_repository.h"
#include "core/agenda_ui_state_machine.h"
#include "core/clock.h"
#include "platform/desktop_file_storage.h"

namespace
{
const std::filesystem::path TEST_ROOT =
    std::filesystem::path("build") /
    "agenda_ui_persistence_tests";

void resetUiPersistenceFiles()
{
    std::error_code error;
    std::filesystem::remove_all(TEST_ROOT, error);
}

class UiPersistenceClock : public Clock
{
public:
    ClockReadResult current{
        ClockError::None,
        DateTime{
            Date{2026, 9, 7},
            TimeOfDay{10, 0}
        }
    };

    ClockReadResult read() const override
    {
        return current;
    }

    ClockError set(
        const DateTime& value
    ) override
    {
        current = ClockReadResult{
            ClockError::None,
            value
        };
        return ClockError::None;
    }
};

DateTime uiPersistenceDefault()
{
    return DateTime{
        Date{2026, 9, 7},
        TimeOfDay{12, 0}
    };
}

void createAllDayEnglishA(
    AgendaUiStateMachine& ui,
    const Date& date
)
{
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::SelectDate,
            date
        }
    );
    ui.handle({AgendaUiActionType::StartCreate});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::ToggleT9Language});
    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            100
        }
    );
    ui.handle({AgendaUiActionType::EditorNext});
    ui.handle({AgendaUiActionType::SaveEditor});
}
}

void test_ui_created_event_survives_reopen()
{
    resetUiPersistenceFiles();

    const std::filesystem::path primaryPath =
        TEST_ROOT / "agenda.bin";
    const std::filesystem::path backupPath =
        TEST_ROOT / "agenda.bak";
    std::uint32_t eventId = 0;

    {
        DesktopFileStorage primary(primaryPath);
        DesktopFileStorage backup(backupPath);
        AgendaRepository repository(primary, backup);
        AgendaController controller(repository);
        UiPersistenceClock clock;
        AgendaUiStateMachine ui(
            controller,
            clock,
            uiPersistenceDefault()
        );

        ui.start();
        createAllDayEnglishA(
            ui,
            Date{2026, 9, 9}
        );

        assert(ui.view().screen == AgendaUiScreen::Month);
        assert(controller.agenda().size() == 1);
        eventId = controller.agenda().events().front().id;
        assert(std::filesystem::exists(primaryPath));
    }

    {
        DesktopFileStorage primary(primaryPath);
        DesktopFileStorage backup(backupPath);
        AgendaRepository repository(primary, backup);
        AgendaController controller(repository);
        UiPersistenceClock clock;
        AgendaUiStateMachine ui(
            controller,
            clock,
            uiPersistenceDefault()
        );

        ui.start();

        assert(controller.agenda().size() == 1);
        const Event* event =
            controller.agenda().findEvent(eventId);

        assert(event != nullptr);
        assert(event->eventDate.year == 2026);
        assert(event->eventDate.month == 9);
        assert(event->eventDate.day == 9);
        assert(!event->eventTime.has_value());
        assert(event->text == "a");
    }

    resetUiPersistenceFiles();
}

void test_ui_acknowledgement_survives_reopen()
{
    resetUiPersistenceFiles();

    const std::filesystem::path primaryPath =
        TEST_ROOT / "agenda.bin";
    const std::filesystem::path backupPath =
        TEST_ROOT / "agenda.bak";
    std::uint32_t eventId = 0;

    {
        DesktopFileStorage primary(primaryPath);
        DesktopFileStorage backup(backupPath);
        AgendaRepository repository(primary, backup);
        AgendaController controller(repository);
        UiPersistenceClock clock;
        AgendaUiStateMachine ui(
            controller,
            clock,
            uiPersistenceDefault()
        );

        ui.start();
        createAllDayEnglishA(
            ui,
            Date{2026, 9, 8}
        );
        eventId = controller.agenda().events().front().id;

        clock.current.value = DateTime{
            Date{2026, 9, 7},
            TimeOfDay{17, 0}
        };
        ui.tick();
        assert(ui.view().reminder->id == eventId);

        ui.handle({AgendaUiActionType::AcknowledgeReminder});
        assert(ui.view().overlay == AgendaUiOverlay::None);
        assert(controller.agenda().findEvent(eventId)->reminderSeen);
    }

    {
        DesktopFileStorage primary(primaryPath);
        DesktopFileStorage backup(backupPath);
        AgendaRepository repository(primary, backup);
        AgendaController controller(repository);
        UiPersistenceClock clock;
        clock.current.value = DateTime{
            Date{2026, 9, 7},
            TimeOfDay{17, 0}
        };
        AgendaUiStateMachine ui(
            controller,
            clock,
            uiPersistenceDefault()
        );

        ui.start();
        const Event* event =
            controller.agenda().findEvent(eventId);

        assert(event != nullptr);
        assert(event->reminderSeen);

        ui.tick();
        assert(ui.view().overlay == AgendaUiOverlay::None);
        assert(!ui.view().reminder.has_value());
    }

    resetUiPersistenceFiles();
}

int main()
{
    test_ui_created_event_survives_reopen();
    test_ui_acknowledgement_survives_reopen();

    std::cout
        << "All agenda UI persistence tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Compile the integration coverage**

This task adds black-box integration coverage after all production behavior is already green, so its first meaningful execution is expected to pass. If it fails, preserve the failing output, add the smallest focused unit test to the responsible earlier suite, implement only that defect fix, and rerun both tests before continuing.

- [ ] **Step 3: Run the UI persistence suite**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_persistence.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp src\platform\desktop_file_storage.cpp -o build\test_agenda_ui_persistence.exe
.\build\test_agenda_ui_persistence.exe
```

Expected: `All agenda UI persistence tests passed.` with no warnings.

- [ ] **Step 4: Rebuild all 16 test executables**

Rebuild every target so the final evidence is linked against the current sources:

```powershell
New-Item build -ItemType Directory -Force | Out-Null
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
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_repository.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\agenda_state.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp -o build\test_agenda_repository.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_controller.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp -o build\test_agenda_controller.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_reminder_engine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\reminder_engine.cpp -o build\test_reminder_engine.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_persistence.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\platform\desktop_file_storage.cpp -o build\test_agenda_persistence.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_state_machine.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp -o build\test_agenda_ui_state_machine.exe
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_ui_persistence.cpp src\core\date_time.cpp src\core\utf8.cpp src\core\event.cpp src\core\agenda_state.cpp src\core\agenda_manager.cpp src\core\crc32.cpp src\core\event_codec.cpp src\core\agenda_repository.cpp src\core\reminder_engine.cpp src\core\agenda_controller.cpp src\core\calendar_engine.cpp src\core\t9_engine.cpp src\core\agenda_ui_state_machine.cpp src\platform\desktop_file_storage.cpp -o build\test_agenda_ui_persistence.exe
```

Expected: all 16 commands return to the prompt without warnings or errors.

- [ ] **Step 5: Run all 16 test executables**

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
.\build\test_agenda_controller.exe
.\build\test_reminder_engine.exe
.\build\test_agenda_persistence.exe
.\build\test_agenda_ui_state_machine.exe
.\build\test_agenda_ui_persistence.exe
```

Expected: 16 success messages and zero failures.

- [ ] **Step 6: Run final repository checks**

```powershell
git diff --check
git status --short
```

Expected before staging: only `tests/test_agenda_ui_persistence.cpp` is new, unless a test-first integration defect required a separately reviewed fix.

- [ ] **Step 7: Stage and commit the integration coverage**

```powershell
git add tests/test_agenda_ui_persistence.cpp
git diff --cached --check
git commit -m "test: add agenda UI persistence coverage"
git status
git log --oneline -10
```

Expected: clean working tree after the commit.

## Plan Self-Review

| Approved design area | Implemented and verified in |
|---|---|
| Hardware-independent architecture and owned view model | Tasks 1, 3, and 8 |
| Offline clock port, startup, retry, and manual setup | Tasks 1 and 2 |
| Monday-first month grid, adjacent dates, selection, and event dots | Task 2 plus existing `CalendarEngine` regression |
| Three-row day pagination and UTF-8-safe shortened rows | Task 3 |
| Details-first navigation and safe event copies | Task 3 |
| Four-step create/update editor and discard confirmation | Task 4 |
| Supported date boundaries, all-day default, noon default, and five-minute controls | Task 4 |
| Greek lowercase T9 start, language/case/mode, numbers, space, and backspace | Task 5 |
| 500-code-point limit | Task 5 plus existing UTF-8/T9 regressions |
| Transactional create/update with exact draft preservation and retry/cancel | Task 6 |
| Confirmed transactional deletion, rollback, retry, and page clamping | Task 7 |
| Deferred modal reminders and immediate lookup after editor exit | Task 8 |
| One-at-a-time persisted reminder acknowledgement | Tasks 8 and 9 |
| Real primary/backup file reopen | Task 9 |
| No ESP32/LCD/touch/network dependency in UI core | Global constraints and completion gate |

Self-review result:

- Every in-scope section of the approved design maps to at least one concrete production step and automated assertion.
- All code identifiers used by Tasks 2 through 9 are introduced by Task 1 or by the task that first uses them.
- No unfinished instruction markers or declaration-only test stubs remain.
- Renderer, touch, ESP32 clock implementation, LittleFS, device loop, sleep, battery, and hardware QA remain deliberately outside this UI-core plan and retain their approved roadmap order.

## Completion Gate

The UI-core stage is complete only when all of these statements have fresh evidence:

- All 16 test executables pass.
- Every newly compiled target produces no `-Wall -Wextra -pedantic` warnings.
- `git diff --check` is silent.
- `git status --short` is silent after the final commit.
- Month, day, details, editor, delete, clock, persistence, and reminder requirements can each be pointed to in an automated test.
- No production source includes ESP32, LCD, touch, LVGL, Wi-Fi, or NTP headers.

After this gate, start the next approved roadmap stage: renderer contract and 240-by-240 round-screen layouts. The remaining hardware stages continue in the order recorded by the design spec.
