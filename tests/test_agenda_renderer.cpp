#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

int main()
{
    test_renderer_clears_with_dark_background();
    test_renderer_does_not_change_input_view();

    std::cout
        << "All agenda renderer tests passed.\n";

    return 0;
}