#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "agenda_renderer.h"
#include "core/utf8.h"

namespace
{
    std::string formatDate(
        const Date &value)
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
        const TimeOfDay &value)
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
        AgendaControllerError error)
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
            return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";
        }

        return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";
    }

    std::string eventErrorText(
        EventError error)
    {
        switch (error)
        {
        case EventError::InvalidDate:
            return "ΜΗ ΕΓΚΥΡΗ ΗΜΕΡΟΜΗΝΙΑ";

        case EventError::InvalidTime:
            return "ΜΗ ΕΓΚΥΡΗ ΩΡΑ";

        case EventError::EmptyText:
            return "ΚΕΝΟ ΚΕΙΜΕΝΟ";

        case EventError::TextTooLong:
            return "ΠΟΛΥ ΜΕΓΑΛΟ ΚΕΙΜΕΝΟ";

        case EventError::PastEvent:
            return "ΠΑΡΕΛΘΟΝ ΓΕΓΟΝΟΣ";

        case EventError::EventNotFound:
            return "ΤΟ ΓΕΓΟΝΟΣ ΔΕΝ ΒΡΕΘΗΚΕ";

        case EventError::AgendaFull:
            return "ΤΟ ΗΜΕΡΟΛΟΓΙΟ ΕΙΝΑΙ ΠΛΗΡΕΣ";

        case EventError::None:
            return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";
        }

        return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";
    }

    std::string clockErrorText(
        ClockError error)
    {
        switch (error)
        {
        case ClockError::Unavailable:
            return "ΤΟ ΡΟΛΟΙ ΔΕΝ ΕΙΝΑΙ ΔΙΑΘΕΣΙΜΟ";

        case ClockError::ReadFailed:
            return "ΑΠΟΤΥΧΙΑ ΑΝΑΓΝΩΣΗΣ ΡΟΛΟΓΙΟΥ";

        case ClockError::WriteFailed:
            return "ΑΠΟΤΥΧΙΑ ΡΥΘΜΙΣΗΣ ΡΟΛΟΓΙΟΥ";

        case ClockError::InvalidDateTime:
            return "ΜΗ ΕΓΚΥΡΗ ΩΡΑ";

        case ClockError::None:
            return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";
        }

        return "ΑΓΝΩΣΤΟ ΣΦΑΛΜΑ";
    }

    std::string greekMonthName(int month)
    {
        constexpr std::array<
            std::string_view,
            12>
            MONTH_NAMES{
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
                "ΔΕΚΕΜΒΡΙΟΣ"};

        if (
            month < 1 ||
            month > 12)
        {
            return "";
        }

        return std::string(
            MONTH_NAMES[static_cast<std::size_t>(
                month - 1)]);
    }

    UiRect monthCellBounds(
        std::size_t index)
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
            AgendaUiLayout::MonthCellHeight};
    }

    UiRect dayRowBounds(
        std::size_t index)
    {
        constexpr std::array<UiRect, 3> rows{
            AgendaUiLayout::DayRow0,
            AgendaUiLayout::DayRow1,
            AgendaUiLayout::DayRow2};

        return rows[index];
    }

    void drawButton(
        DrawSurface &surface,
        const UiRect &bounds,
        std::string_view label,
        UiColor fillColor,
        UiColor textColor)
    {
        surface.fillRoundedRectangle(
            bounds,
            8,
            fillColor);

        surface.drawText(
            bounds,
            label,
            UiTextStyle::Body,
            textColor,
            UiTextAlignment::Center);
    }

    void renderStartupError(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        surface.drawText(
            AgendaUiLayout::Header,
            "ΣΦΑΛΜΑ ΕΚΚΙΝΗΣΗΣ",
            UiTextStyle::Title,
            AgendaUiColors::Error,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::Content,
            controllerErrorText(
                view.controllerError),
            UiTextStyle::Body,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        drawButton(
            surface,
            AgendaUiLayout::FooterCenter,
            "ΞΑΝΑ",
            AgendaUiColors::Accent,
            AgendaUiColors::Background);
    }

    void renderClockSetup(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        surface.drawText(
            AgendaUiLayout::Header,
            "ΡΥΘΜΙΣΗ ΩΡΑΣ",
            UiTextStyle::Title,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.fillRoundedRectangle(
            AgendaUiLayout::EditorValue,
            10,
            AgendaUiColors::Surface);

        surface.drawText(
            AgendaUiLayout::EditorValue,
            formatDate(
                view.clockDraft.date),
            UiTextStyle::Heading,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.fillRoundedRectangle(
            AgendaUiLayout::EditorSecondary,
            10,
            AgendaUiColors::Surface);

        surface.drawText(
            AgendaUiLayout::EditorSecondary,
            formatTime(
                view.clockDraft.time),
            UiTextStyle::Title,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);

        drawButton(
            surface,
            AgendaUiLayout::ClockDatePrevious,
            "<",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::ClockDateNext,
            ">",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::ClockHourUp,
            "+",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::ClockHourDown,
            "-",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::ClockMinuteUp,
            "+",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::ClockMinuteDown,
            "-",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::FooterCenter,
            "ΕΝΤΑΞΕΙ",
            AgendaUiColors::Accent,
            AgendaUiColors::Background);
    }

    void renderMonth(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        const std::string title =
            greekMonthName(
                view.displayedMonth.month) +
            " " +
            std::to_string(
                view.displayedMonth.year);

        surface.drawText(
            AgendaUiLayout::Header,
            title,
            UiTextStyle::Heading,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::MonthPrevious,
            "<",
            UiTextStyle::Heading,
            view.monthPreviousEnabled
                ? AgendaUiColors::PrimaryText
                : AgendaUiColors::Disabled,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::MonthNext,
            ">",
            UiTextStyle::Heading,
            view.monthNextEnabled
                ? AgendaUiColors::PrimaryText
                : AgendaUiColors::Disabled,
            UiTextAlignment::Center);

        constexpr std::array<
            std::string_view,
            7>
            WEEKDAYS{
                "ΔΕ",
                "ΤΡ",
                "ΤΕ",
                "ΠΕ",
                "ΠΑ",
                "ΣΑ",
                "ΚΥ"};

        for (
            std::size_t index = 0;
            index < WEEKDAYS.size();
            ++index)
        {
            const UiRect bounds{
                AgendaUiLayout::MonthWeekdays.x +
                    static_cast<int>(index) *
                        AgendaUiLayout::MonthCellWidth,
                AgendaUiLayout::MonthWeekdays.y,
                AgendaUiLayout::MonthCellWidth,
                AgendaUiLayout::MonthWeekdays.height};

            surface.drawText(
                bounds,
                WEEKDAYS[index],
                UiTextStyle::Caption,
                AgendaUiColors::SecondaryText,
                UiTextAlignment::Center);
        }

        for (
            std::size_t index = 0;
            index < view.monthCells.size();
            ++index)
        {
            const AgendaUiMonthCell &cell =
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
                    bounds.height / 2};

            if (cell.isToday)
            {
                surface.drawCircle(
                    center,
                    9,
                    AgendaUiColors::Accent,
                    1);
            }

            if (cell.isSelected)
            {
                surface.fillCircle(
                    center,
                    9,
                    AgendaUiColors::Accent);
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
                    cell.date->day),
                UiTextStyle::Caption,
                textColor,
                UiTextAlignment::Center);

            if (cell.hasEvents)
            {
                surface.fillCircle(
                    UiPoint{
                        center.x,
                        bounds.y +
                            bounds.height - 3},
                    2,
                    AgendaUiColors::Accent);
            }
        }
    }

    void renderDay(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        if (!view.selectedDate.has_value())
        {
            surface.drawText(
                AgendaUiLayout::Content,
                "ΗΜΕΡΟΜΗΝΙΑ ΜΗ ΔΙΑΘΕΣΙΜΗ",
                UiTextStyle::Body,
                AgendaUiColors::Error,
                UiTextAlignment::Center);

            return;
        }

        surface.drawText(
            AgendaUiLayout::DayHeader,
            formatDate(
                *view.selectedDate),
            UiTextStyle::Heading,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        drawButton(
            surface,
            AgendaUiLayout::DayBack,
            "<",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::DayCreate,
            "+",
            AgendaUiColors::Accent,
            AgendaUiColors::Background);

        const std::size_t rowCount =
            view.dayEvents.size() <
                    AGENDA_UI_EVENTS_PER_PAGE
                ? view.dayEvents.size()
                : AGENDA_UI_EVENTS_PER_PAGE;

        for (
            std::size_t index = 0;
            index < rowCount;
            ++index)
        {
            const AgendaUiEventRow &eventRow =
                view.dayEvents[index];

            const UiRect bounds =
                dayRowBounds(index);

            const UiRect timeBounds{
                bounds.x + 8,
                bounds.y,
                58,
                bounds.height};

            const UiRect textBounds{
                bounds.x + 70,
                bounds.y,
                bounds.width - 78,
                bounds.height};

            surface.fillRoundedRectangle(
                bounds,
                8,
                AgendaUiColors::Surface);

            const std::string timeText =
                eventRow.time.has_value()
                    ? formatTime(*eventRow.time)
                    : "ΟΛΗ ΜΕΡΑ";

            surface.drawText(
                timeBounds,
                timeText,
                UiTextStyle::Caption,
                AgendaUiColors::Accent,
                UiTextAlignment::Left);

            surface.drawText(
                textBounds,
                utf8Prefix(
                    eventRow.text,
                    32),
                UiTextStyle::Body,
                AgendaUiColors::PrimaryText,
                UiTextAlignment::Left);
        }

        surface.drawText(
            AgendaUiLayout::FooterLeft,
            "<",
            UiTextStyle::Heading,
            view.dayPreviousEnabled
                ? AgendaUiColors::Accent
                : AgendaUiColors::Disabled,
            UiTextAlignment::Center);

        if (view.dayPageCount > 0)
        {
            const std::string pageText =
                std::to_string(
                    view.dayPageIndex + 1) +
                "/" +
                std::to_string(
                    view.dayPageCount);

            surface.drawText(
                AgendaUiLayout::FooterCenter,
                pageText,
                UiTextStyle::Body,
                AgendaUiColors::SecondaryText,
                UiTextAlignment::Center);
        }

        surface.drawText(
            AgendaUiLayout::FooterRight,
            ">",
            UiTextStyle::Heading,
            view.dayNextEnabled
                ? AgendaUiColors::Accent
                : AgendaUiColors::Disabled,
            UiTextAlignment::Center);
    }

    void renderEventDetails(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        surface.drawText(
            AgendaUiLayout::Header,
            "ΓΕΓΟΝΟΣ",
            UiTextStyle::Heading,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        if (!view.selectedEvent.has_value())
        {
            surface.drawText(
                AgendaUiLayout::Content,
                "ΤΟ ΓΕΓΟΝΟΣ ΔΕΝ ΒΡΕΘΗΚΕ",
                UiTextStyle::Body,
                AgendaUiColors::Error,
                UiTextAlignment::Center);

            return;
        }

        const Event &event =
            *view.selectedEvent;

        surface.drawText(
            AgendaUiLayout::Subheader,
            formatDate(
                event.eventDate),
            UiTextStyle::Body,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        const UiRect timeBounds{
            28,
            60,
            184,
            26};

        const UiRect textBounds{
            28,
            92,
            184,
            84};

        const std::string timeText =
            event.eventTime.has_value()
                ? formatTime(*event.eventTime)
                : "ΟΛΗ ΜΕΡΑ";

        surface.drawText(
            timeBounds,
            timeText,
            UiTextStyle::Heading,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);

        surface.drawText(
            textBounds,
            event.text,
            UiTextStyle::Body,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterLeft,
            "ΠΙΣΩ",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterCenter,
            "ΑΛΛΑΓΗ",
            UiTextStyle::Caption,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterRight,
            "ΔΙΑΓΡΑΦΗ",
            UiTextStyle::Caption,
            AgendaUiColors::Error,
            UiTextAlignment::Center);
    }

    void renderEditorDate(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        surface.drawText(
            AgendaUiLayout::Header,
            "ΗΜΕΡΟΜΗΝΙΑ",
            UiTextStyle::Title,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.fillRoundedRectangle(
            AgendaUiLayout::EditorDateValue,
            10,
            AgendaUiColors::Surface);

        surface.drawText(
            AgendaUiLayout::EditorDateValue,
            formatDate(
                view.editor.date),
            UiTextStyle::Heading,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);

        drawButton(
            surface,
            AgendaUiLayout::EditorDatePrevious,
            "<",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::EditorDateNext,
            ">",
            AgendaUiColors::Surface,
            AgendaUiColors::PrimaryText);

        surface.drawText(
            AgendaUiLayout::FooterLeft,
            "ΠΙΣΩ",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterCenter,
            "ΑΚΥΡΩΣΗ",
            UiTextStyle::Caption,
            AgendaUiColors::Error,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterRight,
            "ΕΠΟΜΕΝΟ",
            UiTextStyle::Caption,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);
    }

    void renderEditorTime(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        surface.drawText(
            AgendaUiLayout::Header,
            "ΩΡΑ",
            UiTextStyle::Title,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        const bool timed =
            view.editor.time.has_value();

        drawButton(
            surface,
            AgendaUiLayout::EditorTimedMode,
            "ΜΕ ΩΡΑ",
            timed
                ? AgendaUiColors::Accent
                : AgendaUiColors::Surface,
            timed
                ? AgendaUiColors::Background
                : AgendaUiColors::PrimaryText);

        drawButton(
            surface,
            AgendaUiLayout::EditorAllDayMode,
            "ΟΛΗ ΜΕΡΑ",
            !timed
                ? AgendaUiColors::Accent
                : AgendaUiColors::Surface,
            !timed
                ? AgendaUiColors::Background
                : AgendaUiColors::PrimaryText);

        if (timed)
        {
            surface.fillRoundedRectangle(
                AgendaUiLayout::EditorSecondary,
                10,
                AgendaUiColors::Surface);

            surface.drawText(
                AgendaUiLayout::EditorSecondary,
                formatTime(
                    *view.editor.time),
                UiTextStyle::Title,
                AgendaUiColors::Accent,
                UiTextAlignment::Center);

            drawButton(
                surface,
                AgendaUiLayout::EditorHourUp,
                "+",
                AgendaUiColors::Accent,
                AgendaUiColors::Background);

            drawButton(
                surface,
                AgendaUiLayout::EditorHourDown,
                "-",
                AgendaUiColors::Accent,
                AgendaUiColors::Background);

            drawButton(
                surface,
                AgendaUiLayout::EditorMinuteUp,
                "+",
                AgendaUiColors::Accent,
                AgendaUiColors::Background);

            drawButton(
                surface,
                AgendaUiLayout::EditorMinuteDown,
                "-",
                AgendaUiColors::Accent,
                AgendaUiColors::Background);
        }

        surface.drawText(
            AgendaUiLayout::FooterLeft,
            "ΠΙΣΩ",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterCenter,
            "ΑΚΥΡΩΣΗ",
            UiTextStyle::Caption,
            AgendaUiColors::Error,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterRight,
            "ΕΠΟΜΕΝΟ",
            UiTextStyle::Caption,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);
    }

    void renderEditorReview(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        const std::string title =
            view.editor.mode ==
                    AgendaUiEditorMode::Create
                ? "ΝΕΟ ΓΕΓΟΝΟΣ"
                : "ΑΛΛΑΓΗ ΓΕΓΟΝΟΤΟΣ";

        surface.drawText(
            AgendaUiLayout::Header,
            title,
            UiTextStyle::Heading,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::Subheader,
            formatDate(
                view.editor.date),
            UiTextStyle::Body,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        const UiRect timeBounds{
            28,
            58,
            184,
            26};

        const UiRect textBounds{
            28,
            88,
            184,
            88};

        const std::string timeText =
            view.editor.time.has_value()
                ? formatTime(*view.editor.time)
                : "ΟΛΗ ΜΕΡΑ";

        surface.drawText(
            timeBounds,
            timeText,
            UiTextStyle::Heading,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);

        surface.drawText(
            textBounds,
            utf8Prefix(
                view.editor.text,
                64),
            UiTextStyle::Body,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterLeft,
            "ΠΙΣΩ",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        drawButton(
            surface,
            AgendaUiLayout::FooterRight,
            "ΑΠΟΘΗΚΕΥΣΗ",
            AgendaUiColors::Accent,
            AgendaUiColors::Background);
    }

    UiRect t9KeyBounds(
        std::size_t index)
    {
        const int column =
            static_cast<int>(index % 3);

        const int row =
            static_cast<int>(index / 3);

        return UiRect{
            AgendaUiLayout::T9GridX +
                column *
                    (AgendaUiLayout::T9KeyWidth +
                     AgendaUiLayout::T9ColumnGap),
            AgendaUiLayout::T9GridY +
                row *
                    (AgendaUiLayout::T9KeyHeight +
                     AgendaUiLayout::T9RowGap),
            AgendaUiLayout::T9KeyWidth,
            AgendaUiLayout::T9KeyHeight};
    }

    std::array<std::string, 12> t9Labels(
        T9Language language,
        T9Mode mode)
    {
        if (mode == T9Mode::Numbers)
        {
            return {
                "1",
                "2",
                "3",
                "4",
                "5",
                "6",
                "7",
                "8",
                "9",
                "*",
                "0",
                "#"};
        }

        if (language == T9Language::English)
        {
            return {
                "1",
                "2 ABC",
                "3 DEF",
                "4 GHI",
                "5 JKL",
                "6 MNO",
                "7 PQRS",
                "8 TUV",
                "9 WXYZ",
                "EL/EN",
                "SPACE",
                "<"};
        }

        return {
            "1",
            "2 ΑΒΓ",
            "3 ΔΕΖ",
            "4 ΗΘΙ",
            "5 ΚΛΜ",
            "6 ΝΞΟ",
            "7 ΠΡΣ",
            "8 ΤΥΦ",
            "9 ΧΨΩ",
            "EL/EN",
            "ΚΕΝΟ",
            "<"};
    }

    void renderEditorText(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        surface.drawText(
            AgendaUiLayout::Header,
            "ΚΕΙΜΕΝΟ",
            UiTextStyle::Title,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);

        surface.fillRoundedRectangle(
            AgendaUiLayout::T9Preview,
            8,
            AgendaUiColors::Surface);

        surface.drawText(
            AgendaUiLayout::T9Preview,
            utf8Prefix(
                view.editor.text,
                48),
            UiTextStyle::Body,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Left);

        const UiRect languageBounds{
            AgendaUiLayout::T9Status.x,
            AgendaUiLayout::T9Status.y,
            44,
            AgendaUiLayout::T9Status.height};

        const UiRect modeBounds{
            AgendaUiLayout::T9Status.x + 44,
            AgendaUiLayout::T9Status.y,
            76,
            AgendaUiLayout::T9Status.height};

        const UiRect caseBounds{
            AgendaUiLayout::T9Status.x + 120,
            AgendaUiLayout::T9Status.y,
            44,
            AgendaUiLayout::T9Status.height};

        surface.drawText(
            languageBounds,
            view.t9Language ==
                    T9Language::Greek
                ? "ΕΛ"
                : "EN",
            UiTextStyle::Caption,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);

        surface.drawText(
            modeBounds,
            view.t9Mode ==
                    T9Mode::Letters
                ? "ΓΡΑΜΜΑΤΑ"
                : "ΑΡΙΘΜΟΙ",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        surface.drawText(
            caseBounds,
            view.t9Uppercase
                ? "ΚΕΦΑΛΑΙΑ"
                : "πεζά",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        const std::array<std::string, 12> labels =
            t9Labels(
                view.t9Language,
                view.t9Mode);

        for (
            std::size_t index = 0;
            index < labels.size();
            ++index)
        {
            const UiRect bounds =
                t9KeyBounds(index);

            surface.fillRoundedRectangle(
                bounds,
                5,
                AgendaUiColors::Surface);

            surface.drawText(
                bounds,
                labels[index],
                UiTextStyle::Keypad,
                AgendaUiColors::PrimaryText,
                UiTextAlignment::Center);
        }

        surface.drawText(
            AgendaUiLayout::FooterLeft,
            "ΠΙΣΩ",
            UiTextStyle::Caption,
            AgendaUiColors::SecondaryText,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::FooterRight,
            "ΕΠΟΜΕΝΟ",
            UiTextStyle::Caption,
            AgendaUiColors::Accent,
            UiTextAlignment::Center);
    }

    void drawOverlayCard(
        DrawSurface &surface,
        std::string_view title)
    {
        surface.fillRectangle(
            AgendaUiLayout::FullDisplay,
            AgendaUiColors::Overlay);

        surface.fillRoundedRectangle(
            AgendaUiLayout::OverlayCard,
            12,
            AgendaUiColors::Surface);

        surface.drawRoundedRectangle(
            AgendaUiLayout::OverlayCard,
            12,
            AgendaUiColors::Accent,
            1);

        surface.drawText(
            UiRect{40, 58, 160, 28},
            title,
            UiTextStyle::Heading,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);
    }

    void renderOverlay(
        const AgendaUiView &view,
        DrawSurface &surface)
    {
        if (
            view.overlay ==
            AgendaUiOverlay::None)
        {
            return;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::Reminder)
        {
            drawOverlayCard(
                surface,
                "ΥΠΕΝΘΥΜΙΣΗ");

            if (!view.reminder.has_value())
            {
                surface.drawText(
                    UiRect{40, 92, 160, 48},
                    "ΥΠΕΝΘΥΜΙΣΗ ΜΗ ΔΙΑΘΕΣΙΜΗ",
                    UiTextStyle::Body,
                    AgendaUiColors::Error,
                    UiTextAlignment::Center);
            }
            else
            {
                const Event &reminder =
                    *view.reminder;

                surface.drawText(
                    UiRect{40, 88, 160, 42},
                    utf8Prefix(
                        reminder.text,
                        48),
                    UiTextStyle::Body,
                    AgendaUiColors::PrimaryText,
                    UiTextAlignment::Center);

                const std::string reminderTime =
                    reminder.eventTime.has_value()
                        ? formatTime(
                              *reminder.eventTime)
                        : "ΟΛΗ ΜΕΡΑ";

                surface.drawText(
                    UiRect{40, 128, 160, 20},
                    reminderTime,
                    UiTextStyle::Caption,
                    AgendaUiColors::Accent,
                    UiTextAlignment::Center);
            }

            drawButton(
                surface,
                UiRect{82, 154, 76, 28},
                "ΕΝΤΑΞΕΙ",
                AgendaUiColors::Accent,
                AgendaUiColors::Background);

            return;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::ConfirmDelete)
        {
            drawOverlayCard(
                surface,
                "ΔΙΑΓΡΑΦΗ;");

            drawButton(
                surface,
                UiRect{48, 144, 60, 30},
                "ΝΑΙ",
                AgendaUiColors::Error,
                AgendaUiColors::PrimaryText);

            drawButton(
                surface,
                UiRect{132, 144, 60, 30},
                "ΟΧΙ",
                AgendaUiColors::Surface,
                AgendaUiColors::PrimaryText);

            return;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::ConfirmDiscard)
        {
            drawOverlayCard(
                surface,
                "ΑΠΟΡΡΙΨΗ ΑΛΛΑΓΩΝ;");

            drawButton(
                surface,
                UiRect{48, 144, 60, 30},
                "ΝΑΙ",
                AgendaUiColors::Error,
                AgendaUiColors::PrimaryText);

            drawButton(
                surface,
                UiRect{132, 144, 60, 30},
                "ΟΧΙ",
                AgendaUiColors::Surface,
                AgendaUiColors::PrimaryText);

            return;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::OperationError)
        {
            drawOverlayCard(
                surface,
                "ΣΦΑΛΜΑ");

            const std::string message =
                view.controllerError !=
                        AgendaControllerError::None
                    ? controllerErrorText(
                          view.controllerError)
                    : eventErrorText(
                          view.eventError);

            surface.drawText(
                UiRect{40, 90, 160, 46},
                message,
                UiTextStyle::Body,
                AgendaUiColors::Error,
                UiTextAlignment::Center);

            drawButton(
                surface,
                UiRect{44, 148, 70, 30},
                "ΞΑΝΑ",
                AgendaUiColors::Accent,
                AgendaUiColors::Background);

            drawButton(
                surface,
                UiRect{126, 148, 70, 30},
                "ΑΚΥΡΩΣΗ",
                AgendaUiColors::Surface,
                AgendaUiColors::PrimaryText);

            return;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::ClockError)
        {
            drawOverlayCard(
                surface,
                "ΣΦΑΛΜΑ ΩΡΑΣ");

            surface.drawText(
                UiRect{40, 92, 160, 46},
                clockErrorText(
                    view.clockError),
                UiTextStyle::Body,
                AgendaUiColors::Error,
                UiTextAlignment::Center);

            drawButton(
                surface,
                UiRect{82, 150, 76, 30},
                "ΕΝΤΑΞΕΙ",
                AgendaUiColors::Accent,
                AgendaUiColors::Background);
        }
    }

    void renderSafeFallback(
        DrawSurface &surface)
    {
        surface.drawText(
            AgendaUiLayout::Header,
            "ΣΦΑΛΜΑ ΠΡΟΒΟΛΗΣ",
            UiTextStyle::Title,
            AgendaUiColors::Error,
            UiTextAlignment::Center);

        surface.drawText(
            AgendaUiLayout::Content,
            "Η ΟΘΟΝΗ ΔΕΝ ΕΙΝΑΙ ΔΙΑΘΕΣΙΜΗ",
            UiTextStyle::Body,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);
    }
}

void AgendaRenderer::render(
    const AgendaUiView &view,
    DrawSurface &surface) const
{
    surface.clear(
        AgendaUiColors::Background);

    switch (view.screen)
    {
    case AgendaUiScreen::ClockSetup:
        renderClockSetup(
            view,
            surface);
        break;

    case AgendaUiScreen::Month:
        renderMonth(
            view,
            surface);
        break;

    case AgendaUiScreen::Day:
        renderDay(
            view,
            surface);
        break;

    case AgendaUiScreen::EventDetails:
        renderEventDetails(
            view,
            surface);
        break;

    case AgendaUiScreen::EditorDate:
        renderEditorDate(
            view,
            surface);
        break;

    case AgendaUiScreen::EditorTime:
        renderEditorTime(
            view,
            surface);
        break;

    case AgendaUiScreen::EditorText:
        renderEditorText(
            view,
            surface);
        break;

    case AgendaUiScreen::EditorReview:
        renderEditorReview(
            view,
            surface);
        break;

    case AgendaUiScreen::StartupError:
        renderStartupError(
            view,
            surface);
        break;

    default:
        renderSafeFallback(
            surface);
        break;
    }

    renderOverlay(
        view,
        surface);
}
