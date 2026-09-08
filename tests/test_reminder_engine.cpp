#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <vector>

#include "core/reminder_engine.h"

namespace
{
Event reminderEvent(
    std::uint32_t id,
    const Date& date,
    const std::optional<TimeOfDay>& time,
    bool seen = false
)
{
    return Event{
        id,
        date,
        time,
        "Reminder event",
        seen,
        DateTime{
            Date{2026, 9, 1},
            TimeOfDay{9, 0}
        }
    };
}

AgendaManager reminderAgenda(
    std::initializer_list<Event> events,
    std::uint32_t nextId = 100
)
{
    AgendaManager agenda;

    const AgendaState state{
        std::vector<Event>(events),
        nextId
    };

    assert(agenda.restoreState(state));
    return agenda;
}

void assertNoReminder(
    const ReminderCheckResult& result
)
{
    assert(
        result.error ==
        ReminderEngineError::None
    );

    assert(!result.eventId.has_value());
}

void test_empty_agenda_has_no_reminder()
{
    const AgendaManager agenda;

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        )
    );
}

void test_reminder_is_not_due_before_1700()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                1,
                Date{2026, 9, 8},
                TimeOfDay{12, 30}
            )
        });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{16, 59}
            }
        )
    );
}

void test_reminder_is_due_exactly_at_1700()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                4,
                Date{2026, 9, 8},
                TimeOfDay{12, 30}
            )
        });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{17, 0}
            }
        );

    assert(
        result.error ==
        ReminderEngineError::None
    );

    assert(result.eventId == 4);
}

void test_late_evening_reminder_is_due()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                5,
                Date{2026, 9, 8},
                std::nullopt
            )
        });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{23, 59}
            }
        );

    assert(
        result.error ==
        ReminderEngineError::None
    );

    assert(result.eventId == 5);
}

void test_missed_timed_reminder_is_due_before_start()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                6,
                Date{2026, 9, 8},
                TimeOfDay{12, 30}
            )
        });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{12, 29}
            }
        );

    assert(
        result.error ==
        ReminderEngineError::None
    );

    assert(result.eventId == 6);
}

void test_timed_event_is_not_due_at_or_after_start()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                7,
                Date{2026, 9, 8},
                TimeOfDay{12, 30}
            )
        });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{12, 30}
            }
        )
    );

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{12, 31}
            }
        )
    );
}

void test_all_day_event_remains_due_on_event_day()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                8,
                Date{2026, 9, 8},
                std::nullopt
            )
        });

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 8},
                TimeOfDay{23, 59}
            }
        );

    assert(
        result.error ==
        ReminderEngineError::None
    );

    assert(result.eventId == 8);
}

void test_all_day_event_is_not_due_next_day()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                9,
                Date{2026, 9, 8},
                std::nullopt
            )
        });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 9},
                TimeOfDay{0, 0}
            }
        )
    );
}

void test_seen_event_is_skipped()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                10,
                Date{2026, 9, 8},
                std::nullopt,
                true
            )
        });

    assertNoReminder(
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{18, 0}
            }
        )
    );
}

void test_first_due_event_uses_agenda_order()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                30,
                Date{2026, 9, 8},
                TimeOfDay{9, 0}
            ),
            reminderEvent(
                20,
                Date{2026, 9, 8},
                std::nullopt
            ),
            reminderEvent(
                10,
                Date{2026, 9, 8},
                TimeOfDay{8, 0}
            )
        }, 31);

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{18, 0}
            }
        );

    assert(
        result.error ==
        ReminderEngineError::None
    );

    assert(result.eventId == 20);
}

void test_month_year_and_leap_boundaries()
{
    const AgendaManager january =
        reminderAgenda({
            reminderEvent(
                40,
                Date{2027, 1, 1},
                std::nullopt
            )
        });

    assert(
        findNextDueReminder(
            january,
            DateTime{
                Date{2026, 12, 31},
                TimeOfDay{17, 0}
            }
        ).eventId == 40
    );

    const AgendaManager leapYear =
        reminderAgenda({
            reminderEvent(
                41,
                Date{2028, 3, 1},
                std::nullopt
            )
        });

    assert(
        findNextDueReminder(
            leapYear,
            DateTime{
                Date{2028, 2, 29},
                TimeOfDay{17, 0}
            }
        ).eventId == 41
    );
}

void test_first_supported_date_uses_numeric_threshold()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                42,
                Date{2000, 1, 1},
                std::nullopt
            )
        }, 43);

    const ReminderCheckResult result =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2000, 1, 1},
                TimeOfDay{0, 0}
            }
        );

    assert(
        result.error ==
        ReminderEngineError::None
    );

    assert(result.eventId == 42);
}

void test_invalid_current_date_or_time_is_rejected()
{
    const AgendaManager agenda =
        reminderAgenda({
            reminderEvent(
                50,
                Date{2026, 9, 8},
                std::nullopt
            )
        });

    const ReminderCheckResult invalidDate =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 2, 30},
                TimeOfDay{17, 0}
            }
        );

    assert(
        invalidDate.error ==
        ReminderEngineError::InvalidDateTime
    );

    assert(!invalidDate.eventId.has_value());

    const ReminderCheckResult invalidTime =
        findNextDueReminder(
            agenda,
            DateTime{
                Date{2026, 9, 7},
                TimeOfDay{24, 0}
            }
        );

    assert(
        invalidTime.error ==
        ReminderEngineError::InvalidDateTime
    );

    assert(!invalidTime.eventId.has_value());
}
}

int main()
{
    test_empty_agenda_has_no_reminder();
    test_reminder_is_not_due_before_1700();
    test_reminder_is_due_exactly_at_1700();
    test_late_evening_reminder_is_due();
    test_missed_timed_reminder_is_due_before_start();
    test_timed_event_is_not_due_at_or_after_start();
    test_all_day_event_remains_due_on_event_day();
    test_all_day_event_is_not_due_next_day();
    test_seen_event_is_skipped();
    test_first_due_event_uses_agenda_order();
    test_month_year_and_leap_boundaries();
    test_first_supported_date_uses_numeric_threshold();
    test_invalid_current_date_or_time_is_rejected();

    std::cout
        << "All reminder engine tests passed.\n";

    return 0;
}