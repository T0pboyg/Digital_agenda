#pragma once

#include <cstdint>
#include <optional>

#include "core/agenda_ui_types.h"
#include "ui_geometry.h"

class AgendaTouchMapper
{
public:
    std::optional<AgendaUiAction> map(
        const AgendaUiView& view,
        UiPoint releasePoint,
        std::uint32_t timestampMilliseconds
    ) const;
};