#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "agenda_renderer.h"
#include "core/utf8.h"

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
    std::ostringstream output;

    output
        << std::setfill('0')
        << std::setw(2)
        << value.day
        << "/"
        << std::setw(2)
        << value.month
        << "/"
        << std::setw(4)
        << value.year;

    return output.str();
}

std::string formatTime(
    const TimeOfDay& value
)
{
    std::ostringstream output;

    output
        << std::setfill('0')
        << std::setw(2)
        << value.hour
        << ":"
        << std::setw(2)
        << value.minute;

    return output.str();
}

std::string controllerErrorText(
    AgendaControllerError error
)
{
    switch (error)
    {
    case AgendaControllerError::ReadFailed:
        return "ΑΠΟΤΥΧΙΑ ΑΝΑΓΝΩΣΗΣ";

    case AgendaControllerError::WriteFailed:
        return "ΑΠΟΤΥΧΙΑ ΑΠΟΘΗΚΕΥΣΗΣ";

    case AgendaControllerError::InvalidData:
        return "ΜΗ ΕΓΚΥΡΑ ΔΕΔΟΜΕΝΑ";

    case AgendaControllerError::NotReady:
        return "ΜΗ ΕΤΟΙΜΟ";

    case AgendaControllerError::None:
        return "";
    }

    return "";
}

std::string greekMonthName(int month)
{
    constexpr std::array<
        std::string_view,
        12
    > MONTH_NAMES{
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
        return "";
    }

    return std::string(
        MONTH_NAMES[
            static_cast<std::size_t>(
                month - 1
            )
        ]
    );
}

UiRect monthCellBounds(
    std::size_t index
)
{
    const int column =
        static_cast<int>(index % 7);

    const int row =
        static_cast<int>(index / 7);

    return UiRect{
        AgendaUiLayout::MonthGrid.x +
            column *
                AgendaUiLayout::MonthCellWidth,
        AgendaUiLayout::MonthGrid.y +
            row *
                AgendaUiLayout::MonthCellHeight,
        AgendaUiLayout::MonthCellWidth,
        AgendaUiLayout::MonthCellHeight
    };
}

UiRect dayRowBounds(
    std::size_t index
)
{
    constexpr std::array<UiRect, 3> rows{
        AgendaUiLayout::DayRow0,
        AgendaUiLayout::DayRow1,
        AgendaUiLayout::DayRow2
    };

    return rows[index];
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
        greekMonthName(
            view.displayedMonth.month
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
    > WEEKDAYS{
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
        index < WEEKDAYS.size();
        ++index
    )
    {
        const UiRect bounds{
            AgendaUiLayout::MonthWeekdays.x +
                static_cast<int>(index) *
                    AgendaUiLayout::MonthCellWidth,
            AgendaUiLayout::MonthWeekdays.y,
            AgendaUiLayout::MonthCellWidth,
            AgendaUiLayout::MonthWeekdays.height
        };

        surface.drawText(
            bounds,
            WEEKDAYS[index],
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

        UiColor textColor =
            cell.inDisplayedMonth
            ? AgendaUiColors::PrimaryText
            : AgendaUiColors::Disabled;

        if (cell.isSelected)
        {
            textColor =
                AgendaUiColors::Background;
        }

        surface.drawText(
            bounds,
            std::to_string(
                cell.date->day
            ),
            UiTextStyle::Caption,
            textColor,
            UiTextAlignment::Center
        );

        if (cell.hasEvents)
        {
            surface.fillCircle(
                UiPoint{
                    center.x,
                    bounds.y +
                        bounds.height - 3
                },
                2,
                AgendaUiColors::Accent
            );
        }
    }
}

void renderDay(
    const AgendaUiView& view,
    DrawSurface& surface
)
{
    if (!view.selectedDate.has_value())
    {
        return;
    }

    surface.drawText(
        AgendaUiLayout::Header,
        formatDate(
            *view.selectedDate
        ),
        UiTextStyle::Heading,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );

    const std::size_t rowCount =
        view.dayEvents.size() <
            AGENDA_UI_EVENTS_PER_PAGE
        ? view.dayEvents.size()
        : AGENDA_UI_EVENTS_PER_PAGE;

    for (
        std::size_t index = 0;
        index < rowCount;
        ++index
    )
    {
        const AgendaUiEventRow& eventRow =
            view.dayEvents[index];

        const UiRect bounds =
            dayRowBounds(index);

        const UiRect timeBounds{
            bounds.x + 8,
            bounds.y,
            58,
            bounds.height
        };

        const UiRect textBounds{
            bounds.x + 70,
            bounds.y,
            bounds.width - 78,
            bounds.height
        };

        surface.fillRoundedRectangle(
            bounds,
            8,
            AgendaUiColors::Surface
        );

        const std::string timeText =
            eventRow.time.has_value()
            ? formatTime(*eventRow.time)
            : "ΟΛΗ ΜΕΡΑ";

        surface.drawText(
            timeBounds,
            timeText,
            UiTextStyle::Caption,
            AgendaUiColors::Accent,
            UiTextAlignment::Left
        );

        surface.drawText(
            textBounds,
            utf8Prefix(
                eventRow.text,
                32
            ),
            UiTextStyle::Body,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Left
        );
    }

    surface.drawText(
        AgendaUiLayout::FooterLeft,
        "<",
        UiTextStyle::Heading,
        view.dayPreviousEnabled
            ? AgendaUiColors::Accent
            : AgendaUiColors::Disabled,
        UiTextAlignment::Center
    );

    if (view.dayPageCount > 0)
    {
        const std::string pageText =
            std::to_string(
                view.dayPageIndex + 1
            ) +
            "/" +
            std::to_string(
                view.dayPageCount
            );

        surface.drawText(
            AgendaUiLayout::FooterCenter,
            pageText,
            UiTextStyle::Body,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center
        );
    }

    surface.drawText(
        AgendaUiLayout::FooterRight,
        ">",
        UiTextStyle::Heading,
        view.dayNextEnabled
            ? AgendaUiColors::Accent
            : AgendaUiColors::Disabled,
        UiTextAlignment::Center
    );
}

void renderEventDetails(
    const AgendaUiView& view,
    DrawSurface& surface
)
{
    surface.drawText(
        AgendaUiLayout::Header,
        "ΓΕΓΟΝΟΣ",
        UiTextStyle::Heading,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );

    if (!view.selectedEvent.has_value())
    {
        surface.drawText(
            AgendaUiLayout::Content,
            "ΤΟ ΓΕΓΟΝΟΣ ΔΕΝ ΒΡΕΘΗΚΕ",
            UiTextStyle::Body,
            AgendaUiColors::Error,
            UiTextAlignment::Center
        );

        return;
    }

    const Event& event =
        *view.selectedEvent;

    surface.drawText(
        AgendaUiLayout::Subheader,
        formatDate(
            event.eventDate
        ),
        UiTextStyle::Body,
        AgendaUiColors::SecondaryText,
        UiTextAlignment::Center
    );

    const UiRect timeBounds{
        28,
        60,
        184,
        26
    };

    const UiRect textBounds{
        28,
        92,
        184,
        84
    };

    const std::string timeText =
        event.eventTime.has_value()
        ? formatTime(*event.eventTime)
        : "ΟΛΗ ΜΕΡΑ";

    surface.drawText(
        timeBounds,
        timeText,
        UiTextStyle::Heading,
        AgendaUiColors::Accent,
        UiTextAlignment::Center
    );

    surface.drawText(
        textBounds,
        event.text,
        UiTextStyle::Body,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );

    surface.drawText(
        AgendaUiLayout::FooterLeft,
        "ΠΙΣΩ",
        UiTextStyle::Caption,
        AgendaUiColors::SecondaryText,
        UiTextAlignment::Center
    );

    surface.drawText(
        AgendaUiLayout::FooterCenter,
        "ΑΛΛΑΓΗ",
        UiTextStyle::Caption,
        AgendaUiColors::Accent,
        UiTextAlignment::Center
    );

    surface.drawText(
        AgendaUiLayout::FooterRight,
        "ΔΙΑΓΡΑΦΗ",
        UiTextStyle::Caption,
        AgendaUiColors::Error,
        UiTextAlignment::Center
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

    case AgendaUiScreen::Month:
        renderMonth(
            view,
            surface
        );
        break;

    case AgendaUiScreen::Day:
        renderDay(
            view,
            surface
        );
        break;

    case AgendaUiScreen::EventDetails:
        renderEventDetails(
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
        renderSafeFallback(
            surface
        );
        break;
    }
}