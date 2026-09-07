#include "agenda_repository.h"

#include "event_codec.h"

AgendaRepository::AgendaRepository(
    ByteStorage& storage
)
    : storage_(storage)
{
}

AgendaLoadResult AgendaRepository::load() const
{
    const ByteReadResult stored =
        storage_.read();

    if (
        stored.error ==
        ByteStorageError::NotFound
    )
    {
        return AgendaLoadResult{
            AgendaRepositoryError::None,
            AgendaState{{}, 1}
        };
    }

    if (
        stored.error !=
        ByteStorageError::None
    )
    {
        return AgendaLoadResult{
            AgendaRepositoryError::ReadFailed,
            AgendaState{{}, 1}
        };
    }

    const EventDecodeResult decoded =
        decodeAgendaState(stored.bytes);

    if (
        decoded.error !=
        EventCodecError::None
    )
    {
        return AgendaLoadResult{
            AgendaRepositoryError::InvalidData,
            AgendaState{{}, 1}
        };
    }

    return AgendaLoadResult{
        AgendaRepositoryError::None,
        decoded.state
    };
}

AgendaRepositoryError AgendaRepository::save(
    const AgendaState& state
) const
{
    const EventEncodeResult encoded =
        encodeAgendaState(state);

    if (
        encoded.error !=
        EventCodecError::None
    )
    {
        return AgendaRepositoryError::InvalidData;
    }

    const ByteStorageError storageError =
        storage_.write(encoded.bytes);

    if (
        storageError !=
        ByteStorageError::None
    )
    {
        return AgendaRepositoryError::WriteFailed;
    }

    return AgendaRepositoryError::None;
}