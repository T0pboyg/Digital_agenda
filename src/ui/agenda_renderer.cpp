#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "agenda_renderer.h"

namespace
{
std::string formatDate(
    const Date& value
)
{
    std::ostringstream stream;

    stream
        << std::setfill('0')
        << std::setw(2)
        << value.day
        << "/"
        << std::setw(2)
        << value.month
        << "/"
        << std::setw(4)
        << value.year;

    return stream.str();
}

std::string formatTime(
    const TimeOfDay& value
)
{
    std::ostringstream stream;

    stream
        << std::setfill('0')
        << std::setw(2)
        << value.hour
        << ":"
        << std::setw(2)
        << value.minute;

    return stream.str();
}

std::string_view controllerErrorText(
    AgendaControllerError error
)
{
    switch (error)
    {
        case AgendaControllerError::None:
            return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";

        case AgendaControllerError::NotReady:
            return "ΜΗ ΕΤΟΙΜΟ";

        case AgendaControllerError::ReadFailed:
            return "ΑΠΟΤΥΧΙΑ ΑΝΑΓΝΩΣΗΣ";

        case AgendaControllerError::WriteFailed:
            return "ΑΠΟΤΥΧΙΑ ΑΠΟΘΗΚΕΥΣΗΣ";

        case AgendaControllerError::InvalidData:
            return "ΜΗ ΕΓΚΥΡΑ ΔΕΔΟΜΕΝΑ";
    }

    return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";
}

void drawButton(
    DrawSurface& surface,
    const UiRect& bounds,
    std::string_view label,
    UiColor fillColor,
    UiColor textColor
)
{
    surface.fillRoundedRectangle(
        bounds,
        8,
        fillColor
    );

    surface.drawText(
        bounds,
        label,
        UiTextStyle::Body,
        textColor,
        UiTextAlignment::Center
    );
}

void renderStartupError(
    const AgendaUiView& view,
    DrawSurface& surface
)
{
    surface.drawText(
        AgendaUiLayout::Header,
        "ΣΦΑΛΜΑ ΕΚΚΙΝΗΣΗΣ",
        UiTextStyle::Title,
        AgendaUiColors::Error,
        UiTextAlignment::Center
    );

    surface.drawText(
        AgendaUiLayout::Content,
        controllerErrorText(
            view.controllerError
        ),
        UiTextStyle::Body,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );

    drawButton(
        surface,
        AgendaUiLayout::FooterCenter,
        "ΞΑΝΑ",
        AgendaUiColors::Accent,
        AgendaUiColors::Background
    );
}

void renderClockSetup(
    const AgendaUiView& view,
    DrawSurface& surface
)
{
    surface.drawText(
        AgendaUiLayout::Header,
        "ΡΥΘΜΙΣΗ ΩΡΑΣ",
        UiTextStyle::Title,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );

    surface.fillRoundedRectangle(
        AgendaUiLayout::EditorValue,
        10,
        AgendaUiColors::Surface
    );

    surface.drawText(
        AgendaUiLayout::EditorValue,
        formatDate(
            view.clockDraft.date
        ),
        UiTextStyle::Heading,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );

    surface.fillRoundedRectangle(
        AgendaUiLayout::EditorSecondary,
        10,
        AgendaUiColors::Surface
    );

    surface.drawText(
        AgendaUiLayout::EditorSecondary,
        formatTime(
            view.clockDraft.time
        ),
        UiTextStyle::Title,
        AgendaUiColors::Accent,
        UiTextAlignment::Center
    );

    drawButton(
        surface,
        AgendaUiLayout::FooterCenter,
        "ΕΝΤΑΞΕΙ",
        AgendaUiColors::Accent,
        AgendaUiColors::Background
    );
}

void renderSafeFallback(
    DrawSurface& surface
)
{
    surface.drawText(
        AgendaUiLayout::Header,
        "ΗΜΕΡΟΛΟΓΙΟ",
        UiTextStyle::Title,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );
}
}

void AgendaRenderer::render(
    const AgendaUiView& view,
    DrawSurface& surface
) const
{
    surface.clear(
        AgendaUiColors::Background
    );

    switch (view.screen)
    {
        case AgendaUiScreen::ClockSetup:
            renderClockSetup(
                view,
                surface
            );
            break;

        case AgendaUiScreen::StartupError:
            renderStartupError(
                view,
                surface
            );
            break;

        default:
            renderSafeFallback(surface);
            break;
    }
}