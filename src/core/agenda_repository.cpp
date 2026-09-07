#include "agenda_repository.h"

#include "event_codec.h"

namespace
{
AgendaLoadResult failedLoad(
    AgendaRepositoryError error
)
{
    return AgendaLoadResult{
        error,
        AgendaState{{}, 1},
        false
    };
}

AgendaLoadResult recoverFromBackup(
    ByteStorage& primaryStorage,
    ByteStorage* backupStorage,
    AgendaRepositoryError fallbackError
)
{
    if (backupStorage == nullptr)
    {
        return failedLoad(fallbackError);
    }

    const ByteReadResult backupStored =
        backupStorage->read();

    if (
        backupStored.error ==
        ByteStorageError::NotFound
    )
    {
        return failedLoad(fallbackError);
    }

    if (
        backupStored.error !=
        ByteStorageError::None
    )
    {
        return failedLoad(
            AgendaRepositoryError::ReadFailed
        );
    }

    const EventDecodeResult backupDecoded =
        decodeAgendaState(
            backupStored.bytes
        );

    if (
        backupDecoded.error !=
        EventCodecError::None
    )
    {
        return failedLoad(
            AgendaRepositoryError::InvalidData
        );
    }

    primaryStorage.write(
        backupStored.bytes
    );

    return AgendaLoadResult{
        AgendaRepositoryError::None,
        backupDecoded.state,
        true
    };
}
}

AgendaRepository::AgendaRepository(
    ByteStorage& storage
)
    : storage_(storage),
      backupStorage_(nullptr)
{
}

AgendaRepository::AgendaRepository(
    ByteStorage& storage,
    ByteStorage& backupStorage
)
    : storage_(storage),
      backupStorage_(&backupStorage)
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
        return recoverFromBackup(
            storage_,
            backupStorage_,
            AgendaRepositoryError::None
        );
    }

    if (
        stored.error !=
        ByteStorageError::None
    )
    {
        return recoverFromBackup(
            storage_,
            backupStorage_,
            AgendaRepositoryError::ReadFailed
        );
    }

    const EventDecodeResult decoded =
        decodeAgendaState(stored.bytes);

    if (
        decoded.error !=
        EventCodecError::None
    )
    {
        return recoverFromBackup(
            storage_,
            backupStorage_,
            AgendaRepositoryError::InvalidData
        );
    }

    return AgendaLoadResult{
        AgendaRepositoryError::None,
        decoded.state,
        false
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

    if (backupStorage_ != nullptr)
    {
        const ByteReadResult oldStored =
            storage_.read();

        if (
            oldStored.error ==
            ByteStorageError::None
        )
        {
            const EventDecodeResult oldDecoded =
                decodeAgendaState(
                    oldStored.bytes
                );

            if (
                oldDecoded.error ==
                EventCodecError::None
            )
            {
                const ByteStorageError
                    backupError =
                        backupStorage_->write(
                            oldStored.bytes
                        );

                if (
                    backupError !=
                    ByteStorageError::None
                )
                {
                    return
                        AgendaRepositoryError::
                            WriteFailed;
                }
            }
        }
        else if (
            oldStored.error !=
            ByteStorageError::NotFound
        )
        {
            return AgendaRepositoryError::ReadFailed;
        }
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