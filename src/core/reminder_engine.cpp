#include "reminder_engine.h"

namespace
{
    constexpr int ReminderHour = 17;

    long long reminderMinuteFor(
        const Event& event)
    {
        return (
            static_cast<long long>(
                serialDay(event.eventDate)) - 1LL
        ) * 1440LL + ReminderHour * 60LL;
    }
}

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
            reminderMinuteFor(event);

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

ReminderScheduleResult findNextReminderSchedule(
    const AgendaManager& agenda,
    const DateTime& now
)
{
    if (
        !isValidDate(now.date) ||
        !isValidTime(now.time)
    )
    {
        return ReminderScheduleResult{
            ReminderEngineError::InvalidDateTime,
            false,
            std::nullopt
        };
    }

    const long long nowMinute =
        serialMinute(now);

    std::optional<DateTime> earliest;
    std::optional<long long> earliestMinute;

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
            reminderMinuteFor(event);

        if (nowMinute >= reminderMinute)
        {
            return ReminderScheduleResult{
                ReminderEngineError::None,
                true,
                std::nullopt
            };
        }

        const std::optional<Date> reminderDate =
            previousDate(event.eventDate);

        if (!reminderDate.has_value())
        {
            return ReminderScheduleResult{
                ReminderEngineError::InvalidDateTime,
                false,
                std::nullopt
            };
        }

        if (
            !earliestMinute.has_value() ||
            reminderMinute < *earliestMinute
        )
        {
            earliestMinute = reminderMinute;
            earliest = DateTime{
                *reminderDate,
                TimeOfDay{ReminderHour, 0}
            };
        }
    }

    return ReminderScheduleResult{
        ReminderEngineError::None,
        false,
        earliest
    };
}
