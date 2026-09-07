#include <cassert>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>

#include "core/agenda_manager.h"
#include "core/agenda_repository.h"
#include "platform/desktop_file_storage.h"

namespace
{
const std::filesystem::path TEST_ROOT =
    std::filesystem::path("build") /
    "agenda_persistence_tests";

void resetTestFiles()
{
    std::error_code error;

    std::filesystem::remove_all(
        TEST_ROOT,
        error
    );
}

DateTime persistenceNow()
{
    return DateTime{
        Date{2026, 9, 7},
        TimeOfDay{10, 0}
    };
}

EventDraft persistenceDraft(
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
}

void test_event_survives_save_and_new_repository()
{
    resetTestFiles();

    const std::filesystem::path primaryPath =
        TEST_ROOT / "agenda.bin";

    const std::filesystem::path backupPath =
        TEST_ROOT / "agenda.bak";

    DesktopFileStorage primary(primaryPath);
    DesktopFileStorage backup(backupPath);

    AgendaRepository repository(
        primary,
        backup
    );

    AgendaManager agenda;

    const CreateEventResult created =
        agenda.createEvent(
            persistenceDraft("Dentist"),
            persistenceNow()
        );

    assert(created.error == EventError::None);

    assert(
        repository.save(agenda.snapshot()) ==
        AgendaRepositoryError::None
    );

    assert(
        std::filesystem::exists(primaryPath)
    );

    DesktopFileStorage reopenedPrimary(
        primaryPath
    );

    DesktopFileStorage reopenedBackup(
        backupPath
    );

    AgendaRepository reopenedRepository(
        reopenedPrimary,
        reopenedBackup
    );

    const AgendaLoadResult loaded =
        reopenedRepository.load();

    assert(
        loaded.error ==
        AgendaRepositoryError::None
    );

    assert(!loaded.recoveredFromBackup);

    AgendaManager restored;

    assert(restored.restoreState(loaded.state));
    assert(restored.size() == 1);

    const Event* event =
        restored.findEvent(created.eventId);

    assert(event != nullptr);
    assert(event->text == "Dentist");

    assert(event->eventDate.year == 2026);
    assert(event->eventDate.month == 9);
    assert(event->eventDate.day == 8);

    assert(event->eventTime.has_value());
    assert(event->eventTime->hour == 12);
    assert(event->eventTime->minute == 30);

    resetTestFiles();
}

void test_corrupted_primary_recovers_from_backup()
{
    resetTestFiles();

    const std::filesystem::path primaryPath =
        TEST_ROOT / "agenda.bin";

    const std::filesystem::path backupPath =
        TEST_ROOT / "agenda.bak";

    DesktopFileStorage primary(primaryPath);
    DesktopFileStorage backup(backupPath);

    AgendaRepository repository(
        primary,
        backup
    );

    AgendaManager agenda;

    const CreateEventResult first =
        agenda.createEvent(
            persistenceDraft("First"),
            persistenceNow()
        );

    assert(first.error == EventError::None);

    assert(
        repository.save(agenda.snapshot()) ==
        AgendaRepositoryError::None
    );

    const CreateEventResult second =
        agenda.createEvent(
            persistenceDraft("Second"),
            persistenceNow()
        );

    assert(second.error == EventError::None);

    assert(
        repository.save(agenda.snapshot()) ==
        AgendaRepositoryError::None
    );

    assert(
        std::filesystem::exists(backupPath)
    );

    assert(
        primary.write({0x00, 0x01, 0x02}) ==
        ByteStorageError::None
    );

    DesktopFileStorage damagedPrimary(
        primaryPath
    );

    DesktopFileStorage savedBackup(
        backupPath
    );

    AgendaRepository recoveryRepository(
        damagedPrimary,
        savedBackup
    );

    const AgendaLoadResult recovered =
        recoveryRepository.load();

    assert(
        recovered.error ==
        AgendaRepositoryError::None
    );

    assert(recovered.recoveredFromBackup);
    assert(recovered.state.events.size() == 1);

    AgendaManager restored;

    assert(restored.restoreState(recovered.state));
    assert(restored.size() == 1);

    assert(restored.findEvent(first.eventId) != nullptr);
    assert(restored.findEvent(second.eventId) == nullptr);

    assert(
        restored.findEvent(first.eventId)->text ==
        "First"
    );

    DesktopFileStorage repairedPrimary(
        primaryPath
    );

    DesktopFileStorage reopenedBackup(
        backupPath
    );

    AgendaRepository repairedRepository(
        repairedPrimary,
        reopenedBackup
    );

    const AgendaLoadResult repaired =
        repairedRepository.load();

    assert(
        repaired.error ==
        AgendaRepositoryError::None
    );

    assert(!repaired.recoveredFromBackup);
    assert(repaired.state.events.size() == 1);
    assert(repaired.state.events[0].id == first.eventId);
    assert(repaired.state.events[0].text == "First");

    resetTestFiles();
}

int main()
{
    test_event_survives_save_and_new_repository();
    test_corrupted_primary_recovers_from_backup();

    std::cout
        << "All agenda persistence tests passed.\n";

    return 0;
}