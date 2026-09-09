#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>

#include "core/calendar_engine.h"
#include "ui/agenda_renderer.h"

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

class RecordingDrawSurface final :
    public DrawSurface
{
public:
    std::vector<DrawCommand> commands;

    void clear(
        UiColor color
    ) override
    {
        DrawCommand command;

        command.type =
            DrawCommandType::Clear;

        command.color = color;

        commands.push_back(command);
    }

    void fillRectangle(
        const UiRect& bounds,
        UiColor color
    ) override
    {
        recordRectangle(
            DrawCommandType::FillRectangle,
            bounds,
            0,
            0,
            color
        );
    }

    void drawRectangle(
        const UiRect& bounds,
        UiColor color,
        int thickness
    ) override
    {
        recordRectangle(
            DrawCommandType::DrawRectangle,
            bounds,
            0,
            thickness,
            color
        );
    }

    void fillRoundedRectangle(
        const UiRect& bounds,
        int radius,
        UiColor color
    ) override
    {
        recordRectangle(
            DrawCommandType::
                FillRoundedRectangle,
            bounds,
            radius,
            0,
            color
        );
    }

    void drawRoundedRectangle(
        const UiRect& bounds,
        int radius,
        UiColor color,
        int thickness
    ) override
    {
        recordRectangle(
            DrawCommandType::
                DrawRoundedRectangle,
            bounds,
            radius,
            thickness,
            color
        );
    }

    void fillCircle(
        UiPoint center,
        int radius,
        UiColor color
    ) override
    {
        recordCircle(
            DrawCommandType::FillCircle,
            center,
            radius,
            0,
            color
        );
    }

    void drawCircle(
        UiPoint center,
        int radius,
        UiColor color,
        int thickness
    ) override
    {
        recordCircle(
            DrawCommandType::DrawCircle,
            center,
            radius,
            thickness,
            color
        );
    }

    void drawLine(
        UiPoint start,
        UiPoint end,
        UiColor color,
        int thickness
    ) override
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
        const UiRect& bounds,
        std::string_view text,
        UiTextStyle style,
        UiColor color,
        UiTextAlignment alignment
    ) override
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
        const UiRect& bounds,
        int radius,
        int thickness,
        UiColor color
    )
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
        UiColor color
    )
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
    const RecordingDrawSurface& surface,
    std::string_view expected
)
{
    return std::any_of(
        surface.commands.begin(),
        surface.commands.end(),
        [expected](
            const DrawCommand& command
        )
        {
            return
                command.type ==
                    DrawCommandType::DrawText &&
                command.text == expected;
        }
    );
}

bool containsTextWithColor(
    const RecordingDrawSurface& surface,
    std::string_view expected,
    UiColor color
)
{
    return std::any_of(
        surface.commands.begin(),
        surface.commands.end(),
        [expected, color](
            const DrawCommand& command
        )
        {
            return
                command.type ==
                    DrawCommandType::DrawText &&
                command.text == expected &&
                command.color == color;
        }
    );
}

bool rectInside(
    const UiRect& inner,
    const UiRect& outer
)
{
    return
        inner.x >= outer.x &&
        inner.y >= outer.y &&
        inner.x + inner.width <=
            outer.x + outer.width &&
        inner.y + inner.height <=
            outer.y + outer.height;
}

bool pointInDisplay(
    UiPoint value
)
{
    return
        value.x >= 0 &&
        value.y >= 0 &&
        value.x <
            AGENDA_UI_DISPLAY_WIDTH &&
        value.y <
            AGENDA_UI_DISPLAY_HEIGHT;
}

bool rectInDisplay(
    const UiRect& value
)
{
    return
        value.x >= 0 &&
        value.y >= 0 &&
        value.width >= 0 &&
        value.height >= 0 &&
        value.x + value.width <=
            AGENDA_UI_DISPLAY_WIDTH &&
        value.y + value.height <=
            AGENDA_UI_DISPLAY_HEIGHT;
}

bool commandInDisplay(
    const DrawCommand& command
)
{
    if (
        command.type ==
        DrawCommandType::Clear
    )
    {
        return true;
    }

    if (
        command.type ==
        DrawCommandType::DrawLine
    )
    {
        return
            pointInDisplay(
                command.start
            ) &&
            pointInDisplay(
                command.end
            );
    }

    if (
        command.type ==
            DrawCommandType::FillCircle ||
        command.type ==
            DrawCommandType::DrawCircle
    )
    {
        return
            command.start.x -
                command.radius >= 0 &&
            command.start.y -
                command.radius >= 0 &&
            command.start.x +
                command.radius <
                AGENDA_UI_DISPLAY_WIDTH &&
            command.start.y +
                command.radius <
                AGENDA_UI_DISPLAY_HEIGHT;
    }

    return rectInDisplay(
        command.bounds
    );
}

bool allCommandsInBounds(
    const RecordingDrawSurface& surface
)
{
    return std::all_of(
        surface.commands.begin(),
        surface.commands.end(),
        commandInDisplay
    );
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
        DrawCommandType::Clear
    );

    assert(
        surface.commands.front().color ==
        AgendaUiColors::Background
    );
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
        AgendaUiScreen::Day
    );

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
        surface
    );

    assert(
        containsText(
            surface,
            "ΣΦΑΛΜΑ ΕΚΚΙΝΗΣΗΣ"
        )
    );

    assert(
        containsText(
            surface,
            "ΑΠΟΤΥΧΙΑ ΑΝΑΓΝΩΣΗΣ"
        )
    );

    assert(
        containsText(
            surface,
            "ΞΑΝΑ"
        )
    );
}

void test_clock_setup_formats_date_and_time()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::ClockSetup;

    view.clockDraft = DateTime{
        Date{2026, 9, 9},
        TimeOfDay{7, 5}
    };

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface
    );

    assert(
        containsText(
            surface,
            "ΡΥΘΜΙΣΗ ΩΡΑΣ"
        )
    );

    assert(
        containsText(
            surface,
            "09/09/2026"
        )
    );

    assert(
        containsText(
            surface,
            "07:05"
        )
    );

    assert(
        containsText(
            surface,
            "ΕΝΤΑΞΕΙ"
        )
    );
}

void test_clock_setup_uses_only_in_bounds_commands()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::ClockSetup;

    RecordingDrawSurface surface;

    AgendaRenderer{}.render(
        view,
        surface
    );

    assert(
        allCommandsInBounds(surface)
    );
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
            view.displayedMonth
        );

    for (
        std::size_t index = 0;
        index < grid.size();
        ++index
    )
    {
        view.monthCells[index].date =
            grid[index].date;

        view.monthCells[index].
            inDisplayedMonth =
                grid[index].
                    inDisplayedMonth;
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
        surface
    );

    assert(
        containsText(
            surface,
            "ΣΕΠΤΕΜΒΡΙΟΣ 2026"
        )
    );

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
                    const DrawCommand& command
                )
                {
                    return
                        command.type ==
                            DrawCommandType::
                                DrawText &&
                        rectInside(
                            command.bounds,
                            AgendaUiLayout::
                                MonthGrid
                        );
                }
            )
        );

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
        surface
    );

    const std::size_t accentFills =
        static_cast<std::size_t>(
            std::count_if(
                surface.commands.begin(),
                surface.commands.end(),
                [](
                    const DrawCommand& command
                )
                {
                    return
                        command.type ==
                            DrawCommandType::
                                FillCircle &&
                        command.color ==
                            AgendaUiColors::
                                Accent;
                }
            )
        );

    const bool hasTodayOutline =
        std::any_of(
            surface.commands.begin(),
            surface.commands.end(),
            [](
                const DrawCommand& command
            )
            {
                return
                    command.type ==
                        DrawCommandType::
                            DrawCircle &&
                    command.color ==
                        AgendaUiColors::
                            Accent;
            }
        );

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
        surface
    );

    assert(
        containsTextWithColor(
            surface,
            "<",
            AgendaUiColors::Disabled
        )
    );

    assert(
        containsTextWithColor(
            surface,
            ">",
            AgendaUiColors::PrimaryText
        )
    );

    assert(
        containsTextWithColor(
            surface,
            "31",
            AgendaUiColors::Disabled
        )
    );
}

int main()
{
    test_renderer_clears_with_dark_background();
    test_renderer_does_not_change_input_view();
    test_startup_error_shows_retry_action();
    test_clock_setup_formats_date_and_time();
    test_clock_setup_uses_only_in_bounds_commands();
    test_month_renders_title_weekdays_and_all_cells();
    test_month_renders_selected_today_and_event_states();
    test_month_renders_disabled_navigation_and_adjacent_dates();

    std::cout
        << "All agenda renderer tests passed.\n";

    return 0;
}