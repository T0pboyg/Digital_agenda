#include <cassert>
#include <iostream>
#include <optional>
#include <string>

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

int main()
{
    test_new_manager_is_empty();
    test_create_event_assigns_id_and_fields();

    std::cout
        << "All agenda manager tests passed.\n";

    return 0;
}