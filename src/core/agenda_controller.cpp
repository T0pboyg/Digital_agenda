#include "agenda_controller.h"

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
