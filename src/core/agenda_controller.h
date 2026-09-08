#pragma once

#include <cstdint>
#include <optional>

#include "agenda_manager.h"
#include "agenda_repository.h"
#include "reminder_engine.h"

enum class AgendaControllerError
{
    None,
    NotReady,
    ReadFailed,
    WriteFailed,
    InvalidData
};

struct AgendaStartResult
{
    AgendaControllerError error;
    bool recoveredFromBackup;
};

struct AgendaControllerCreateResult
{
    AgendaControllerError error;
    EventError eventError;
    std::uint32_t eventId;
};

struct AgendaControllerChangeResult
{
    AgendaControllerError error;
    EventError eventError;
};

struct AgendaControllerReminderResult
{
    AgendaControllerError error;
    ReminderEngineError reminderError;
    std::optional<Event> event;
};

class AgendaController
{
public:
    explicit AgendaController(
        AgendaRepository& repository
    );

    AgendaStartResult start();

    bool isReady() const;

    const AgendaManager& agenda() const;

    AgendaControllerReminderResult nextReminder(
        const DateTime& now
    ) const;

    AgendaControllerCreateResult createEvent(
        const EventDraft& draft,
        const DateTime& now
    );

    AgendaControllerChangeResult updateEvent(
        std::uint32_t eventId,
        const EventDraft& draft,
        const DateTime& now
    );

    AgendaControllerChangeResult deleteEvent(
        std::uint32_t eventId
    );

private:
    AgendaManager agenda_;
    AgendaRepository& repository_;
    bool ready_;
};
