#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/agenda_manager.h"

DateTime testNow()
{
    return DateTime{
        Date{2026, 9, 4},
        TimeOfDay{10, 0}
    };
}

EventDraft validDraft(const std::string& text)
{
    return EventDraft{
        Date{2026, 9, 5},
        std::optional<TimeOfDay>{
            TimeOfDay{12, 30}
        },
        text
    };
}

void test_new_manager_is_empty()
{
    AgendaManager agenda;

    assert(agenda.empty());
    assert(agenda.size() == 0);
    assert(agenda.events().empty());
    assert(agenda.findEvent(1) == nullptr);
}

void test_create_event_assigns_id_and_fields()
{
    AgendaManager agenda;
    const DateTime now = testNow();
    const EventDraft draft = validDraft("Dentist");

    const CreateEventResult result =
        agenda.createEvent(draft, now);

    assert(result.error == EventError::None);
    assert(result.eventId == 1);
    assert(!agenda.empty());
    assert(agenda.size() == 1);

    const Event* event =
        agenda.findEvent(result.eventId);

    assert(event != nullptr);
    assert(event->id == 1);

    assert(
        serialDay(event->eventDate) ==
        serialDay(draft.eventDate)
    );

    assert(event->eventTime.has_value());

    assert(
        serialMinute(
            DateTime{
                event->eventDate,
                event->eventTime.value()
            }
        ) ==
        serialMinute(
            DateTime{
                draft.eventDate,
                draft.eventTime.value()
            }
        )
    );

    assert(event->text == "Dentist");
    assert(!event->reminderSeen);

    assert(
        serialMinute(event->createdAt) ==
        serialMinute(now)
    );
}

void test_events_are_sorted_by_date()
{
    AgendaManager agenda;
    const DateTime now = testNow();

    agenda.createEvent(
        EventDraft{
            Date{2026, 9, 6},
            std::optional<TimeOfDay>{
                TimeOfDay{12, 0}
            },
            "Later event"
        },
        now
    );

    agenda.createEvent(
        EventDraft{
            Date{2026, 9, 5},
            std::optional<TimeOfDay>{
                TimeOfDay{12, 0}
            },
            "Earlier event"
        },
        now
    );

    const std::vector<Event>& events =
        agenda.events();

    assert(events.size() == 2);
    assert(events[0].id == 2);
    assert(events[1].id == 1);
}

void test_all_day_event_precedes_timed_event()
{
    AgendaManager agenda;
    const DateTime now = testNow();

    const CreateEventResult timed =
        agenda.createEvent(
            EventDraft{
                Date{2026, 9, 5},
                std::optional<TimeOfDay>{
                    TimeOfDay{9, 0}
                },
                "Timed event"
            },
            now
        );

    const CreateEventResult allDay =
        agenda.createEvent(
            EventDraft{
                Date{2026, 9, 5},
                std::nullopt,
                "All-day event"
            },
            now
        );

    const std::vector<Event>& events =
        agenda.events();

    assert(events.size() == 2);
    assert(events[0].id == allDay.eventId);
    assert(events[1].id == timed.eventId);
}

void test_timed_events_are_sorted_by_time()
{
    AgendaManager agenda;
    const DateTime now = testNow();

    const CreateEventResult evening =
        agenda.createEvent(
            EventDraft{
                Date{2026, 9, 5},
                std::optional<TimeOfDay>{
                    TimeOfDay{18, 0}
                },
                "Evening"
            },
            now
        );

    const CreateEventResult morning =
        agenda.createEvent(
            EventDraft{
                Date{2026, 9, 5},
                std::optional<TimeOfDay>{
                    TimeOfDay{9, 0}
                },
                "Morning"
            },
            now
        );

    const std::vector<Event>& events =
        agenda.events();

    assert(events.size() == 2);
    assert(events[0].id == morning.eventId);
    assert(events[1].id == evening.eventId);
}

void test_missing_update_reports_not_found()
{
    AgendaManager agenda;

    const EventError error =
        agenda.updateEvent(
            999,
            validDraft("Missing event"),
            testNow()
        );

    assert(error == EventError::EventNotFound);
    assert(agenda.empty());
}

void test_update_preserves_identity_and_reorders_event()
{
    AgendaManager agenda;
    const DateTime now = testNow();

    const CreateEventResult target =
        agenda.createEvent(
            EventDraft{
                Date{2026, 9, 6},
                std::optional<TimeOfDay>{
                    TimeOfDay{18, 0}
                },
                "Original"
            },
            now
        );

    agenda.createEvent(
        EventDraft{
            Date{2026, 9, 5},
            std::optional<TimeOfDay>{
                TimeOfDay{12, 0}
            },
            "Other"
        },
        now
    );

    const Event* original =
        agenda.findEvent(target.eventId);

    assert(original != nullptr);

    const auto originalCreatedMinute =
        serialMinute(original->createdAt);

    const EventError error =
        agenda.updateEvent(
            target.eventId,
            EventDraft{
                Date{2026, 9, 5},
                std::nullopt,
                "Updated"
            },
            now
        );

    assert(error == EventError::None);

    const Event* updated =
        agenda.findEvent(target.eventId);

    assert(updated != nullptr);
    assert(updated->id == target.eventId);
    assert(updated->text == "Updated");
    assert(!updated->eventTime.has_value());
    assert(!updated->reminderSeen);

    assert(
        serialMinute(updated->createdAt) ==
        originalCreatedMinute
    );

    assert(
        agenda.events().front().id ==
        target.eventId
    );
}

void test_failed_update_is_atomic()
{
    AgendaManager agenda;

    const CreateEventResult created =
        agenda.createEvent(
            validDraft("Original"),
            testNow()
        );

    const Event before =
        *agenda.findEvent(created.eventId);

    EventDraft invalid =
        validDraft("");

    const EventError error =
        agenda.updateEvent(
            created.eventId,
            invalid,
            testNow()
        );

    assert(error == EventError::EmptyText);

    const Event* after =
        agenda.findEvent(created.eventId);

    assert(after != nullptr);
    assert(after->id == before.id);
    assert(after->text == before.text);

    assert(
        serialDay(after->eventDate) ==
        serialDay(before.eventDate)
    );

    assert(
        after->eventTime.has_value() ==
        before.eventTime.has_value()
    );

    assert(
        serialMinute(
            DateTime{
                after->eventDate,
                after->eventTime.value()
            }
        ) ==
        serialMinute(
            DateTime{
                before.eventDate,
                before.eventTime.value()
            }
        )
    );

    assert(
        serialMinute(after->createdAt) ==
        serialMinute(before.createdAt)
    );

    assert(
        after->reminderSeen ==
        before.reminderSeen
    );
}

void test_equal_date_and_time_are_sorted_by_id()
{
    AgendaManager agenda;
    const DateTime now = testNow();

    const CreateEventResult first =
        agenda.createEvent(
            EventDraft{
                Date{2026, 9, 6},
                std::optional<TimeOfDay>{
                    TimeOfDay{9, 0}
                },
                "First"
            },
            now
        );

    const CreateEventResult second =
        agenda.createEvent(
            EventDraft{
                Date{2026, 9, 5},
                std::optional<TimeOfDay>{
                    TimeOfDay{9, 0}
                },
                "Second"
            },
            now
        );

    const EventError error =
        agenda.updateEvent(
            first.eventId,
            EventDraft{
                Date{2026, 9, 5},
                std::optional<TimeOfDay>{
                    TimeOfDay{9, 0}
                },
                "First updated"
            },
            now
        );

    assert(error == EventError::None);

    const std::vector<Event>& events =
        agenda.events();

    assert(events.size() == 2);
    assert(events[0].id == first.eventId);
    assert(events[1].id == second.eventId);
}

void test_delete_missing_event_returns_false()
{
    AgendaManager agenda;

    assert(!agenda.deleteEvent(999));
    assert(agenda.empty());
    assert(agenda.size() == 0);
}

void test_delete_existing_event_removes_only_target()
{
    AgendaManager agenda;

    const CreateEventResult first =
        agenda.createEvent(
            validDraft("First"),
            testNow()
        );

    const CreateEventResult second =
        agenda.createEvent(
            validDraft("Second"),
            testNow()
        );

    assert(agenda.deleteEvent(first.eventId));
    assert(agenda.size() == 1);

    assert(
        agenda.findEvent(first.eventId) ==
        nullptr
    );

    assert(
        agenda.findEvent(second.eventId) !=
        nullptr
    );
}

void test_deleted_id_is_not_reused()
{
    AgendaManager agenda;

    const CreateEventResult first =
        agenda.createEvent(
            validDraft("First"),
            testNow()
        );

    assert(first.eventId == 1);
    assert(agenda.deleteEvent(first.eventId));

    const CreateEventResult second =
        agenda.createEvent(
            validDraft("Second"),
            testNow()
        );

    assert(second.eventId == 2);
    assert(agenda.size() == 1);
    assert(
        agenda.findEvent(second.eventId) !=
        nullptr
    );
}

int main()
{
    test_new_manager_is_empty();
    test_create_event_assigns_id_and_fields();
    test_events_are_sorted_by_date();
    test_all_day_event_precedes_timed_event();
    test_timed_events_are_sorted_by_time();
    test_missing_update_reports_not_found();
    test_update_preserves_identity_and_reorders_event();
    test_failed_update_is_atomic();
    test_equal_date_and_time_are_sorted_by_id();
    test_delete_missing_event_returns_false();
    test_delete_existing_event_removes_only_target();
    test_deleted_id_is_not_reused();

    std::cout
        << "All agenda manager tests passed.\n";

    return 0;
}