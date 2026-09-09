#include "agenda_renderer.h"

void AgendaRenderer::render(
    const AgendaUiView& view,
    DrawSurface& surface
) const
{
    surface.clear(
        AgendaUiColors::Background
    );

    surface.drawText(
        AgendaUiLayout::Header,
        view.screen ==
            AgendaUiScreen::StartupError
            ? "ΣΦΑΛΜΑ"
            : "ΗΜΕΡΟΛΟΓΙΟ",
        UiTextStyle::Title,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );
}