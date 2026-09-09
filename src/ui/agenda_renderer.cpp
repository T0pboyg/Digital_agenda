#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "agenda_renderer.h"

namespace
{
constexpr UiRect MONTH_PREVIOUS{
    20,
    8,
    28,
    28
};

constexpr UiRect MONTH_NEXT{
    192,
    8,
    28,
    28
};

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

std::string_view greekMonthName(
    int month
)
{
    constexpr std::array<
        std::string_view,
        12
    > names{
        "ΙΑΝΟΥΑΡΙΟΣ",
        "ΦΕΒΡΟΥΑΡΙΟΣ",
        "ΜΑΡΤΙΟΣ",
        "ΑΠΡΙΛΙΟΣ",
        "ΜΑΙΟΣ",
        "ΙΟΥΝΙΟΣ",
        "ΙΟΥΛΙΟΣ",
        "ΑΥΓΟΥΣΤΟΣ",
        "ΣΕΠΤΕΜΒΡΙΟΣ",
        "ΟΚΤΩΒΡΙΟΣ",
        "ΝΟΕΜΒΡΙΟΣ",
        "ΔΕΚΕΜΒΡΙΟΣ"
    };

    if (
        month < 1 ||
        month > 12
    )
    {
        return "ΑΓΝΩΣΤΟΣ";
    }

    return names[
        static_cast<std::size_t>(
            month - 1
        )
    ];
}

UiRect monthCellBounds(
    std::size_t index
)
{
    const int column =
        static_cast<int>(
            index % 7
        );

    const int row =
        static_cast<int>(
            index / 7
        );

    return UiRect{
        AgendaUiLayout::MonthGrid.x +
            column *
                AgendaUiLayout::
                    MonthCellWidth,
        AgendaUiLayout::MonthGrid.y +
            row *
                AgendaUiLayout::
                    MonthCellHeight,
        AgendaUiLayout::MonthCellWidth,
        AgendaUiLayout::MonthCellHeight
    };
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

void renderMonth(
    const AgendaUiView& view,
    DrawSurface& surface
)
{
    const std::string title =
        std::string(
            greekMonthName(
                view.displayedMonth.month
            )
        ) +
        " " +
        std::to_string(
            view.displayedMonth.year
        );

    surface.drawText(
        AgendaUiLayout::Header,
        title,
        UiTextStyle::Heading,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );

    surface.drawText(
        MONTH_PREVIOUS,
        "<",
        UiTextStyle::Heading,
        view.monthPreviousEnabled
            ? AgendaUiColors::PrimaryText
            : AgendaUiColors::Disabled,
        UiTextAlignment::Center
    );

    surface.drawText(
        MONTH_NEXT,
        ">",
        UiTextStyle::Heading,
        view.monthNextEnabled
            ? AgendaUiColors::PrimaryText
            : AgendaUiColors::Disabled,
        UiTextAlignment::Center
    );

    constexpr std::array<
        std::string_view,
        7
    > weekdays{
        "ΔΕ",
        "ΤΡ",
        "ΤΕ",
        "ΠΕ",
        "ΠΑ",
        "ΣΑ",
        "ΚΥ"
    };

    for (
        std::size_t index = 0;
        index < weekdays.size();
        ++index
    )
    {
        const UiRect bounds{
            AgendaUiLayout::
                MonthWeekdays.x +
                static_cast<int>(index) *
                    AgendaUiLayout::
                        MonthCellWidth,
            AgendaUiLayout::
                MonthWeekdays.y,
            AgendaUiLayout::
                MonthCellWidth,
            AgendaUiLayout::
                MonthWeekdays.height
        };

        surface.drawText(
            bounds,
            weekdays[index],
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center
        );
    }

    for (
        std::size_t index = 0;
        index < view.monthCells.size();
        ++index
    )
    {
        const AgendaUiMonthCell& cell =
            view.monthCells[index];

        if (!cell.date.has_value())
        {
            continue;
        }

        const UiRect bounds =
            monthCellBounds(index);

        const UiPoint center{
            bounds.x +
                bounds.width / 2,
            bounds.y +
                bounds.height / 2
        };

        if (cell.isToday)
        {
            surface.drawCircle(
                center,
                9,
                AgendaUiColors::Accent,
                1
            );
        }

        if (cell.isSelected)
        {
            surface.fillCircle(
                center,
                9,
                AgendaUiColors::Accent
            );
        }

        UiColor numberColor =
            cell.inDisplayedMonth
                ? AgendaUiColors::
                    PrimaryText
                : AgendaUiColors::
                    Disabled;

        if (cell.isSelected)
        {
            numberColor =
                AgendaUiColors::
                    Background;
        }

        surface.drawText(
            bounds,
            std::to_string(
                cell.date->day
            ),
            UiTextStyle::Caption,
            numberColor,
            UiTextAlignment::Center
        );

        if (cell.hasEvents)
        {
            const UiPoint marker{
                center.x,
                bounds.y +
                    bounds.height - 2
            };

            surface.fillCircle(
                marker,
                2,
                cell.isSelected
                    ? AgendaUiColors::
                        Background
                    : AgendaUiColors::
                        Accent
            );
        }
    }
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

        case AgendaUiScreen::Month:
            renderMonth(
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