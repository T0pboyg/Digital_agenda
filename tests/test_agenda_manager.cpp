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

int main()
{
    test_new_manager_is_empty();
    test_create_event_assigns_id_and_fields();
    test_events_are_sorted_by_date();
    test_all_day_event_precedes_timed_event();
    test_timed_events_are_sorted_by_time();

    std::cout
        << "All agenda manager tests passed.\n";

    return 0;
}