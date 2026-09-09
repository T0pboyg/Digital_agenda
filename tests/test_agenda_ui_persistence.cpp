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

    std::filesystem::remove_all(
        TEST_ROOT,
        error
    );
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

    ui.handle(
        {AgendaUiActionType::StartCreate}
    );

    ui.handle(
        {AgendaUiActionType::EditorNext}
    );

    ui.handle(
        {AgendaUiActionType::EditorNext}
    );

    ui.handle(
        {
            AgendaUiActionType::
                ToggleT9Language
        }
    );

    ui.handle(
        AgendaUiAction{
            AgendaUiActionType::T9Digit,
            std::nullopt,
            std::nullopt,
            '2',
            100
        }
    );

    ui.handle(
        {AgendaUiActionType::EditorNext}
    );

    ui.handle(
        {AgendaUiActionType::SaveEditor}
    );
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
        DesktopFileStorage primary(
            primaryPath
        );

        DesktopFileStorage backup(
            backupPath
        );

        AgendaRepository repository(
            primary,
            backup
        );

        AgendaController controller(
            repository
        );

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

        assert(
            ui.view().screen ==
            AgendaUiScreen::Month
        );

        assert(
            controller.agenda().size() == 1
        );

        eventId =
            controller.agenda()
                .events()
                .front()
                .id;

        assert(
            std::filesystem::exists(
                primaryPath
            )
        );
    }

    {
        DesktopFileStorage primary(
            primaryPath
        );

        DesktopFileStorage backup(
            backupPath
        );

        AgendaRepository repository(
            primary,
            backup
        );

        AgendaController controller(
            repository
        );

        UiPersistenceClock clock;

        AgendaUiStateMachine ui(
            controller,
            clock,
            uiPersistenceDefault()
        );

        ui.start();

        assert(
            controller.agenda().size() == 1
        );

        const Event* event =
            controller.agenda().findEvent(
                eventId
            );

        assert(event != nullptr);
        assert(event->eventDate.year == 2026);
        assert(event->eventDate.month == 9);
        assert(event->eventDate.day == 9);

        assert(
            !event->eventTime.has_value()
        );

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
        DesktopFileStorage primary(
            primaryPath
        );

        DesktopFileStorage backup(
            backupPath
        );

        AgendaRepository repository(
            primary,
            backup
        );

        AgendaController controller(
            repository
        );

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

        eventId =
            controller.agenda()
                .events()
                .front()
                .id;

        clock.current.value = DateTime{
            Date{2026, 9, 7},
            TimeOfDay{17, 0}
        };

        ui.tick();

        assert(
            ui.view().reminder.has_value()
        );

        assert(
            ui.view().reminder->id ==
            eventId
        );

        ui.handle(
            {
                AgendaUiActionType::
                    AcknowledgeReminder
            }
        );

        assert(
            ui.view().overlay ==
            AgendaUiOverlay::None
        );

        assert(
            controller.agenda()
                .findEvent(eventId)
                ->reminderSeen
        );
    }

    {
        DesktopFileStorage primary(
            primaryPath
        );

        DesktopFileStorage backup(
            backupPath
        );

        AgendaRepository repository(
            primary,
            backup
        );

        AgendaController controller(
            repository
        );

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
            controller.agenda().findEvent(
                eventId
            );

        assert(event != nullptr);
        assert(event->reminderSeen);

        ui.tick();

        assert(
            ui.view().overlay ==
            AgendaUiOverlay::None
        );

        assert(
            !ui.view().reminder.has_value()
        );
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