#include "agenda_controller.h"

#include <cassert>

namespace
{
AgendaControllerError mapRepositoryError(
    AgendaRepositoryError error
)
{
    switch (error)
    {
        case AgendaRepositoryError::None:
            return AgendaControllerError::None;

        case AgendaRepositoryError::ReadFailed:
            return AgendaControllerError::ReadFailed;

        case AgendaRepositoryError::WriteFailed:
            return AgendaControllerError::WriteFailed;

        case AgendaRepositoryError::InvalidData:
            return AgendaControllerError::InvalidData;
    }

    return AgendaControllerError::InvalidData;
}
}

AgendaController::AgendaController(
    AgendaRepository& repository
)
    : repository_(repository),
      ready_(false)
{
}

AgendaStartResult AgendaController::start()
{
    ready_ = false;

    const AgendaLoadResult loaded =
        repository_.load();

    if (
        loaded.error !=
        AgendaRepositoryError::None
    )
    {
        return AgendaStartResult{
            mapRepositoryError(loaded.error),
            false
        };
    }

    if (!agenda_.restoreState(loaded.state))
    {
        return AgendaStartResult{
            AgendaControllerError::InvalidData,
            false
        };
    }

    ready_ = true;

    return AgendaStartResult{
        AgendaControllerError::None,
        loaded.recoveredFromBackup
    };
}

bool AgendaController::isReady() const
{
    return ready_;
}

const AgendaManager& AgendaController::agenda() const
{
    return agenda_;
}

AgendaControllerCreateResult
AgendaController::createEvent(
    const EventDraft& draft,
    const DateTime& now
)
{
    if (!ready_)
    {
        return AgendaControllerCreateResult{
            AgendaControllerError::NotReady,
            EventError::None,
            0
        };
    }

    const AgendaState previousState =
        agenda_.snapshot();

    const CreateEventResult created =
        agenda_.createEvent(draft, now);

    if (created.error != EventError::None)
    {
        return AgendaControllerCreateResult{
            AgendaControllerError::None,
            created.error,
            0
        };
    }

    const AgendaRepositoryError saveError =
        repository_.save(agenda_.snapshot());

    if (saveError != AgendaRepositoryError::None)
    {
        const bool restored =
            agenda_.restoreState(previousState);

        assert(restored);
        (void)restored;

        return AgendaControllerCreateResult{
            mapRepositoryError(saveError),
            EventError::None,
            0
        };
    }

    return AgendaControllerCreateResult{
        AgendaControllerError::None,
        EventError::None,
        created.eventId
    };
}

AgendaControllerChangeResult
AgendaController::updateEvent(
    std::uint32_t eventId,
    const EventDraft& draft,
    const DateTime& now
)
{
    if (!ready_)
    {
        return AgendaControllerChangeResult{
            AgendaControllerError::NotReady,
            EventError::None
        };
    }

    const AgendaState previousState =
        agenda_.snapshot();

    const EventError eventError =
        agenda_.updateEvent(
            eventId,
            draft,
            now
        );

    if (eventError != EventError::None)
    {
        return AgendaControllerChangeResult{
            AgendaControllerError::None,
            eventError
        };
    }

    const AgendaRepositoryError saveError =
        repository_.save(agenda_.snapshot());

    if (saveError != AgendaRepositoryError::None)
    {
        const bool restored =
            agenda_.restoreState(previousState);

        assert(restored);
        (void)restored;

        return AgendaControllerChangeResult{
            mapRepositoryError(saveError),
            EventError::None
        };
    }

    return AgendaControllerChangeResult{
        AgendaControllerError::None,
        EventError::None
    };
}

AgendaControllerChangeResult
AgendaController::deleteEvent(
    std::uint32_t eventId
)
{
    if (!ready_)
    {
        return AgendaControllerChangeResult{
            AgendaControllerError::NotReady,
            EventError::None
        };
    }

    const AgendaState previousState =
        agenda_.snapshot();

    if (!agenda_.deleteEvent(eventId))
    {
        return AgendaControllerChangeResult{
            AgendaControllerError::None,
            EventError::EventNotFound
        };
    }

    const AgendaRepositoryError saveError =
        repository_.save(agenda_.snapshot());

    if (saveError != AgendaRepositoryError::None)
    {
        const bool restored =
            agenda_.restoreState(previousState);

        assert(restored);
        (void)restored;

        return AgendaControllerChangeResult{
            mapRepositoryError(saveError),
            EventError::None
        };
    }

    return AgendaControllerChangeResult{
        AgendaControllerError::None,
        EventError::None
    };
}
