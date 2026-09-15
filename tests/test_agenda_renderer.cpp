#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
#include <optional>

#include "core/calendar_engine.h"
#include "ui/agenda_renderer.h"
#include "core/utf8.h"

enum class DrawCommandType
{
    Clear,
    FillRectangle,
    DrawRectangle,
    FillRoundedRectangle,
    DrawRoundedRectangle,
    FillCircle,
    DrawCircle,
    DrawLine,
    DrawText
};

struct DrawCommand
{
    DrawCommandType type =
        DrawCommandType::Clear;

    UiRect bounds{0, 0, 0, 0};

    UiPoint start{0, 0};
    UiPoint end{0, 0};

    int radius = 0;
    int thickness = 0;

    UiColor color{0, 0, 0};

    UiTextStyle textStyle =
        UiTextStyle::Body;

    UiTextAlignment alignment =
        UiTextAlignment::Left;

    std::string text;
};

class RecordingDrawSurface final : public DrawSurface
{
public:
    std::vector<DrawCommand> commands;

    void clear(
        UiColor color) override
    {
        DrawCommand command;

        command.type =
            DrawCommandType::Clear;

        command.color = color;

        commands.push_back(command);
    }

    void fillRectangle(
        const UiRect &bounds,
        UiColor color) override
    {
        recordRectangle(
            DrawCommandType::FillRectangle,
            bounds,
            0,
            0,
            color);
    }

    void drawRectangle(
        const UiRect &bounds,
        UiColor color,
        int thickness) override
    {
        recordRectangle(
            DrawCommandType::DrawRectangle,
            bounds,
            0,
            thickness,
            color);
    }

    void fillRoundedRectangle(
        const UiRect &bounds,
        int radius,
        UiColor color) override
    {
        recordRectangle(
            DrawCommandType::
                FillRoundedRectangle,
            bounds,
            radius,
            0,
            color);
    }

    void drawRoundedRectangle(
        const UiRect &bounds,
        int radius,
        UiColor color,
        int thickness) override
    {
        recordRectangle(
            DrawCommandType::
                DrawRoundedRectangle,
            bounds,
            radius,
            thickness,
            color);
    }

    void fillCircle(
        UiPoint center,
        int radius,
        UiColor color) override
    {
        recordCircle(
            DrawCommandType::FillCircle,
            center,
            radius,
            0,
            color);
    }

    void drawCircle(
        UiPoint center,
        int radius,
        UiColor color,
        int thickness) override
    {
        recordCircle(
            DrawCommandType::DrawCircle,
            center,
            radius,
            thickness,
            color);
    }

    void drawLine(
        UiPoint start,
        UiPoint end,
        UiColor color,
        int thickness) override
    {
        DrawCommand command;

        command.type =
            DrawCommandType::DrawLine;

        command.start = start;
        command.end = end;
        command.color = color;
        command.thickness = thickness;

        commands.push_back(command);
    }

    void drawText(
        const UiRect &bounds,
        std::string_view text,
        UiTextStyle style,
        UiColor color,
        UiTextAlignment alignment) override
    {
        DrawCommand command;

        command.type =
            DrawCommandType::DrawText;

        command.bounds = bounds;
        command.text = std::string(text);
        command.textStyle = style;
        command.color = color;
        command.alignment = alignment;

        commands.push_back(command);
    }

private:
    void recordRectangle(
        DrawCommandType type,
        const UiRect &bounds,
        int radius,
        int thickness,
        UiColor color)
    {
        DrawCommand command;

        command.type = type;
        command.bounds = bounds;
        command.radius = radius;
        command.thickness = thickness;
        command.color = color;

        commands.push_back(command);
    }

    void recordCircle(
        DrawCommandType type,
        UiPoint center,
        int radius,
        int thickness,
        UiColor color)
    {
        DrawCommand command;

        command.type = type;
        command.start = center;
        command.radius = radius;
        command.thickness = thickness;
        command.color = color;

        commands.push_back(command);
    }
};

bool containsText(
    const RecordingDrawSurface &surface,
    std::string_view expected)
{
    return std::any_of(
        surface.commands.begin(),
        surface.commands.end(),
        [expected](
            const DrawCommand &command)
        {
            return command.type ==
                       DrawCommandType::DrawText &&
                   command.text == expected;
        });
}

bool hasButton(
    const RecordingDrawSurface &surface,
    const UiRect &bounds,
    std::string_view label)
{
    const bool hasBackground =
        std::any_of(
            surface.commands.begin(),
            surface.commands.end(),
            [&](const DrawCommand &command)
            {
                return command.type ==
                           DrawCommandType::
                               FillRoundedRectangle &&
                       command.bounds == bounds;
            });

    const bool hasLabel =
        std::any_of(
            surface.commands.begin(),
            surface.commands.end(),
            [&](const DrawCommand &command)
            {
                return command.type ==
                           DrawCommandType::DrawText &&
                       command.bounds == bounds &&
                       command.text == label;
            });

    return hasBackground && hasLabel;
}

const DrawCommand *findTextCommand(
    const RecordingDrawSurface &surface,
    std::string_view expected)
{
    const auto found =
        std::find_if(
            surface.commands.begin(),
            surface.commands.end(),
            [expected](
                const DrawCommand &command)
            {
                return command.type ==
                           DrawCommandType::DrawText &&
                       command.text == expected;
            });

    if (
        found ==
        surface.commands.end())
    {
        return nullptr;
    }

    return &*found;
}

std::size_t commandIndex(
    const RecordingDrawSurface &surface,
    const DrawCommand *command)
{
    assert(command != nullptr);

    return static_cast<std::size_t>(
        command -
        surface.commands.data());
}

bool containsTextWithColor(
    const RecordingDrawSurface &surface,
    std::string_view expected,
    UiColor color)
{
    return std::any_of(
        surface.commands.begin(),
        surface.commands.end(),
        [expected, color](
            const DrawCommand &command)
        {
            return command.type ==
                       DrawCommandType::DrawText &&
                   command.text == expected &&
                   command.color == color;
        });
}

bool rectInside(
    const UiRect &inner,
    const UiRect &outer)
{
    return inner.x >= outer.x &&
           inner.y >= outer.y &&
           inner.x + inner.width <=
               outer.x + outer.width &&
           inner.y + inner.height <=
               outer.y + outer.height;
}

bool pointInDisplay(
    UiPoint value)
{
    return value.x >= 0 &&
           value.y >= 0 &&
           value.x <
               AGENDA_UI_DISPLAY_WIDTH &&
           value.y <
               AGENDA_UI_DISPLAY_HEIGHT;
}

bool rectInDisplay(
    const UiRect &value)
{
    return value.x >= 0 &&
           value.y >= 0 &&
           value.width >= 0 &&
           value.height >= 0 &&
           value.x + value.width <=
               AGENDA_UI_DISPLAY_WIDTH &&
           value.y + value.height <=
               AGENDA_UI_DISPLAY_HEIGHT;
}

bool commandInDisplay(
    const DrawCommand &command)
{
    if (
        command.type ==
        DrawCommandType::Clear)
    {
        return true;
    }

    if (
        command.type ==
        DrawCommandType::DrawLine)
    {
        return pointInDisplay(
                   command.start) &&
               pointInDisplay(
                   command.end);
    }

    if (
        command.type ==
            DrawCommandType::FillCircle ||
        command.type ==
            DrawCommandType::DrawCircle)
    {
        return command.start.x -
                       command.radius >=
                   0 &&
               command.start.y -
                       command.radius >=
                   0 &&
               command.start.x +
                       command.radius <
                   AGENDA_UI_DISPLAY_WIDTH &&
               command.start.y +
                       command.radius <
                   AGENDA_UI_DISPLAY_HEIGHT;
    }

    return rectInDisplay(
        command.bounds);
}

bool allCommandsInBounds(
    const RecordingDrawSurface &surface)
{
    return std::all_of(
        surface.commands.begin(),
        surface.commands.end(),
        commandInDisplay);
}

bool rectCenterIsVisibleOnRoundDisplay(
    const UiRect &bounds)
{
    constexpr int safeMargin = 10;
    constexpr int centerX =
        AGENDA_UI_DISPLAY_WIDTH / 2;
    constexpr int centerY =
        AGENDA_UI_DISPLAY_HEIGHT / 2;
    constexpr int visibleRadius =
        AGENDA_UI_DISPLAY_WIDTH / 2 -
        safeMargin;

    const int controlCenterX =
        bounds.x + bounds.width / 2;
    const int controlCenterY =
        bounds.y + bounds.height / 2;
    const int deltaX =
        controlCenterX - centerX;
    const int deltaY =
        controlCenterY - centerY;

    return
        deltaX * deltaX +
            deltaY * deltaY <=
        visibleRadius * visibleRadius;
}

void test_renderer_clears_with_dark_background()
{
    AgendaUiView view;
    RecordingDrawSurface surface;
    AgendaRenderer renderer;

    renderer.render(view, surface);

    assert(!surface.commands.empty());

    assert(
        surface.commands.front().type ==
        DrawCommandType::Clear);

    assert(
        surface.commands.front().color ==
        AgendaUiColors::Background);
}

void test_renderer_does_not_change_input_view()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Day;

    view.selectedDate =
        Date{2026, 9, 9};

    view.dayPageIndex = 1;
    view.dayPageCount = 3;

    RecordingDrawSurface surface;
    AgendaRenderer renderer;

    renderer.render(view, surface);

    assert(
        view.screen ==
        AgendaUiScreen::Day);

    assert(view.selectedDate.has_value());
    assert(view.selectedDate->day == 9);
    assert(view.dayPageIndex == 1);
    assert(view.dayPageCount == 3);
}

void test_startup_error_shows_retry_action()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::StartupError;

    view.controllerError =
        AgendaControllerError::ReadFailed;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "ΣΦΑΛΜΑ ΕΚΚΙΝΗΣΗΣ"));

    assert(
        containsText(
            surface,
            "ΑΠΟΤΥΧΙΑ ΑΝΑΓΝΩΣΗΣ"));

    assert(
        containsText(
            surface,
            "ΞΑΝΑ"));
}

void test_clock_setup_formats_date_and_time()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::ClockSetup;

    view.clockDraft = DateTime{
        Date{2026, 9, 9},
        TimeOfDay{7, 5}};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "ΡΥΘΜΙΣΗ ΩΡΑΣ"));

    assert(
        containsText(
            surface,
            "09/09/2026"));

    assert(
        containsText(
            surface,
            "07:05"));

    assert(
        containsText(
            surface,
            "ΕΝΤΑΞΕΙ"));
}

void test_clock_setup_renders_all_adjustment_buttons()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::ClockSetup;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    constexpr UiRect datePrevious{
        20,
        67,
        28,
        34};

    constexpr UiRect dateNext{
        192,
        67,
        28,
        34};

    constexpr UiRect hourUp{
        42,
        116,
        56,
        18};

    constexpr UiRect hourDown{
        42,
        158,
        56,
        18};

    constexpr UiRect minuteUp{
        142,
        116,
        56,
        18};

    constexpr UiRect minuteDown{
        142,
        158,
        56,
        18};

    assert(
        hasButton(
            surface,
            datePrevious,
            "<"));

    assert(
        hasButton(
            surface,
            dateNext,
            ">"));

    assert(hasButton(surface, hourUp, "+"));
    assert(hasButton(surface, hourDown, "-"));
    assert(hasButton(surface, minuteUp, "+"));
    assert(hasButton(surface, minuteDown, "-"));
}

void test_clock_setup_uses_only_in_bounds_commands()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::ClockSetup;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        allCommandsInBounds(surface));
}

AgendaUiView septemberMonthView()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Month;

    view.displayedMonth =
        YearMonth{2026, 9};

    view.monthPreviousEnabled = true;
    view.monthNextEnabled = true;

    const MonthGrid grid =
        buildMonthGrid(
            view.displayedMonth);

    for (
        std::size_t index = 0;
        index < grid.size();
        ++index)
    {
        view.monthCells[index].date =
            grid[index].date;

        view.monthCells[index].inDisplayedMonth =
            grid[index].inDisplayedMonth;
    }

    return view;
}

void test_month_renders_title_weekdays_and_all_cells()
{
    const AgendaUiView view =
        septemberMonthView();

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "ΣΕΠΤΕΜΒΡΙΟΣ 2026"));

    assert(containsText(surface, "ΔΕ"));
    assert(containsText(surface, "ΤΡ"));
    assert(containsText(surface, "ΤΕ"));
    assert(containsText(surface, "ΠΕ"));
    assert(containsText(surface, "ΠΑ"));
    assert(containsText(surface, "ΣΑ"));
    assert(containsText(surface, "ΚΥ"));

    const std::size_t cellTextCount =
        static_cast<std::size_t>(
            std::count_if(
                surface.commands.begin(),
                surface.commands.end(),
                [](
                    const DrawCommand &command)
                {
                    return command.type ==
                               DrawCommandType::
                                   DrawText &&
                           rectInside(
                               command.bounds,
                               AgendaUiLayout::
                                   MonthGrid);
                }));

    assert(cellTextCount == 42);
}

void test_month_renders_selected_today_and_event_states()
{
    AgendaUiView view =
        septemberMonthView();

    view.monthCells[10].isSelected =
        true;

    view.monthCells[11].isToday =
        true;

    view.monthCells[12].hasEvents =
        true;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    const std::size_t accentFills =
        static_cast<std::size_t>(
            std::count_if(
                surface.commands.begin(),
                surface.commands.end(),
                [](
                    const DrawCommand &command)
                {
                    return command.type ==
                               DrawCommandType::
                                   FillCircle &&
                           command.color ==
                               AgendaUiColors::
                                   Accent;
                }));

    const bool hasTodayOutline =
        std::any_of(
            surface.commands.begin(),
            surface.commands.end(),
            [](
                const DrawCommand &command)
            {
                return command.type ==
                           DrawCommandType::
                               DrawCircle &&
                       command.color ==
                           AgendaUiColors::
                               Accent;
            });

    assert(accentFills >= 2);
    assert(hasTodayOutline);
}

void test_month_renders_disabled_navigation_and_adjacent_dates()
{
    AgendaUiView view =
        septemberMonthView();

    view.monthPreviousEnabled = false;
    view.monthNextEnabled = true;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsTextWithColor(
            surface,
            "<",
            AgendaUiColors::Disabled));

    assert(
        containsTextWithColor(
            surface,
            ">",
            AgendaUiColors::PrimaryText));

    assert(
        containsTextWithColor(
            surface,
            "31",
            AgendaUiColors::Disabled));
}

void test_month_navigation_is_visible_on_round_display()
{
    const AgendaUiView view =
        septemberMonthView();

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    const DrawCommand *previous =
        findTextCommand(surface, "<");
    const DrawCommand *next =
        findTextCommand(surface, ">");

    assert(previous != nullptr);
    assert(next != nullptr);
    assert(
        rectCenterIsVisibleOnRoundDisplay(
            previous->bounds));
    assert(
        rectCenterIsVisibleOnRoundDisplay(
            next->bounds));
}

void test_day_renders_three_rows_and_page_state()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Day;

    view.selectedDate =
        Date{2026, 9, 9};

    view.dayPageIndex = 1;
    view.dayPageCount = 3;

    view.dayPreviousEnabled = true;
    view.dayNextEnabled = true;

    view.dayEvents = {
        AgendaUiEventRow{
            1,
            TimeOfDay{9, 5},
            "Οδοντίατρος"},
        AgendaUiEventRow{
            2,
            TimeOfDay{12, 30},
            "Συνάντηση"},
        AgendaUiEventRow{
            3,
            TimeOfDay{18, 0},
            "Προπόνηση"}};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "09/09/2026"));

    assert(
        containsText(
            surface,
            "2/3"));

    assert(
        containsText(
            surface,
            "09:05"));

    assert(
        containsText(
            surface,
            "Οδοντίατρος"));

    assert(
        containsText(
            surface,
            "Συνάντηση"));

    assert(
        containsText(
            surface,
            "Προπόνηση"));
}

void test_day_uses_all_day_label_and_utf8_safe_preview()
{
    const std::string longText =
        "Αυτό είναι ένα πολύ μεγάλο ελληνικό γεγονός "
        "που πρέπει να κοπεί με ασφάλεια";

    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Day;

    view.selectedDate =
        Date{2026, 9, 9};

    view.dayEvents = {
        AgendaUiEventRow{
            7,
            std::nullopt,
            longText}};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    const std::string expected =
        utf8Prefix(
            longText,
            32);

    assert(
        containsText(
            surface,
            "ΟΛΗ ΜΕΡΑ"));

    assert(
        containsText(
            surface,
            expected));

    assert(
        isValidUtf8(expected));
}

void test_event_details_render_event_or_safe_error()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EventDetails;

    view.selectedEvent = Event{
        7,
        Date{2026, 9, 10},
        TimeOfDay{8, 5},
        "Ιατρικό ραντεβού",
        false,
        DateTime{
            Date{2026, 9, 9},
            TimeOfDay{10, 0}}};

    RecordingDrawSurface eventSurface;

    AgendaRenderer{}.render(
        view,
        eventSurface);

    assert(
        containsText(
            eventSurface,
            "10/09/2026"));

    assert(
        containsText(
            eventSurface,
            "08:05"));

    assert(
        containsText(
            eventSurface,
            "Ιατρικό ραντεβού"));

    assert(
        containsText(
            eventSurface,
            "ΠΙΣΩ"));

    assert(
        containsText(
            eventSurface,
            "ΑΛΛΑΓΗ"));

    assert(
        containsText(
            eventSurface,
            "ΔΙΑΓΡΑΦΗ"));

    view.selectedEvent.reset();

    RecordingDrawSurface missingSurface;

    AgendaRenderer{}.render(
        view,
        missingSurface);

    assert(
        containsText(
            missingSurface,
            "ΤΟ ΓΕΓΟΝΟΣ ΔΕΝ ΒΡΕΘΗΚΕ"));
}

void test_editor_date_renders_draft_and_navigation()
{
    AgendaUiView view;
    view.screen =
        AgendaUiScreen::EditorDate;

    view.editor.date =
        Date{2026, 9, 10};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "ΗΜΕΡΟΜΗΝΙΑ"));

    assert(
        containsText(
            surface,
            "10/09/2026"));

    assert(
        containsText(
            surface,
            "ΠΙΣΩ"));

    assert(
        containsText(
            surface,
            "ΕΠΟΜΕΝΟ"));

    assert(
        containsText(
            surface,
            "ΑΚΥΡΩΣΗ"));
}

void test_editor_time_renders_timed_and_all_day_variants()
{
    AgendaUiView view;
    view.screen =
        AgendaUiScreen::EditorTime;

    view.editor.time =
        TimeOfDay{8, 5};

    RecordingDrawSurface timedSurface;

    AgendaRenderer{}.render(
        view,
        timedSurface);

    assert(
        containsText(
            timedSurface,
            "ΩΡΑ"));

    assert(
        containsText(
            timedSurface,
            "ΜΕ ΩΡΑ"));

    assert(
        containsText(
            timedSurface,
            "08:05"));

    view.editor.time.reset();

    RecordingDrawSurface allDaySurface;

    AgendaRenderer{}.render(
        view,
        allDaySurface);

    assert(
        containsText(
            allDaySurface,
            "ΟΛΗ ΜΕΡΑ"));

    assert(
        !containsText(
            allDaySurface,
            "08:05"));
}

void test_editor_review_renders_create_and_update_variants()
{
    AgendaUiView view;
    view.screen =
        AgendaUiScreen::EditorReview;

    view.editor.date =
        Date{2026, 9, 10};

    view.editor.time =
        TimeOfDay{8, 5};

    view.editor.text =
        "Ραντεβού";

    view.editor.mode =
        AgendaUiEditorMode::Create;

    RecordingDrawSurface createSurface;

    AgendaRenderer{}.render(
        view,
        createSurface);

    assert(
        containsText(
            createSurface,
            "ΝΕΟ ΓΕΓΟΝΟΣ"));

    assert(
        containsText(
            createSurface,
            "10/09/2026"));

    assert(
        containsText(
            createSurface,
            "08:05"));

    assert(
        containsText(
            createSurface,
            "Ραντεβού"));

    assert(
        containsText(
            createSurface,
            "ΑΠΟΘΗΚΕΥΣΗ"));

    view.editor.mode =
        AgendaUiEditorMode::Update;

    view.editor.eventId = 7;

    RecordingDrawSurface updateSurface;

    AgendaRenderer{}.render(
        view,
        updateSurface);

    assert(
        containsText(
            updateSurface,
            "ΑΛΛΑΓΗ ΓΕΓΟΝΟΤΟΣ"));

    assert(
        containsText(
            updateSurface,
            "ΑΠΟΘΗΚΕΥΣΗ"));
}

void test_t9_editor_renders_greek_letters_and_twelve_keys()
{
    AgendaUiView view;
    view.screen =
        AgendaUiScreen::EditorText;

    view.t9Language =
        T9Language::Greek;

    view.t9Mode =
        T9Mode::Letters;

    view.t9Uppercase = false;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "ΕΛ"));

    assert(
        containsText(
            surface,
            "ΓΡΑΜΜΑΤΑ"));

    assert(
        containsText(
            surface,
            "πεζά"));

    assert(
        containsText(
            surface,
            "2 ΑΒΓ"));

    assert(
        containsText(
            surface,
            "7 ΠΡΣ"));

    assert(
        containsText(
            surface,
            "9 ΧΨΩ"));

    const std::size_t keyCount =
        static_cast<std::size_t>(
            std::count_if(
                surface.commands.begin(),
                surface.commands.end(),
                [](const DrawCommand &command)
                {
                    return command.type ==
                               DrawCommandType::
                                   FillRoundedRectangle &&
                           command.bounds.x >=
                               AgendaUiLayout::T9GridX &&
                           command.bounds.y >=
                               AgendaUiLayout::T9GridY &&
                           command.bounds.y < 190;
                }));

    assert(keyCount == 12);
}

void test_t9_editor_renders_english_numbers_and_case_state()
{
    AgendaUiView view;
    view.screen =
        AgendaUiScreen::EditorText;

    view.t9Language =
        T9Language::English;

    view.t9Mode =
        T9Mode::Numbers;

    view.t9Uppercase = true;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "EN"));

    assert(
        containsText(
            surface,
            "ΑΡΙΘΜΟΙ"));

    assert(
        containsText(
            surface,
            "ΚΕΦΑΛΑΙΑ"));

    assert(
        containsText(
            surface,
            "2"));

    assert(
        containsText(
            surface,
            "9"));

    assert(
        containsText(
            surface,
            "0"));
}

void test_t9_space_key_label_follows_input_mode()
{
    constexpr UiRect spaceKey{
        100,
        166,
        50,
        22};

    AgendaUiView view;
    view.screen =
        AgendaUiScreen::EditorText;
    view.t9Mode =
        T9Mode::Letters;
    view.t9Language =
        T9Language::Greek;

    RecordingDrawSurface greekSurface;

    AgendaRenderer{}.render(
        view,
        greekSurface);

    assert(
        hasButton(
            greekSurface,
            spaceKey,
            "ΚΕΝΟ"));

    view.t9Language =
        T9Language::English;

    RecordingDrawSurface englishSurface;

    AgendaRenderer{}.render(
        view,
        englishSurface);

    assert(
        hasButton(
            englishSurface,
            spaceKey,
            "SPACE"));

    view.t9Mode =
        T9Mode::Numbers;

    RecordingDrawSurface numberSurface;

    AgendaRenderer{}.render(
        view,
        numberSurface);

    assert(
        hasButton(
            numberSurface,
            spaceKey,
            "0"));
}

void test_t9_editor_preview_is_utf8_safe()
{
    AgendaUiView view;
    view.screen =
        AgendaUiScreen::EditorText;

    view.editor.text =
        "Αυτό το κείμενο είναι αρκετά μεγάλο ώστε "
        "να χρειάζεται ασφαλή περικοπή χαρακτήρων";

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    const std::string expected =
        utf8Prefix(
            view.editor.text,
            48);

    const DrawCommand *preview =
        findTextCommand(
            surface,
            expected);

    assert(preview != nullptr);
    assert(isValidUtf8(preview->text));

    assert(
        utf8CodePointCount(
            preview->text) <= 48);
}

void test_reminder_overlay_is_drawn_after_base_screen()
{
    AgendaUiView view =
        septemberMonthView();

    view.overlay =
        AgendaUiOverlay::Reminder;

    view.reminder = Event{
        7,
        Date{2026, 9, 10},
        TimeOfDay{8, 5},
        "Ιατρικό ραντεβού",
        false,
        DateTime{
            Date{2026, 9, 9},
            TimeOfDay{10, 0}}};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    const auto scrim =
        std::find_if(
            surface.commands.begin(),
            surface.commands.end(),
            [](const DrawCommand &command)
            {
                return command.type ==
                           DrawCommandType::
                               FillRectangle &&
                       command.bounds ==
                           AgendaUiLayout::
                               FullDisplay &&
                       command.color ==
                           AgendaUiColors::Overlay;
            });

    assert(
        scrim !=
        surface.commands.end());

    const std::size_t scrimIndex =
        static_cast<std::size_t>(
            scrim -
            surface.commands.begin());

    const DrawCommand *title =
        findTextCommand(
            surface,
            "ΥΠΕΝΘΥΜΙΣΗ");

    const DrawCommand *text =
        findTextCommand(
            surface,
            "Ιατρικό ραντεβού");

    const DrawCommand *confirm =
        findTextCommand(
            surface,
            "ΕΝΤΑΞΕΙ");

    assert(
        commandIndex(
            surface,
            title) > scrimIndex);

    assert(
        commandIndex(
            surface,
            text) > scrimIndex);

    assert(
        commandIndex(
            surface,
            confirm) > scrimIndex);
}

void test_confirmation_overlays_render_both_choices()
{
    AgendaUiView view =
        septemberMonthView();

    view.overlay =
        AgendaUiOverlay::ConfirmDelete;

    RecordingDrawSurface deleteSurface;

    AgendaRenderer{}.render(
        view,
        deleteSurface);

    assert(
        containsText(
            deleteSurface,
            "ΔΙΑΓΡΑΦΗ;"));

    assert(
        containsText(
            deleteSurface,
            "ΝΑΙ"));

    assert(
        containsText(
            deleteSurface,
            "ΟΧΙ"));

    view.overlay =
        AgendaUiOverlay::ConfirmDiscard;

    RecordingDrawSurface discardSurface;

    AgendaRenderer{}.render(
        view,
        discardSurface);

    assert(
        containsText(
            discardSurface,
            "ΑΠΟΡΡΙΨΗ ΑΛΛΑΓΩΝ;"));

    assert(
        containsText(
            discardSurface,
            "ΝΑΙ"));

    assert(
        containsText(
            discardSurface,
            "ΟΧΙ"));
}

void test_operation_and_clock_errors_render_safe_messages()
{
    AgendaUiView view =
        septemberMonthView();

    view.overlay =
        AgendaUiOverlay::OperationError;

    view.controllerError =
        AgendaControllerError::WriteFailed;

    RecordingDrawSurface operationSurface;

    AgendaRenderer{}.render(
        view,
        operationSurface);

    assert(
        containsText(
            operationSurface,
            "ΑΠΟΤΥΧΙΑ ΑΠΟΘΗΚΕΥΣΗΣ"));

    assert(
        containsText(
            operationSurface,
            "ΞΑΝΑ"));

    assert(
        containsText(
            operationSurface,
            "ΑΚΥΡΩΣΗ"));

    view.controllerError =
        AgendaControllerError::None;

    view.eventError =
        EventError::EmptyText;

    RecordingDrawSurface validationSurface;

    AgendaRenderer{}.render(
        view,
        validationSurface);

    assert(
        containsText(
            validationSurface,
            "ΚΕΝΟ ΚΕΙΜΕΝΟ"));

    view.overlay =
        AgendaUiOverlay::ClockError;

    view.eventError =
        EventError::None;

    view.clockError =
        ClockError::InvalidDateTime;

    RecordingDrawSurface clockSurface;

    AgendaRenderer{}.render(
        view,
        clockSurface);

    assert(
        containsText(
            clockSurface,
            "ΜΗ ΕΓΚΥΡΗ ΩΡΑ"));
}

void test_unknown_screen_renders_safe_error()
{
    AgendaUiView view;

    view.screen =
        static_cast<AgendaUiScreen>(
            999);

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    assert(
        containsText(
            surface,
            "ΣΦΑΛΜΑ ΠΡΟΒΟΛΗΣ"));

    assert(
        allCommandsInBounds(
            surface));
}

void test_missing_required_values_render_safe_errors()
{
    AgendaUiView dayView;

    dayView.screen =
        AgendaUiScreen::Day;

    RecordingDrawSurface daySurface;

    AgendaRenderer{}.render(
        dayView,
        daySurface);

    assert(
        containsText(
            daySurface,
            "ΗΜΕΡΟΜΗΝΙΑ ΜΗ ΔΙΑΘΕΣΙΜΗ"));

    AgendaUiView reminderView =
        septemberMonthView();

    reminderView.overlay =
        AgendaUiOverlay::Reminder;

    reminderView.reminder.reset();

    RecordingDrawSurface reminderSurface;

    AgendaRenderer{}.render(
        reminderView,
        reminderSurface);

    assert(
        containsText(
            reminderSurface,
            "ΥΠΕΝΘΥΜΙΣΗ ΜΗ ΔΙΑΘΕΣΙΜΗ"));
}

void test_every_supported_screen_stays_in_bounds()
{
    AgendaUiView view =
        septemberMonthView();

    view.clockDraft = DateTime{
        Date{2026, 9, 9},
        TimeOfDay{10, 0}};

    view.selectedDate =
        Date{2026, 9, 9};

    view.selectedEvent = Event{
        7,
        Date{2026, 9, 10},
        std::nullopt,
        "Γεγονός",
        false,
        DateTime{
            Date{2026, 9, 9},
            TimeOfDay{10, 0}}};

    view.editor.date =
        Date{2026, 9, 10};

    view.editor.time =
        TimeOfDay{8, 5};

    view.editor.text =
        "Γεγονός";

    const std::array<
        AgendaUiScreen,
        9>
        screens{
            AgendaUiScreen::ClockSetup,
            AgendaUiScreen::Month,
            AgendaUiScreen::Day,
            AgendaUiScreen::EventDetails,
            AgendaUiScreen::EditorDate,
            AgendaUiScreen::EditorTime,
            AgendaUiScreen::EditorText,
            AgendaUiScreen::EditorReview,
            AgendaUiScreen::StartupError};

    for (
        AgendaUiScreen screen :
        screens)
    {
        view.screen = screen;

        RecordingDrawSurface surface;

        AgendaRenderer{}.render(
            view,
            surface);

        assert(
            allCommandsInBounds(
                surface));
    }
}

void test_day_renders_visible_back_and_create_buttons()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Day;

    view.selectedDate =
        Date{2026, 9, 10};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    const DrawCommand *back =
        findTextCommand(surface, "<");
    const DrawCommand *create =
        findTextCommand(surface, "+");

    assert(back != nullptr);
    assert(create != nullptr);

    assert(
        hasButton(
            surface,
            back->bounds,
            "<"));

    assert(
        hasButton(
            surface,
            create->bounds,
            "+"));

    assert(
        rectCenterIsVisibleOnRoundDisplay(
            back->bounds));
    assert(
        rectCenterIsVisibleOnRoundDisplay(
            create->bounds));
}

void test_editor_date_renders_visible_day_buttons()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorDate;

    view.editor.date =
        Date{2026, 9, 10};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    constexpr UiRect previousDayButton{
        20,
        67,
        28,
        34};

    constexpr UiRect nextDayButton{
        192,
        67,
        28,
        34};

    assert(
        hasButton(
            surface,
            previousDayButton,
            "<"));

    assert(
        hasButton(
            surface,
            nextDayButton,
            ">"));
}

void test_editor_time_renders_mode_and_time_buttons()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorTime;

    view.editor.time =
        TimeOfDay{8, 30};

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    constexpr UiRect timedMode{
        28,
        58,
        88,
        32};

    constexpr UiRect allDayMode{
        124,
        58,
        88,
        32};

    constexpr UiRect hourUp{
        42,
        110,
        56,
        18};

    constexpr UiRect hourDown{
        42,
        158,
        56,
        18};

    constexpr UiRect minuteUp{
        142,
        110,
        56,
        18};

    constexpr UiRect minuteDown{
        142,
        158,
        56,
        18};

    assert(
        hasButton(
            surface,
            timedMode,
            "ΜΕ ΩΡΑ"));

    assert(
        hasButton(
            surface,
            allDayMode,
            "ΟΛΗ ΜΕΡΑ"));

    assert(hasButton(surface, hourUp, "+"));
    assert(hasButton(surface, hourDown, "-"));
    assert(hasButton(surface, minuteUp, "+"));
    assert(hasButton(surface, minuteDown, "-"));
}

void test_t9_editor_renders_language_key()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorText;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    constexpr UiRect languageKey{
        45,
        166,
        50,
        22};

    assert(
        hasButton(
            surface,
            languageKey,
            "EL/EN"));
}

void test_t9_editor_renders_backspace_key()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorText;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface);

    constexpr UiRect backspaceKey{
        155,
        166,
        50,
        22};

    assert(
        hasButton(
            surface,
            backspaceKey,
            "<"));
}

int main()
{
    test_renderer_clears_with_dark_background();
    test_renderer_does_not_change_input_view();
    test_startup_error_shows_retry_action();
    test_clock_setup_formats_date_and_time();
    test_clock_setup_renders_all_adjustment_buttons();
    test_clock_setup_uses_only_in_bounds_commands();
    test_month_renders_title_weekdays_and_all_cells();
    test_month_renders_selected_today_and_event_states();
    test_month_renders_disabled_navigation_and_adjacent_dates();
    test_month_navigation_is_visible_on_round_display();
    test_day_renders_three_rows_and_page_state();
    test_day_renders_visible_back_and_create_buttons();
    test_day_uses_all_day_label_and_utf8_safe_preview();
    test_event_details_render_event_or_safe_error();
    test_editor_date_renders_draft_and_navigation();
    test_editor_date_renders_visible_day_buttons();
    test_editor_time_renders_timed_and_all_day_variants();
    test_editor_time_renders_mode_and_time_buttons();
    test_editor_review_renders_create_and_update_variants();
    test_t9_editor_renders_greek_letters_and_twelve_keys();
    test_t9_editor_renders_english_numbers_and_case_state();
    test_t9_space_key_label_follows_input_mode();
    test_t9_editor_preview_is_utf8_safe();
    test_reminder_overlay_is_drawn_after_base_screen();
    test_confirmation_overlays_render_both_choices();
    test_operation_and_clock_errors_render_safe_messages();
    test_unknown_screen_renders_safe_error();
    test_missing_required_values_render_safe_errors();
    test_every_supported_screen_stays_in_bounds();
    test_t9_editor_renders_language_key();
    test_t9_editor_renders_backspace_key();

    std::cout
        << "All agenda renderer tests passed.\n";

    return 0;
}