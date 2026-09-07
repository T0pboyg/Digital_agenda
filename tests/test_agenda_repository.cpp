#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

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

    ByteReadResult read() const override
    {
        return readResult;
    }

    ByteStorageError write(
        const std::vector<std::uint8_t>& bytes
    ) const override
    {
        writtenBytes = bytes;
        return writeResult;
    }
};

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

int main()
{
    test_missing_file_loads_empty_agenda();
    test_valid_bytes_load_agenda_state();
    test_corrupted_bytes_report_invalid_data();
    test_storage_read_failure_is_reported();
    test_save_encodes_and_writes_state();
    test_storage_write_failure_is_reported();
    test_invalid_state_is_not_written();

    std::cout
        << "All agenda repository tests passed.\n";

    return 0;
}