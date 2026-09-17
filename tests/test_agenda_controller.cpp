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

AgendaState rollbackControllerState()
{
    return AgendaState{
        {
            Event{
                11,
                Date{2026, 9, 8},
                std::optional<TimeOfDay>{},
                "All-day planning",
                true,
                DateTime{
                    Date{2026, 9, 1},
                    TimeOfDay{8, 15}
                }
            },
            Event{
                7,
                Date{2026, 9, 8},
                std::optional<TimeOfDay>{
                    TimeOfDay{9, 0}
                },
                "Morning appointment",
                false,
                DateTime{
                    Date{2026, 9, 2},
                    TimeOfDay{9, 30}
                }
            },
            Event{
                19,
                Date{2026, 9, 12},
                std::optional<TimeOfDay>{
                    TimeOfDay{16, 45}
                },
                "Late appointment",
                true,
                DateTime{
                    Date{2026, 9, 3},
                    TimeOfDay{11, 50}
                }
            }
        },
        25
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

static_assert(
    std::is_same_v<
        decltype(
            std::declval<
                AgendaController&
            >().agenda()
        ),
        const AgendaManager&
    >,
    "A mutable controller must still expose read-only agenda access"
);

DateTime controllerNow()
{
    return DateTime{
        Date{2026, 9, 7},
        TimeOfDay{10, 0}
    };
}

EventDraft futureDraft(
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

void assertDateEqual(
    const Date& actual,
    const Date& expected
)
{
    assert(actual.year == expected.year);
    assert(actual.month == expected.month);
    assert(actual.day == expected.day);
}

void assertTimeEqual(
    const TimeOfDay& actual,
    const TimeOfDay& expected
)
{
    assert(actual.hour == expected.hour);
    assert(actual.minute == expected.minute);
}

void assertDateTimeEqual(
    const DateTime& actual,
    const DateTime& expected
)
{
    assertDateEqual(actual.date, expected.date);
    assertTimeEqual(actual.time, expected.time);
}

void assertEventEqual(
    const Event& actual,
    const Event& expected
)
{
    assert(actual.id == expected.id);
    assertDateEqual(actual.eventDate, expected.eventDate);

    assert(
        actual.eventTime.has_value() ==
        expected.eventTime.has_value()
    );

    if (actual.eventTime.has_value())
    {
        assertTimeEqual(
            *actual.eventTime,
            *expected.eventTime
        );
    }

    assert(actual.text == expected.text);
    assert(actual.reminderSeen == expected.reminderSeen);
    assertDateTimeEqual(actual.createdAt, expected.createdAt);
}

void assertAgendaStateEqual(
    const AgendaState& actual,
    const AgendaState& expected
)
{
    assert(actual.nextId == expected.nextId);
    assert(actual.events.size() == expected.events.size());

    for (std::size_t index = 0;
         index < actual.events.size();
         ++index)
    {
        assertEventEqual(
            actual.events[index],
            expected.events[index]
        );
    }
}

void test_create_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(result.eventError == EventError::None);
    assert(result.eventId == 0);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 0);
}

void test_mutations_remain_rejected_after_failed_startup()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::ReadFailed
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Rejected creation"),
            controllerNow()
        );

    const AgendaControllerChangeResult updated =
        controller.updateEvent(
            1,
            futureDraft("Rejected update"),
            controllerNow()
        );

    const AgendaControllerChangeResult deleted =
        controller.deleteEvent(1);

    assert(!controller.isReady());
    assert(created.error == AgendaControllerError::NotReady);
    assert(created.eventError == EventError::None);
    assert(created.eventId == 0);
    assert(updated.error == AgendaControllerError::NotReady);
    assert(updated.eventError == EventError::None);
    assert(deleted.error == AgendaControllerError::NotReady);
    assert(deleted.eventError == EventError::None);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 0);
}

void test_failed_restart_revokes_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult saved =
        controller.createEvent(
            futureDraft("Persisted event"),
            controllerNow()
        );

    assert(saved.eventId == 1);

    const AgendaState before =
        controller.agenda().snapshot();

    storage.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    const AgendaStartResult restarted =
        controller.start();

    assert(
        restarted.error ==
        AgendaControllerError::ReadFailed
    );

    assert(!restarted.recoveredFromBackup);
    assert(!controller.isReady());

    const std::size_t writesBeforeMutations =
        storage.writeCount;

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Rejected creation"),
            controllerNow()
        );

    const AgendaControllerChangeResult updated =
        controller.updateEvent(
            saved.eventId,
            futureDraft("Rejected update"),
            controllerNow()
        );

    const AgendaControllerChangeResult deleted =
        controller.deleteEvent(saved.eventId);

    assert(created.error == AgendaControllerError::NotReady);
    assert(created.eventError == EventError::None);
    assert(created.eventId == 0);
    assert(updated.error == AgendaControllerError::NotReady);
    assert(updated.eventError == EventError::None);
    assert(deleted.error == AgendaControllerError::NotReady);
    assert(deleted.eventError == EventError::None);
    assert(storage.writeCount == writesBeforeMutations);

    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
}

void test_successful_creation_is_saved()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::None);
    assert(result.eventId == 1);
    assert(controller.agenda().size() == 1);
    assert(storage.writeCount == 1);

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(decoded.error == EventCodecError::None);
    assert(decoded.state.events.size() == 1);
    assert(decoded.state.events[0].id == 1);
    assert(decoded.state.events[0].text == "Dentist");
    assert(decoded.state.nextId == 2);
}

void test_invalid_creation_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft(""),
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::EmptyText);
    assert(result.eventId == 0);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 0);
}

void test_create_write_failure_rolls_back_and_preserves_id()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(rollbackControllerState())
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const AgendaControllerCreateResult failed =
        controller.createEvent(
            futureDraft("First attempt"),
            controllerNow()
        );

    assert(
        failed.error ==
        AgendaControllerError::WriteFailed
    );

    assert(failed.eventError == EventError::None);
    assert(failed.eventId == 0);
    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );

    storage.writeResult =
        ByteStorageError::None;

    const AgendaControllerCreateResult saved =
        controller.createEvent(
            futureDraft("Second attempt"),
            controllerNow()
        );

    assert(saved.error == AgendaControllerError::None);
    assert(saved.eventError == EventError::None);
    assert(saved.eventId == before.nextId);
    assert(
        controller.agenda().size() ==
        before.events.size() + 1
    );
    assert(
        controller.agenda().snapshot().nextId ==
        before.nextId + 1
    );
}

void test_create_read_failure_rolls_back()
{
    TestByteStorage primary;
    TestByteStorage backup;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(rollbackControllerState())
    };

    AgendaRepository repository(
        primary,
        backup
    );

    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    primary.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    const AgendaControllerCreateResult result =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(
        result.error ==
        AgendaControllerError::ReadFailed
    );

    assert(result.eventError == EventError::None);
    assert(result.eventId == 0);
    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
    assert(primary.writeCount == 0);
    assert(backup.writeCount == 0);
}

void test_update_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            1,
            futureDraft("Updated"),
            controllerNow()
        );

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 0);
}

void test_successful_update_is_saved()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Original"),
            controllerNow()
        );

    assert(created.eventId == 1);
    storage.writeCount = 0;

    const EventDraft updatedDraft{
        Date{2026, 9, 9},
        std::optional<TimeOfDay>{
            TimeOfDay{14, 45}
        },
        "Updated"
    };

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            created.eventId,
            updatedDraft,
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 1);

    const Event* updated =
        controller.agenda().findEvent(created.eventId);

    assert(updated != nullptr);
    assert(updated->text == "Updated");
    assert(updated->eventDate.day == 9);
    assert(updated->eventTime.has_value());
    assert(updated->eventTime->hour == 14);
    assert(updated->eventTime->minute == 45);

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(decoded.error == EventCodecError::None);
    assert(decoded.state.events.size() == 1);
    assert(decoded.state.events[0].text == "Updated");
}

void test_missing_update_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            77,
            futureDraft("Updated"),
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::EventNotFound);
    assert(storage.writeCount == 0);
}

void test_past_update_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Original"),
            controllerNow()
        );

    assert(created.eventId == 1);
    storage.writeCount = 0;

    const EventDraft pastDraft{
        Date{2026, 9, 6},
        std::optional<TimeOfDay>{},
        "Past"
    };

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            created.eventId,
            pastDraft,
            controllerNow()
        );

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::PastEvent);
    assert(storage.writeCount == 0);
    assert(
        controller.agenda()
            .findEvent(created.eventId)
            ->text == "Original"
    );
}

void test_update_write_failure_restores_exact_state()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(rollbackControllerState())
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    assert(before.events.size() == 3);
    assert(before.events[0].id == 11);
    assert(before.events[1].id == 7);
    assert(before.events[2].id == 19);
    assert(before.events[2].reminderSeen);

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const EventDraft changedDraft{
        Date{2026, 9, 7},
        std::optional<TimeOfDay>{
            TimeOfDay{11, 0}
        },
        "Changed"
    };

    const AgendaControllerChangeResult result =
        controller.updateEvent(
            19,
            changedDraft,
            controllerNow()
        );

    assert(
        result.error ==
        AgendaControllerError::WriteFailed
    );

    assert(result.eventError == EventError::None);

    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
}

void test_delete_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerChangeResult result =
        controller.deleteEvent(1);

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 0);
}

void test_successful_delete_is_saved()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerCreateResult created =
        controller.createEvent(
            futureDraft("Dentist"),
            controllerNow()
        );

    assert(created.eventId == 1);
    storage.writeCount = 0;

    const AgendaControllerChangeResult result =
        controller.deleteEvent(created.eventId);

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::None);
    assert(controller.agenda().empty());
    assert(storage.writeCount == 1);

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(decoded.error == EventCodecError::None);
    assert(decoded.state.events.empty());
    assert(decoded.state.nextId == 2);
}

void test_missing_delete_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.deleteEvent(77);

    assert(result.error == AgendaControllerError::None);
    assert(result.eventError == EventError::EventNotFound);
    assert(storage.writeCount == 0);
}

void test_delete_write_failure_restores_exact_state()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(rollbackControllerState())
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const AgendaControllerChangeResult result =
        controller.deleteEvent(7);

    assert(
        result.error ==
        AgendaControllerError::WriteFailed
    );

    assert(result.eventError == EventError::None);

    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
}
}

void test_reminder_lookup_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(
        result.reminderError ==
        ReminderEngineError::None
    );

    assert(!result.event.has_value());
    assert(storage.writeCount == 0);
}

void test_invalid_clock_is_reported_by_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{24, 0}
            }
        );

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(
        result.reminderError ==
        ReminderEngineError::InvalidDateTime
    );

    assert(!result.event.has_value());
    assert(storage.writeCount == 0);
}

void test_due_reminder_returns_safe_copy_without_save()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        7,
                        Date{2026, 9, 8},
                        TimeOfDay{12, 30},
                        "Dentist",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    }
                },
                8
            }
        )
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const std::size_t writesBefore =
        storage.writeCount;

    AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(
        result.reminderError ==
        ReminderEngineError::None
    );

    assert(result.event.has_value());
    assert(result.event->id == 7);
    assert(result.event->text == "Dentist");
    assert(storage.writeCount == writesBefore);

    result.event->text = "Changed copy";

    assert(
        controller.agenda()
            .findEvent(7)
            ->text == "Dentist"
    );
}

void test_controller_reports_no_due_reminder()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult result =
        controller.nextReminder(
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(
        result.reminderError ==
        ReminderEngineError::None
    );

    assert(!result.event.has_value());
    assert(storage.writeCount == 0);
}

void test_acknowledgement_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(1);

    assert(
        result.error ==
        AgendaControllerError::NotReady
    );

    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 0);
}

void test_missing_acknowledgement_does_not_save()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(999);

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(
        result.eventError ==
        EventError::EventNotFound
    );

    assert(storage.writeCount == 0);
}

void test_successful_acknowledgement_is_saved()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        7,
                        Date{2026, 9, 8},
                        std::nullopt,
                        "All day",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    }
                },
                8
            }
        )
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(7);

    assert(
        result.error ==
        AgendaControllerError::None
    );

    assert(result.eventError == EventError::None);
    assert(storage.writeCount == 1);

    assert(
        controller.agenda()
            .findEvent(7)
            ->reminderSeen
    );

    const EventDecodeResult decoded =
        decodeAgendaState(storage.writtenBytes);

    assert(
        decoded.error ==
        EventCodecError::None
    );

    assert(decoded.state.events.size() == 1);
    assert(decoded.state.events[0].reminderSeen);
}

void test_acknowledgement_write_failure_restores_exact_state()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(rollbackControllerState())
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    storage.writeResult =
        ByteStorageError::WriteFailed;

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(7);

    assert(
        result.error ==
        AgendaControllerError::WriteFailed
    );

    assert(result.eventError == EventError::None);

    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
}

void test_acknowledgement_read_failure_restores_state()
{
    TestByteStorage primary;
    TestByteStorage backup;

    primary.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        7,
                        Date{2026, 9, 8},
                        std::nullopt,
                        "All day",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    }
                },
                8
            }
        )
    };

    AgendaRepository repository(
        primary,
        backup
    );

    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const AgendaState before =
        controller.agenda().snapshot();

    primary.readResult = ByteReadResult{
        ByteStorageError::ReadFailed,
        {}
    };

    const AgendaControllerChangeResult result =
        controller.acknowledgeReminder(7);

    assert(
        result.error ==
        AgendaControllerError::ReadFailed
    );

    assert(result.eventError == EventError::None);
    assert(primary.writeCount == 0);
    assert(backup.writeCount == 0);

    assertAgendaStateEqual(
        controller.agenda().snapshot(),
        before
    );
}

void test_due_reminders_advance_one_at_a_time()
{
    TestByteStorage storage;

    storage.readResult = ByteReadResult{
        ByteStorageError::None,
        encodedBytes(
            AgendaState{
                {
                    Event{
                        1,
                        Date{2026, 9, 8},
                        TimeOfDay{9, 0},
                        "Timed",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{10, 0}
                        }
                    },
                    Event{
                        2,
                        Date{2026, 9, 8},
                        std::nullopt,
                        "All day",
                        false,
                        DateTime{
                            Date{2026, 9, 1},
                            TimeOfDay{11, 0}
                        }
                    }
                },
                3
            }
        )
    };

    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None
    );

    const DateTime now{
        Date{2026, 9, 7},
        TimeOfDay{18, 0}
    };

    const AgendaControllerReminderResult first =
        controller.nextReminder(now);

    assert(first.event.has_value());
    assert(first.event->id == 2);

    assert(
        controller
            .acknowledgeReminder(2)
            .error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult second =
        controller.nextReminder(now);

    assert(second.event.has_value());
    assert(second.event->id == 1);

    assert(
        controller
            .acknowledgeReminder(1)
            .error ==
        AgendaControllerError::None
    );

    const AgendaControllerReminderResult finished =
        controller.nextReminder(now);

    assert(
        finished.error ==
        AgendaControllerError::None
    );

    assert(
        finished.reminderError ==
        ReminderEngineError::None
    );

    assert(!finished.event.has_value());
    assert(storage.writeCount == 2);
}

void test_reminder_schedule_requires_ready_controller()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    const AgendaControllerReminderScheduleResult result =
        controller.reminderSchedule(
            DateTime{
                Date{2026, 9, 16},
                TimeOfDay{10, 0}});

    assert(result.error == AgendaControllerError::NotReady);
    assert(result.reminderError == ReminderEngineError::None);
    assert(!result.dueNow);
    assert(!result.scheduledAt.has_value());
}

void test_controller_exposes_reminder_schedule_without_writing()
{
    TestByteStorage storage;
    AgendaRepository repository(storage);
    AgendaController controller(repository);

    assert(
        controller.start().error ==
        AgendaControllerError::None);

    const AgendaControllerReminderScheduleResult result =
        controller.reminderSchedule(
            DateTime{
                Date{2026, 9, 16},
                TimeOfDay{10, 0}});

    assert(result.error == AgendaControllerError::None);
    assert(result.reminderError == ReminderEngineError::None);
    assert(!result.dueNow);
    assert(!result.scheduledAt.has_value());
    assert(storage.writeCount == 0);
}

int main()
{
    test_new_controller_is_not_ready();
    test_first_boot_starts_empty();
    test_existing_state_loads_on_start();
    test_backup_recovery_is_reported();
    test_read_failure_prevents_startup();
    test_invalid_data_prevents_startup();
    test_create_requires_ready_controller();
    test_mutations_remain_rejected_after_failed_startup();
    test_failed_restart_revokes_ready_controller();
    test_successful_creation_is_saved();
    test_invalid_creation_does_not_save();
    test_create_write_failure_rolls_back_and_preserves_id();
    test_create_read_failure_rolls_back();
    test_update_requires_ready_controller();
    test_successful_update_is_saved();
    test_missing_update_does_not_save();
    test_past_update_does_not_save();
    test_update_write_failure_restores_exact_state();
    test_delete_requires_ready_controller();
    test_successful_delete_is_saved();
    test_missing_delete_does_not_save();
    test_delete_write_failure_restores_exact_state();
    test_reminder_lookup_requires_ready_controller();
    test_invalid_clock_is_reported_by_controller();
    test_due_reminder_returns_safe_copy_without_save();
    test_controller_reports_no_due_reminder();
    test_acknowledgement_requires_ready_controller();
    test_missing_acknowledgement_does_not_save();
    test_successful_acknowledgement_is_saved();
    test_acknowledgement_write_failure_restores_exact_state();
    test_acknowledgement_read_failure_restores_state();
    test_due_reminders_advance_one_at_a_time();
    test_reminder_schedule_requires_ready_controller();
    test_controller_exposes_reminder_schedule_without_writing();

    std::cout
        << "All agenda controller tests passed.\n";

    return 0;
}
