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

void test_clock_setup_success_opens_selected_month()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error =
        ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        uiTestNow()
    );

    ui.start();

    ui.handle({
        AgendaUiActionType::ClockNextDay
    });

    ui.handle({
        AgendaUiActionType::ConfirmClock
    });

    const AgendaUiView view = ui.view();

    assert(clock.setCount == 1);
    assert(view.screen == AgendaUiScreen::Month);
    assert(view.clockError == ClockError::None);
    assert(view.displayedMonth.year == 2026);
    assert(view.displayedMonth.month == 9);
}

void test_clock_next_day_crosses_month_boundary()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error =
        ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        DateTime{
            Date{2026, 9, 30},
            TimeOfDay{10, 0}
        }
    );

    ui.start();

    ui.handle({
        AgendaUiActionType::ClockNextDay
    });

    const AgendaUiView view = ui.view();

    assert(view.clockDraft.date.year == 2026);
    assert(view.clockDraft.date.month == 10);
    assert(view.clockDraft.date.day == 1);
}

void test_clock_next_day_stops_at_supported_maximum()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error =
        ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        DateTime{
            Date{2099, 12, 31},
            TimeOfDay{10, 0}
        }
    );

    ui.start();

    ui.handle({
        AgendaUiActionType::ClockNextDay
    });

    const AgendaUiView view = ui.view();

    assert(view.clockDraft.date.year == 2099);
    assert(view.clockDraft.date.month == 12);
    assert(view.clockDraft.date.day == 31);
}

void test_clock_previous_day_crosses_month_and_stops_at_minimum()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error =
        ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        DateTime{
            Date{2026, 3, 1},
            TimeOfDay{10, 0}
        }
    );

    ui.start();

    ui.handle({
        AgendaUiActionType::ClockPreviousDay
    });

    AgendaUiView view = ui.view();

    assert(view.clockDraft.date.year == 2026);
    assert(view.clockDraft.date.month == 2);
    assert(view.clockDraft.date.day == 28);

    UiTestStorage minimumStorage;
    UiTestClock minimumClock;

    minimumClock.readResult.error =
        ClockError::Unavailable;

    AgendaRepository minimumRepository(
        minimumStorage
    );

    AgendaController minimumController(
        minimumRepository
    );

    AgendaUiStateMachine minimumUi(
        minimumController,
        minimumClock,
        DateTime{
            Date{2000, 1, 1},
            TimeOfDay{10, 0}
        }
    );

    minimumUi.start();

    minimumUi.handle({
        AgendaUiActionType::ClockPreviousDay
    });

    view = minimumUi.view();

    assert(view.clockDraft.date.year == 2000);
    assert(view.clockDraft.date.month == 1);
    assert(view.clockDraft.date.day == 1);
}

void test_clock_hour_and_minute_controls_wrap()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error =
        ClockError::Unavailable;

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        DateTime{
            Date{2026, 9, 8},
            TimeOfDay{0, 0}
        }
    );

    ui.start();

    ui.handle({
        AgendaUiActionType::ClockPreviousHour
    });

    assert(ui.view().clockDraft.time.hour == 23);

    ui.handle({
        AgendaUiActionType::ClockNextHour
    });

    assert(ui.view().clockDraft.time.hour == 0);

    ui.handle({
        AgendaUiActionType::ClockPreviousMinute
    });

    assert(ui.view().clockDraft.time.minute == 59);

    ui.handle({
        AgendaUiActionType::ClockNextMinute
    });

    assert(ui.view().clockDraft.time.minute == 0);

    ui.handle({
        AgendaUiActionType::ClockNextMinute
    });

    assert(ui.view().clockDraft.time.minute == 1);
}

void test_month_previous_and_next_navigation()
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

    ui.handle({
        AgendaUiActionType::PreviousMonth
    });

    AgendaUiView view = ui.view();

    assert(view.screen == AgendaUiScreen::Month);
    assert(view.displayedMonth.year == 2026);
    assert(view.displayedMonth.month == 8);

    ui.handle({
        AgendaUiActionType::NextMonth
    });

    view = ui.view();

    assert(view.displayedMonth.year == 2026);
    assert(view.displayedMonth.month == 9);
}

void test_month_navigation_exposes_boundary_flags()
{
    UiTestStorage currentStorage;
    UiTestClock currentClock;

    AgendaRepository currentRepository(
        currentStorage
    );

    AgendaController currentController(
        currentRepository
    );

    AgendaUiStateMachine currentUi(
        currentController,
        currentClock,
        uiTestNow()
    );

    currentUi.start();

    assert(
        currentUi.view().monthPreviousEnabled
    );

    assert(
        currentUi.view().monthNextEnabled
    );

    UiTestStorage minimumStorage;
    UiTestClock minimumClock;

    minimumClock.readResult.value =
        DateTime{
            Date{2000, 1, 1},
            TimeOfDay{10, 0}
        };

    AgendaRepository minimumRepository(
        minimumStorage
    );

    AgendaController minimumController(
        minimumRepository
    );

    AgendaUiStateMachine minimumUi(
        minimumController,
        minimumClock,
        minimumClock.readResult.value
    );

    minimumUi.start();

    assert(
        !minimumUi.view().monthPreviousEnabled
    );

    assert(
        minimumUi.view().monthNextEnabled
    );

    UiTestStorage maximumStorage;
    UiTestClock maximumClock;

    maximumClock.readResult.value =
        DateTime{
            Date{2099, 12, 31},
            TimeOfDay{10, 0}
        };

    AgendaRepository maximumRepository(
        maximumStorage
    );

    AgendaController maximumController(
        maximumRepository
    );

    AgendaUiStateMachine maximumUi(
        maximumController,
        maximumClock,
        maximumClock.readResult.value
    );

    maximumUi.start();

    assert(
        maximumUi.view().monthPreviousEnabled
    );

    assert(
        !maximumUi.view().monthNextEnabled
    );
}

void test_selecting_neighboring_date_opens_its_day()
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

    AgendaUiAction previousMonthAction{
        AgendaUiActionType::PreviousMonth
    };

    ui.handle(previousMonthAction);

    AgendaUiAction selectDateAction{
        AgendaUiActionType::SelectDate
    };

    selectDateAction.date =
        Date{2026, 9, 1};

    ui.handle(selectDateAction);

    const AgendaUiView view = ui.view();

    assert(
        view.screen ==
        AgendaUiScreen::Day
    );

    assert(view.displayedMonth.year == 2026);
    assert(view.displayedMonth.month == 9);

    assert(view.selectedDate.has_value());

    assert(
        view.selectedDate->year == 2026
    );

    assert(
        view.selectedDate->month == 9
    );

    assert(
        view.selectedDate->day == 1
    );

    assert(view.dayPageIndex == 0);
}

void test_month_view_builds_calendar_cells()
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

    assert(view.monthCells.size() == 42);

    const AgendaUiMonthCell& first =
        view.monthCells[0];

    assert(first.date.has_value());
    assert(first.date->year == 2026);
    assert(first.date->month == 8);
    assert(first.date->day == 31);
    assert(!first.inDisplayedMonth);

    const AgendaUiMonthCell& second =
        view.monthCells[1];

    assert(second.date.has_value());
    assert(second.date->year == 2026);
    assert(second.date->month == 9);
    assert(second.date->day == 1);
    assert(second.inDisplayedMonth);

    const AgendaUiMonthCell& last =
        view.monthCells[41];

    assert(last.date.has_value());
    assert(last.date->year == 2026);
    assert(last.date->month == 10);
    assert(last.date->day == 11);
    assert(!last.inDisplayedMonth);
}

void test_month_cells_mark_only_current_day()
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

    const AgendaUiMonthCell& today =
        view.monthCells[8];

    assert(today.date.has_value());
    assert(today.date->year == 2026);
    assert(today.date->month == 9);
    assert(today.date->day == 8);

    assert(today.isToday);
    assert(!view.monthCells[7].isToday);
    assert(!view.monthCells[9].isToday);
}

void test_month_cells_mark_only_selected_date()
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

    AgendaUiAction selectDateAction{
        AgendaUiActionType::SelectDate
    };

    selectDateAction.date =
        Date{2026, 9, 10};

    ui.handle(selectDateAction);

    const AgendaUiView view = ui.view();

    const AgendaUiMonthCell& selected =
        view.monthCells[10];

    assert(selected.date.has_value());
    assert(selected.date->year == 2026);
    assert(selected.date->month == 9);
    assert(selected.date->day == 10);

    assert(selected.isSelected);
    assert(!view.monthCells[9].isSelected);
    assert(!view.monthCells[11].isSelected);
}

void test_month_cells_mark_only_dates_with_events()
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

    const AgendaControllerCreateResult created =
        controller.createEvent(
            EventDraft{
                Date{2026, 9, 10},
                std::nullopt,
                "Marked"
            },
            uiTestNow()
        );

    assert(
        created.error ==
        AgendaControllerError::None
    );

    assert(
        created.eventError ==
        EventError::None
    );

    const AgendaUiView view = ui.view();

    const AgendaUiMonthCell& eventDate =
        view.monthCells[10];

    assert(eventDate.date.has_value());
    assert(eventDate.date->year == 2026);
    assert(eventDate.date->month == 9);
    assert(eventDate.date->day == 10);

    assert(eventDate.hasEvents);
    assert(!view.monthCells[9].hasEvents);
    assert(!view.monthCells[11].hasEvents);
}

void test_invalid_selected_date_is_ignored()
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

    AgendaUiAction action{
        AgendaUiActionType::SelectDate
    };

    action.date =
        Date{2026, 2, 30};

    ui.handle(action);

    const AgendaUiView view = ui.view();

    assert(
        view.screen ==
        AgendaUiScreen::Month
    );

    assert(view.displayedMonth.year == 2026);
    assert(view.displayedMonth.month == 9);
    assert(!view.selectedDate.has_value());
    assert(view.dayPageIndex == 0);
    assert(storage.writeCount == 0);
}

void test_clock_write_failure_keeps_manual_setup_open()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error =
        ClockError::Unavailable;

    clock.setResult =
        ClockError::WriteFailed;

    AgendaRepository repository(storage);

    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        uiTestNow()
    );

    ui.start();

    AgendaUiAction confirmAction{
        AgendaUiActionType::ConfirmClock
    };

    ui.handle(confirmAction);

    const AgendaUiView view = ui.view();

    assert(clock.setCount == 1);

    assert(
        view.screen ==
        AgendaUiScreen::ClockSetup
    );

    assert(
        view.clockError ==
        ClockError::WriteFailed
    );
}

void test_invalid_manual_clock_draft_is_not_written()
{
    UiTestStorage storage;
    UiTestClock clock;

    clock.readResult.error =
        ClockError::Unavailable;

    const DateTime invalidDefault{
        Date{2026, 2, 30},
        TimeOfDay{10, 0}
    };

    AgendaRepository repository(storage);

    AgendaController controller(repository);

    AgendaUiStateMachine ui(
        controller,
        clock,
        invalidDefault
    );

    ui.start();

    AgendaUiAction confirmAction{
        AgendaUiActionType::ConfirmClock
    };

    ui.handle(confirmAction);

    const AgendaUiView view = ui.view();

    assert(clock.setCount == 0);

    assert(
        view.screen ==
        AgendaUiScreen::ClockSetup
    );

    assert(
        view.clockError ==
        ClockError::InvalidDateTime
    );
}

void test_irrelevant_month_action_has_no_side_effect()
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

    const AgendaUiView before = ui.view();

    AgendaUiAction irrelevantAction{
        AgendaUiActionType::NextDayPage
    };

    ui.handle(irrelevantAction);

    const AgendaUiView after = ui.view();

    assert(after.screen == before.screen);

    assert(
        after.displayedMonth.year ==
        before.displayedMonth.year
    );

    assert(
        after.displayedMonth.month ==
        before.displayedMonth.month
    );

    assert(
        after.selectedDate.has_value() ==
        before.selectedDate.has_value()
    );

    assert(after.dayPageIndex == before.dayPageIndex);
    assert(storage.writeCount == 0);
}

int main()
{
    test_valid_startup_opens_current_month();
    test_unavailable_clock_opens_manual_setup();
    test_invalid_or_failed_clock_read_opens_manual_setup();
    test_agenda_load_failure_opens_startup_error();
    test_startup_retry_loads_after_storage_recovers();
    test_clock_setup_success_opens_selected_month();
    test_clock_next_day_crosses_month_boundary();
    test_clock_next_day_stops_at_supported_maximum();
    test_clock_previous_day_crosses_month_and_stops_at_minimum();
    test_clock_hour_and_minute_controls_wrap();
    test_month_previous_and_next_navigation();
    test_month_navigation_exposes_boundary_flags();
    test_selecting_neighboring_date_opens_its_day();
    test_month_view_builds_calendar_cells();
    test_month_cells_mark_only_current_day();
    test_month_cells_mark_only_selected_date();
    test_month_cells_mark_only_dates_with_events();
    test_invalid_selected_date_is_ignored();
    test_clock_write_failure_keeps_manual_setup_open();
    test_invalid_manual_clock_draft_is_not_written();
    test_irrelevant_month_action_has_no_side_effect();

    std::cout
        << "All agenda UI state machine tests passed.\n";

    return 0;
}