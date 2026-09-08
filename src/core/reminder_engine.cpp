#include "reminder_engine.h"

ReminderCheckResult findNextDueReminder(
    const AgendaManager& agenda,
    const DateTime& now
)
{
    if (
        !isValidDate(now.date) ||
        !isValidTime(now.time)
    )
    {
        return ReminderCheckResult{
            ReminderEngineError::InvalidDateTime,
            std::nullopt
        };
    }

    const long long nowMinute =
        serialMinute(now);

    for (const Event& event : agenda.events())
    {
        if (
            event.reminderSeen ||
            isPast(event, now)
        )
        {
            continue;
        }

        const long long reminderMinute =
            (
                static_cast<long long>(
                    serialDay(event.eventDate)
                ) - 1LL
            ) * 1440LL + 17LL * 60LL;

        if (nowMinute >= reminderMinute)
        {
            return ReminderCheckResult{
                ReminderEngineError::None,
                event.id
            };
        }
    }

    return ReminderCheckResult{
        ReminderEngineError::None,
        std::nullopt
    };
}