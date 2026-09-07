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
