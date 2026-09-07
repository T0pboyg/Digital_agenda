#pragma once

#include "agenda_state.h"
#include "byte_storage.h"

enum class AgendaRepositoryError
{
    None,
    ReadFailed,
    WriteFailed,
    InvalidData
};

struct AgendaLoadResult
{
    AgendaRepositoryError error;
    AgendaState state;
};

class AgendaRepository
{
public:
    explicit AgendaRepository(
        ByteStorage& storage
    );

    AgendaLoadResult load() const;

    AgendaRepositoryError save(
        const AgendaState& state
    ) const;

private:
    ByteStorage& storage_;
};