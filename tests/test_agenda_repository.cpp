#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>

#include "core/agenda_repository.h"
#include "core/event_codec.h"

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

AgendaState repositoryState(
    const std::string& text,
    std::uint32_t id = 1
)
{
    return AgendaState{
        {
            Event{
                id,
                Date{2026, 9, 10},
                std::optional<TimeOfDay>{},
                text,
                false,
                DateTime{
                    Date{2026, 9, 4},
                    TimeOfDay{10, 0}
                }
            }
        },
        id + 1
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

void test_missing_file_loads_empty_agenda()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(result.state.events.empty());
    assert(result.state.nextId == 1);
}

void test_valid_bytes_load_agenda_state()
{
    const Event event{
        7,
        Date{2026, 9, 8},
        std::optional<TimeOfDay>{
            TimeOfDay{12, 30}
        },
        "Dentist",
        true,
        DateTime{
            Date{2026, 9, 4},
            TimeOfDay{10, 0}
        }
    };

    const AgendaState storedState{
        {event},
        8
    };

    const EventEncodeResult encoded =
        encodeAgendaState(storedState);

    assert(
        encoded.error ==
        EventCodecError::None
    );

    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encoded.bytes
    };

    AgendaRepository repository(storage);

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(result.state.events.size() == 1);
    assert(result.state.nextId == 8);

    const Event& loaded =
        result.state.events.front();

    assert(loaded.id == 7);
    assert(loaded.eventDate.year == 2026);
    assert(loaded.eventDate.month == 9);
    assert(loaded.eventDate.day == 8);
    assert(loaded.eventTime.has_value());
    assert(loaded.eventTime->hour == 12);
    assert(loaded.eventTime->minute == 30);
    assert(loaded.text == "Dentist");
    assert(loaded.reminderSeen);
}

void test_corrupted_bytes_report_invalid_data()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        {0x01, 0x02, 0x03}
    };

    AgendaRepository repository(storage);

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::InvalidData
    );

    assert(result.state.events.empty());
    assert(result.state.nextId == 1);
}

void test_storage_read_failure_is_reported()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    AgendaRepository repository(storage);

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::ReadFailed
    );

    assert(result.state.events.empty());
    assert(result.state.nextId == 1);
}

void test_save_encodes_and_writes_state()
{
    const Event event{
        1,
        Date{2026, 9, 10},
        std::optional<TimeOfDay>{},
        "Meeting",
        false,
        DateTime{
            Date{2026, 9, 4},
            TimeOfDay{10, 0}
        }
    };

    const AgendaState state{
        {event},
        2
    };

    TestByteStorage storage;
    AgendaRepository repository(storage);

    const AgendaRepositoryError error =
        repository.save(state);

    assert(
        error ==
        AgendaRepositoryError::None
    );

    assert(!storage.writtenBytes.empty());

    const EventDecodeResult decoded =
        decodeAgendaState(
            storage.writtenBytes
        );

    assert(
        decoded.error ==
        EventCodecError::None
    );

    assert(decoded.state.events.size() == 1);
    assert(decoded.state.nextId == 2);
    assert(decoded.state.events.front().id == 1);

    assert(
        decoded.state.events.front().text ==
        "Meeting"
    );
}

void test_storage_write_failure_is_reported()
{
    const Event event{
        1,
        Date{2026, 9, 10},
        std::optional<TimeOfDay>{},
        "Meeting",
        false,
        DateTime{
            Date{2026, 9, 4},
            TimeOfDay{10, 0}
        }
    };

    const AgendaState state{
        {event},
        2
    };

    TestByteStorage storage;

    storage.writeResult =
        ByteStorageError::WriteFailed;

    AgendaRepository repository(storage);

    const AgendaRepositoryError error =
        repository.save(state);

    assert(
        error ==
        AgendaRepositoryError::WriteFailed
    );
}

void test_invalid_state_is_not_written()
{
    const Event event{
        1,
        Date{2026, 9, 10},
        std::optional<TimeOfDay>{},
        "Meeting",
        false,
        DateTime{
            Date{2026, 9, 4},
            TimeOfDay{10, 0}
        }
    };

    const AgendaState invalidState{
        {event},
        1
    };

    TestByteStorage storage;

    storage.writeResult =
        ByteStorageError::WriteFailed;

    storage.writtenBytes = {0xAA};

    AgendaRepository repository(storage);

    const AgendaRepositoryError error =
        repository.save(invalidState);

    assert(
        error ==
        AgendaRepositoryError::InvalidData
    );

    assert(
        storage.writtenBytes ==
        std::vector<std::uint8_t>{0xAA}
    );
}

void test_corrupted_primary_recovers_and_repairs()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        {0x01, 0x02}
    };

    TestByteStorage backup;

    const std::vector<std::uint8_t>
        backupBytes =
            encodedBytes(
                repositoryState("Recovered")
            );

    backup.readResult = ByteReadResult{
        ByteStorageError::None,
        backupBytes
    };

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(result.recoveredFromBackup);
    assert(result.state.events.size() == 1);

    assert(
        result.state.events.front().text ==
        "Recovered"
    );

    assert(
        primary.writtenBytes ==
        backupBytes
    );
}

void test_missing_primary_recovers_from_backup()
{
    TestByteStorage primary;
    TestByteStorage backup;

    const std::vector<std::uint8_t>
        backupBytes =
            encodedBytes(
                repositoryState("Backup")
            );

    backup.readResult = ByteReadResult{
        ByteStorageError::None,
        backupBytes
    };

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(result.recoveredFromBackup);
    assert(result.state.events.size() == 1);

    assert(
        result.state.events.front().text ==
        "Backup"
    );

    assert(
        primary.writtenBytes ==
        backupBytes
    );
}

void test_primary_read_failure_recovers_from_backup()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    TestByteStorage backup;

    const std::vector<std::uint8_t>
        backupBytes =
            encodedBytes(
                repositoryState("Recovered")
            );

    backup.readResult = ByteReadResult{
        ByteStorageError::None,
        backupBytes
    };

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(result.recoveredFromBackup);

    assert(
        result.state.events.front().text ==
        "Recovered"
    );

    assert(
        primary.writtenBytes ==
        backupBytes
    );
}

void test_both_missing_load_empty_agenda()
{
    TestByteStorage primary;
    TestByteStorage backup;

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(!result.recoveredFromBackup);
    assert(result.state.events.empty());
    assert(result.state.nextId == 1);
    assert(primary.readCount == 1);
    assert(backup.readCount == 1);
}

void test_valid_primary_does_not_read_backup()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            repositoryState("Primary")
        )
    };

    TestByteStorage backup;

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(!result.recoveredFromBackup);

    assert(
        result.state.events.front().text ==
        "Primary"
    );

    assert(primary.readCount == 1);
    assert(backup.readCount == 0);
}

void test_recovery_survives_primary_repair_failure()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        {0x01, 0x02}
    };

    primary.writeResult =
        ByteStorageError::WriteFailed;

    TestByteStorage backup;

    backup.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            repositoryState("Recovered")
        )
    };

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::None
    );

    assert(result.recoveredFromBackup);

    assert(
        result.state.events.front().text ==
        "Recovered"
    );

    assert(primary.writeCount == 1);
}

void test_save_moves_old_primary_to_backup()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            repositoryState("Old")
        )
    };

    TestByteStorage backup;

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaRepositoryError error =
        repository.save(
            repositoryState("New", 2)
        );

    assert(
        error ==
        AgendaRepositoryError::None
    );

    assert(backup.writeCount == 1);
    assert(primary.writeCount == 1);

    const EventDecodeResult decodedBackup =
        decodeAgendaState(
            backup.writtenBytes
        );

    const EventDecodeResult decodedPrimary =
        decodeAgendaState(
            primary.writtenBytes
        );

    assert(
        decodedBackup.error ==
        EventCodecError::None
    );

    assert(
        decodedPrimary.error ==
        EventCodecError::None
    );

    assert(
        decodedBackup.state.events.front().text ==
        "Old"
    );

    assert(
        decodedPrimary.state.events.front().text ==
        "New"
    );
}

void test_missing_primary_with_invalid_backup_reports_invalid_data()
{
    TestByteStorage primary;
    TestByteStorage backup;

    backup.readResult = ByteReadResult{
        ByteStorageError::None,
        {0x01, 0x02}
    };

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaLoadResult result =
        repository.load();

    assert(
        result.error ==
        AgendaRepositoryError::InvalidData
    );

    assert(!result.recoveredFromBackup);
    assert(result.state.events.empty());
    assert(result.state.nextId == 1);
}

void test_backup_failure_does_not_write_primary()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            repositoryState("Old")
        )
    };

    primary.writtenBytes = {0xAA};

    TestByteStorage backup;

    backup.writeResult =
        ByteStorageError::WriteFailed;

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaRepositoryError error =
        repository.save(
            repositoryState("New", 2)
        );

    assert(
        error ==
        AgendaRepositoryError::WriteFailed
    );

    assert(backup.writeCount == 1);
    assert(primary.writeCount == 0);

    assert(
        primary.writtenBytes ==
        std::vector<std::uint8_t>{0xAA}
    );
}

void test_primary_write_failure_keeps_old_backup()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            repositoryState("Old")
        )
    };

    primary.writeResult =
        ByteStorageError::WriteFailed;

    TestByteStorage backup;

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaRepositoryError error =
        repository.save(
            repositoryState("New", 2)
        );

    assert(
        error ==
        AgendaRepositoryError::WriteFailed
    );

    assert(backup.writeCount == 1);
    assert(primary.writeCount == 1);

    const EventDecodeResult decodedBackup =
        decodeAgendaState(
            backup.writtenBytes
        );

    assert(
        decodedBackup.error ==
        EventCodecError::None
    );

    assert(
        decodedBackup.state.events.front().text ==
        "Old"
    );
}

void test_missing_primary_does_not_replace_backup()
{
    TestByteStorage primary;
    TestByteStorage backup;

    backup.writtenBytes = {0xAA};

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaRepositoryError error =
        repository.save(
            repositoryState("New")
        );

    assert(
        error ==
        AgendaRepositoryError::None
    );

    assert(primary.writeCount == 1);
    assert(backup.writeCount == 0);

    assert(
        backup.writtenBytes ==
        std::vector<std::uint8_t>{0xAA}
    );
}

void test_invalid_primary_does_not_replace_backup()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        {0x01, 0x02}
    };

    TestByteStorage backup;

    backup.writtenBytes = {0xAA};

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaRepositoryError error =
        repository.save(
            repositoryState("New")
        );

    assert(
        error ==
        AgendaRepositoryError::None
    );

    assert(primary.writeCount == 1);
    assert(backup.writeCount == 0);

    assert(
        backup.writtenBytes ==
        std::vector<std::uint8_t>{0xAA}
    );
}

void test_primary_read_failure_stops_save()
{
    TestByteStorage primary;

    primary.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    TestByteStorage backup;

    AgendaRepository repository(
        primary,
        backup
    );

    const AgendaRepositoryError error =
        repository.save(
            repositoryState("New")
        );

    assert(
        error ==
        AgendaRepositoryError::ReadFailed
    );

    assert(primary.writeCount == 0);
    assert(backup.writeCount == 0);
}

int main()
{
    test_missing_file_loads_empty_agenda();
    test_valid_bytes_load_agenda_state();
    test_corrupted_bytes_report_invalid_data();
    test_storage_read_failure_is_reported();
    test_save_encodes_and_writes_state();
    test_storage_write_failure_is_reported();
    test_invalid_state_is_not_written();
    test_corrupted_primary_recovers_and_repairs();
    test_missing_primary_recovers_from_backup();
    test_primary_read_failure_recovers_from_backup();
    test_both_missing_load_empty_agenda();
    test_valid_primary_does_not_read_backup();
    test_recovery_survives_primary_repair_failure();
    test_save_moves_old_primary_to_backup();
    test_missing_primary_with_invalid_backup_reports_invalid_data();
    test_backup_failure_does_not_write_primary();
    test_primary_write_failure_keeps_old_backup();
    test_missing_primary_does_not_replace_backup();
    test_invalid_primary_does_not_replace_backup();
    test_primary_read_failure_stops_save();

    std::cout
        << "All agenda repository tests passed.\n";

    return 0;
}