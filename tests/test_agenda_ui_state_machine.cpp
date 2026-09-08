#include <cassert>
#include <cstddef>
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