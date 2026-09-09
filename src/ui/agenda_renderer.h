#pragma once

#include "core/agenda_ui_types.h"
#include "draw_surface.h"

class AgendaRenderer
{
public:
    void render(
        const AgendaUiView& view,
        DrawSurface& surface
    ) const;
};