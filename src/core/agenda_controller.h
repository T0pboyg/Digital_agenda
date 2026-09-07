#pragma once

#include <cstdint>

#include "agenda_manager.h"
#include "agenda_repository.h"

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

class AgendaController
{
public:
    explicit AgendaController(
        AgendaRepository& repository
    );

    AgendaStartResult start();

    bool isReady() const;

    const AgendaManager& agenda() const;

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
