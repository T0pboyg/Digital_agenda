#include <cstddef>
#include <array>

#include "agenda_touch_mapper.h"
#include "alarm_renderer.h"

namespace
{
    bool containsPoint(
        const UiRect &bounds,
        UiPoint point)
    {
        return point.x >= bounds.x &&
               point.x < bounds.x + bounds.width &&
               point.y >= bounds.y &&
               point.y < bounds.y + bounds.height;
    }

    AgendaUiAction makeAction(
        AgendaUiActionType type)
    {
        AgendaUiAction action;
        action.type = type;

        return action;
    }
}

std::optional<AgendaUiAction> AgendaTouchMapper::map(
    const AgendaUiView &view,
    UiPoint releasePoint,
    std::uint32_t timestampMilliseconds) const
{

    if (view.overlay == AgendaUiOverlay::Alarm || view.screen == AgendaUiScreen::AlarmList || view.screen == AgendaUiScreen::AlarmEditor)
        return mapAlarmTouch(view, releasePoint);
    if (view.alarmFeature && view.screen == AgendaUiScreen::Month && view.overlay == AgendaUiOverlay::None && containsPoint(AlarmLayout::Open, releasePoint))
        return makeAction(AgendaUiActionType::OpenAlarms);

    if (view.overlay != AgendaUiOverlay::None)
    {
        if (view.overlay == AgendaUiOverlay::Reminder)
        {
            constexpr UiRect okButton{
                82,
                154,
                76,
                28};

            if (containsPoint(okButton, releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::AcknowledgeReminder);
            }

            return std::nullopt;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::ConfirmDelete)
        {
            constexpr UiRect yesButton{
                48,
                144,
                60,
                30};

            constexpr UiRect noButton{
                132,
                144,
                60,
                30};

            if (containsPoint(yesButton, releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::ConfirmDelete);
            }

            if (containsPoint(noButton, releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::CancelDelete);
            }

            return std::nullopt;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::ConfirmDiscard)
        {
            constexpr UiRect yesButton{
                48,
                144,
                60,
                30};

            constexpr UiRect noButton{
                132,
                144,
                60,
                30};

            if (containsPoint(yesButton, releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::ConfirmDiscard);
            }

            if (containsPoint(noButton, releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::CancelDiscard);
            }

            return std::nullopt;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::OperationError)
        {
            constexpr UiRect retryButton{
                44,
                148,
                70,
                30};

            constexpr UiRect cancelButton{
                126,
                148,
                70,
                30};

            if (
                containsPoint(
                    retryButton,
                    releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::RetryOperation);
            }

            if (
                containsPoint(
                    cancelButton,
                    releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::CancelOperation);
            }

            return std::nullopt;
        }

        if (
            view.overlay ==
            AgendaUiOverlay::ClockError)
        {
            constexpr UiRect okButton{
                82,
                150,
                76,
                30};

            if (
                containsPoint(
                    okButton,
                    releasePoint))
            {
                return makeAction(
                    AgendaUiActionType::CancelOperation);
            }

            return std::nullopt;
        }

        return std::nullopt;
    }

    if (view.screen == AgendaUiScreen::ClockSetup)
    {
        if (
            containsPoint(
                AgendaUiLayout::ClockDatePrevious,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ClockPreviousDay);
        }

        if (
            containsPoint(
                AgendaUiLayout::ClockDateNext,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ClockNextDay);
        }

        if (
            containsPoint(
                AgendaUiLayout::ClockHourUp,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ClockNextHour);
        }

        if (
            containsPoint(
                AgendaUiLayout::ClockHourDown,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ClockPreviousHour);
        }

        if (
            containsPoint(
                AgendaUiLayout::ClockMinuteUp,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ClockNextMinute);
        }

        if (
            containsPoint(
                AgendaUiLayout::ClockMinuteDown,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ClockPreviousMinute);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterCenter,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ConfirmClock);
        }

        return std::nullopt;
    }

    if (view.screen == AgendaUiScreen::StartupError)
    {
        if (
            containsPoint(
                AgendaUiLayout::FooterCenter,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::RetryStartup);
        }

        return std::nullopt;
    }

    if (view.screen == AgendaUiScreen::Day)
    {
        if (
            containsPoint(
                AgendaUiLayout::DayBack,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::Back);
        }

        if (
            containsPoint(
                AgendaUiLayout::DayCreate,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::StartCreate);
        }
        if (
            view.dayPreviousEnabled &&
            containsPoint(
                AgendaUiLayout::FooterLeft,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::PreviousDayPage);
        }

        if (
            view.dayNextEnabled &&
            containsPoint(
                AgendaUiLayout::FooterRight,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::NextDayPage);
        }

        constexpr std::array<UiRect, 3> DAY_ROWS{
            AgendaUiLayout::DayRow0,
            AgendaUiLayout::DayRow1,
            AgendaUiLayout::DayRow2};

        for (
            std::size_t index = 0;
            index < DAY_ROWS.size() &&
            index < view.dayEvents.size();
            ++index)
        {
            if (
                !containsPoint(
                    DAY_ROWS[index],
                    releasePoint))
            {
                continue;
            }

            AgendaUiAction action =
                makeAction(
                    AgendaUiActionType::SelectEvent);

            action.eventId =
                view.dayEvents[index].eventId;

            return action;
        }

        return std::nullopt;
    }

    if (
        view.screen ==
        AgendaUiScreen::EventDetails)
    {
        if (
            containsPoint(
                AgendaUiLayout::FooterLeft,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::Back);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterCenter,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::StartEdit);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterRight,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::RequestDelete);
        }

        return std::nullopt;
    }

    if (
        view.screen ==
        AgendaUiScreen::EditorDate)
    {
        if (
            containsPoint(
                AgendaUiLayout::EditorDatePrevious,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorPreviousDay);
        }

        if (
            containsPoint(
                AgendaUiLayout::EditorDateNext,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorNextDay);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterLeft,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorBack);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterCenter,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorCancel);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterRight,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorNext);
        }

        return std::nullopt;
    }

    if (view.screen == AgendaUiScreen::EditorTime)
    {
        const bool timed =
            view.editor.time.has_value();

        if (
            !timed &&
            containsPoint(
                AgendaUiLayout::EditorTimedMode,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ToggleEventTime);
        }

        if (
            timed &&
            containsPoint(
                AgendaUiLayout::EditorAllDayMode,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ToggleEventTime);
        }

        if (
            timed &&
            containsPoint(
                AgendaUiLayout::EditorHourUp,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorNextHour);
        }

        if (
            timed &&
            containsPoint(
                AgendaUiLayout::EditorHourDown,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorPreviousHour);
        }

        if (
            timed &&
            containsPoint(
                AgendaUiLayout::EditorMinuteUp,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorNextMinute);
        }

        if (
            timed &&
            containsPoint(
                AgendaUiLayout::EditorMinuteDown,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorPreviousMinute);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterLeft,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorBack);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterCenter,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorCancel);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterRight,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorNext);
        }

        return std::nullopt;
    }

    if (view.screen == AgendaUiScreen::EditorText)
    {
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

        if (containsPoint(languageBounds, releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ToggleT9Language);
        }

        if (containsPoint(modeBounds, releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ToggleT9Mode);
        }

        if (containsPoint(caseBounds, releasePoint))
        {
            return makeAction(
                AgendaUiActionType::ToggleT9Case);
        }

        constexpr std::array<char, 12> digits{
            '1',
            '2',
            '3',
            '4',
            '5',
            '6',
            '7',
            '8',
            '9',
            '*',
            '0',
            '#'};

        for (
            std::size_t index = 0;
            index < digits.size();
            ++index)
        {
            const int column =
                static_cast<int>(index % 3);

            const int row =
                static_cast<int>(index / 3);

            const UiRect bounds{
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

            if (containsPoint(bounds, releasePoint))
            {
                if (index == 9)
                {
                    return makeAction(
                        AgendaUiActionType::ToggleT9Language);
                }

                if (index == 11)
                {
                    return makeAction(
                        AgendaUiActionType::T9Backspace);
                }

                AgendaUiAction action =
                    makeAction(
                        AgendaUiActionType::T9Digit);

                action.digit = digits[index];

                action.timestampMilliseconds =
                    timestampMilliseconds;

                return action;
            }
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterLeft,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorBack);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterRight,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorNext);
        }

        return std::nullopt;
    }

    if (view.screen == AgendaUiScreen::EditorReview)
    {
        if (
            containsPoint(
                AgendaUiLayout::FooterLeft,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::EditorBack);
        }

        if (
            containsPoint(
                AgendaUiLayout::FooterRight,
                releasePoint))
        {
            return makeAction(
                AgendaUiActionType::SaveEditor);
        }

        return std::nullopt;
    }

    if (view.screen != AgendaUiScreen::Month)
    {
        return std::nullopt;
    }

    if (
        view.monthPreviousEnabled &&
        containsPoint(
            AgendaUiLayout::MonthPrevious,
            releasePoint))
    {
        return makeAction(
            AgendaUiActionType::PreviousMonth);
    }

    if (
        view.monthNextEnabled &&
        containsPoint(
            AgendaUiLayout::MonthNext,
            releasePoint))
    {
        return makeAction(
            AgendaUiActionType::NextMonth);
    }

    if (
        !containsPoint(
            AgendaUiLayout::MonthGrid,
            releasePoint))
    {
        return std::nullopt;
    }

    const int column =
        (releasePoint.x -
         AgendaUiLayout::MonthGrid.x) /
        AgendaUiLayout::MonthCellWidth;

    const int row =
        (releasePoint.y -
         AgendaUiLayout::MonthGrid.y) /
        AgendaUiLayout::MonthCellHeight;

    const std::size_t index =
        static_cast<std::size_t>(
            row * 7 + column);

    const AgendaUiMonthCell &cell =
        view.monthCells[index];

    if (!cell.date.has_value())
    {
        return std::nullopt;
    }

    AgendaUiAction action =
        makeAction(
            AgendaUiActionType::SelectDate);

    action.date = cell.date;

    return action;
}