#include <cassert>
#include <iostream>

#include "ui/agenda_touch_mapper.h"

void test_month_previous_arrow_maps_to_previous_month()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Month;

    view.monthPreviousEnabled = true;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{34, 22},
            100);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::PreviousMonth);
}

void test_month_next_arrow_maps_to_next_month()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Month;

    view.monthNextEnabled = true;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{206, 22},
            200);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::NextMonth);
}

void test_month_cell_maps_to_its_actual_date()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Month;

    view.monthCells[17].date =
        Date{2026, 9, 17};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{120, 108},
            300);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::SelectDate);

    assert(action->date.has_value());
    assert(action->date->year == 2026);
    assert(action->date->month == 9);
    assert(action->date->day == 17);
}

void test_day_previous_page_maps_when_enabled()
{
    AgendaUiView view;

    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 17};
    view.dayPageIndex = 1;
    view.dayPageCount = 2;
    view.dayPreviousEnabled = true;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{52, 212},
            400);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::PreviousDayPage);
}

void test_day_next_page_maps_when_enabled()
{
    AgendaUiView view;

    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 17};
    view.dayPageIndex = 0;
    view.dayPageCount = 2;
    view.dayNextEnabled = true;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{188, 212},
            500);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::NextDayPage);
}

void test_day_event_row_maps_to_event_id()
{
    AgendaUiView view;

    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 17};

    AgendaUiEventRow row;

    row.eventId = 42;
    row.time = TimeOfDay{8, 30};
    row.text = "Meeting";

    view.dayEvents.push_back(row);

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{120, 73},
            600);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::SelectEvent);

    assert(action->eventId.has_value());
    assert(*action->eventId == 42);
}

void test_day_back_button_maps_to_back()
{
    AgendaUiView view;

    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 17};

    const UiRect bounds =
        AgendaUiLayout::DayBack;

    const UiPoint center{
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height / 2};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            center,
            700);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::Back);
}

void test_day_create_button_maps_to_start_create()
{
    AgendaUiView view;

    view.screen = AgendaUiScreen::Day;
    view.selectedDate = Date{2026, 9, 17};

    const UiRect bounds =
        AgendaUiLayout::DayCreate;

    const UiPoint center{
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height / 2};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            center,
            800);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::StartCreate);
}

AgendaUiView eventDetailsView()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EventDetails;

    view.selectedEvent = Event{
        42,
        Date{2026, 9, 17},
        TimeOfDay{8, 30},
        "Meeting",
        false,
        DateTime{
            Date{2026, 9, 10},
            TimeOfDay{12, 0}}};

    return view;
}

void test_event_details_back_maps_to_back()
{
    const AgendaUiView view =
        eventDetailsView();

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{52, 212},
            900);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::Back);
}

void test_event_details_edit_maps_to_start_edit()
{
    const AgendaUiView view =
        eventDetailsView();

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{120, 212},
            1000);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::StartEdit);
}

void test_event_details_delete_maps_to_request_delete()
{
    const AgendaUiView view =
        eventDetailsView();

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{188, 212},
            1100);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::RequestDelete);
}

void test_editor_date_previous_maps_to_previous_day()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorDate;

    view.editor.date =
        Date{2026, 9, 17};

    const UiRect bounds =
        AgendaUiLayout::EditorDatePrevious;

    const UiPoint center{
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height / 2};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            center,
            1200);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::EditorPreviousDay);
}

void test_editor_date_next_maps_to_next_day()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorDate;

    view.editor.date =
        Date{2026, 9, 17};

    const UiRect bounds =
        AgendaUiLayout::EditorDateNext;

    const UiPoint center{
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height / 2};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            center,
            1300);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::EditorNextDay);
}

void test_editor_date_next_stage_maps_to_editor_next()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorDate;

    view.editor.date =
        Date{2026, 9, 17};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{188, 212},
            1400);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::EditorNext);
}

void test_editor_date_back_maps_to_editor_back()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorDate;

    view.editor.date =
        Date{2026, 9, 17};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{52, 212},
            1500);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::EditorBack);
}

void test_editor_date_cancel_maps_to_editor_cancel()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorDate;

    view.editor.date =
        Date{2026, 9, 17};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{120, 212},
            1600);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::EditorCancel);
}

void test_editor_time_all_day_mode_maps_to_toggle()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = TimeOfDay{8, 30};

    const UiRect bounds =
        AgendaUiLayout::EditorAllDayMode;

    const UiPoint center{
        bounds.x + bounds.width / 2,
        bounds.y + bounds.height / 2};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            center,
            1700);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ToggleEventTime);
}

void test_editor_time_hour_buttons_map_to_hour_changes()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = TimeOfDay{8, 30};

    const UiRect upBounds =
        AgendaUiLayout::EditorHourUp;

    const UiRect downBounds =
        AgendaUiLayout::EditorHourDown;

    const auto upAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                upBounds.x + upBounds.width / 2,
                upBounds.y + upBounds.height / 2},
            1800);

    const auto downAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                downBounds.x + downBounds.width / 2,
                downBounds.y + downBounds.height / 2},
            1900);

    assert(upAction.has_value());
    assert(
        upAction->type ==
        AgendaUiActionType::EditorNextHour);

    assert(downAction.has_value());
    assert(
        downAction->type ==
        AgendaUiActionType::EditorPreviousHour);
}

void test_editor_time_minute_buttons_map_to_minute_changes()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = TimeOfDay{8, 30};

    const UiRect upBounds =
        AgendaUiLayout::EditorMinuteUp;

    const UiRect downBounds =
        AgendaUiLayout::EditorMinuteDown;

    const auto upAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                upBounds.x + upBounds.width / 2,
                upBounds.y + upBounds.height / 2},
            2000);

    const auto downAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                downBounds.x + downBounds.width / 2,
                downBounds.y + downBounds.height / 2},
            2100);

    assert(upAction.has_value());
    assert(
        upAction->type ==
        AgendaUiActionType::EditorNextMinute);

    assert(downAction.has_value());
    assert(
        downAction->type ==
        AgendaUiActionType::EditorPreviousMinute);
}

void test_editor_time_timed_mode_enables_time_for_all_day_event()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = std::nullopt;

    const UiRect bounds =
        AgendaUiLayout::EditorTimedMode;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            2200);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ToggleEventTime);
}

void test_editor_time_back_button_maps_to_editor_back()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = TimeOfDay{8, 30};

    const UiRect bounds =
        AgendaUiLayout::FooterLeft;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            2300);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::EditorBack);
}

void test_editor_time_cancel_button_maps_to_editor_cancel()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = TimeOfDay{8, 30};

    const UiRect bounds =
        AgendaUiLayout::FooterCenter;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            2400);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::EditorCancel);
}

void test_editor_time_next_button_maps_to_editor_next()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorTime;
    view.editor.time = TimeOfDay{8, 30};

    const UiRect bounds =
        AgendaUiLayout::FooterRight;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            2500);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::EditorNext);
}

void test_editor_review_back_button_maps_to_editor_back()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorReview;

    const UiRect bounds =
        AgendaUiLayout::FooterLeft;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            2600);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::EditorBack);
}

void test_editor_review_save_button_maps_to_save_editor()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorReview;

    const UiRect bounds =
        AgendaUiLayout::FooterRight;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            2700);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::SaveEditor);
}

void test_editor_text_key_two_maps_to_t9_digit_action()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;

    const UiPoint keyTwoCenter{
        AgendaUiLayout::T9GridX +
            AgendaUiLayout::T9KeyWidth +
            AgendaUiLayout::T9ColumnGap +
            AgendaUiLayout::T9KeyWidth / 2,
        AgendaUiLayout::T9GridY +
            AgendaUiLayout::T9KeyHeight / 2};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            keyTwoCenter,
            2800);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::T9Digit);
    assert(action->digit == '2');
    assert(
        action->timestampMilliseconds ==
        2800U);
}

void test_editor_text_back_button_maps_to_editor_back()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;

    const UiRect bounds =
        AgendaUiLayout::FooterLeft;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            2900);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::EditorBack);
}

void test_editor_text_next_button_maps_to_editor_next()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;

    const UiRect bounds =
        AgendaUiLayout::FooterRight;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            3000);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::EditorNext);
}

void test_editor_text_language_status_maps_to_language_toggle()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;

    const UiRect bounds{
        AgendaUiLayout::T9Status.x,
        AgendaUiLayout::T9Status.y,
        44,
        AgendaUiLayout::T9Status.height};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            3100);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ToggleT9Language);
}

void test_editor_text_mode_status_maps_to_mode_toggle()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;

    const UiRect bounds{
        AgendaUiLayout::T9Status.x + 44,
        AgendaUiLayout::T9Status.y,
        76,
        AgendaUiLayout::T9Status.height};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            3200);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ToggleT9Mode);
}

void test_editor_text_case_status_maps_to_case_toggle()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::EditorText;

    const UiRect bounds{
        AgendaUiLayout::T9Status.x + 120,
        AgendaUiLayout::T9Status.y,
        44,
        AgendaUiLayout::T9Status.height};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            3300);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ToggleT9Case);
}

void test_clock_setup_previous_day_maps_to_previous_day_action()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::ClockSetup;

    const UiRect bounds =
        AgendaUiLayout::ClockDatePrevious;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            3400);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ClockPreviousDay);
}

void test_clock_setup_next_day_maps_to_next_day_action()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::ClockSetup;

    const UiRect bounds =
        AgendaUiLayout::ClockDateNext;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            3500);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ClockNextDay);
}

void test_clock_setup_confirm_button_maps_to_confirm_clock()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::ClockSetup;

    const UiRect bounds =
        AgendaUiLayout::FooterCenter;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            3600);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ConfirmClock);
}

void test_clock_setup_hour_buttons_map_to_hour_actions()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::ClockSetup;

    const UiRect upBounds =
        AgendaUiLayout::ClockHourUp;

    const UiRect downBounds =
        AgendaUiLayout::ClockHourDown;

    const auto upAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                upBounds.x + upBounds.width / 2,
                upBounds.y + upBounds.height / 2},
            3700);

    const auto downAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                downBounds.x + downBounds.width / 2,
                downBounds.y + downBounds.height / 2},
            3800);

    assert(upAction.has_value());
    assert(
        upAction->type ==
        AgendaUiActionType::ClockNextHour);

    assert(downAction.has_value());
    assert(
        downAction->type ==
        AgendaUiActionType::ClockPreviousHour);
}

void test_clock_setup_minute_buttons_map_to_minute_actions()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::ClockSetup;

    const UiRect upBounds =
        AgendaUiLayout::ClockMinuteUp;

    const UiRect downBounds =
        AgendaUiLayout::ClockMinuteDown;

    const auto upAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                upBounds.x + upBounds.width / 2,
                upBounds.y + upBounds.height / 2},
            3900);

    const auto downAction =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                downBounds.x + downBounds.width / 2,
                downBounds.y + downBounds.height / 2},
            4000);

    assert(upAction.has_value());
    assert(
        upAction->type ==
        AgendaUiActionType::ClockNextMinute);

    assert(downAction.has_value());
    assert(
        downAction->type ==
        AgendaUiActionType::ClockPreviousMinute);
}

void test_startup_error_retry_button_maps_to_retry_startup()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::StartupError;

    const UiRect bounds =
        AgendaUiLayout::FooterCenter;

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            4100);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::RetryStartup);
}

void test_reminder_overlay_ok_maps_to_acknowledge_reminder()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Month;
    view.overlay = AgendaUiOverlay::Reminder;

    constexpr UiRect bounds{
        82,
        154,
        76,
        28};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            4200);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::AcknowledgeReminder);
}

void test_confirm_delete_overlay_yes_maps_to_confirm_delete()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Month;
    view.overlay = AgendaUiOverlay::ConfirmDelete;

    constexpr UiRect bounds{
        48,
        144,
        60,
        30};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            4300);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ConfirmDelete);
}

void test_confirm_delete_overlay_no_maps_to_cancel_delete()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Month;
    view.overlay = AgendaUiOverlay::ConfirmDelete;

    constexpr UiRect bounds{
        132,
        144,
        60,
        30};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            4400);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::CancelDelete);
}

void test_confirm_discard_overlay_yes_maps_to_confirm_discard()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Month;
    view.overlay = AgendaUiOverlay::ConfirmDiscard;

    constexpr UiRect bounds{
        48,
        144,
        60,
        30};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            4500);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::ConfirmDiscard);
}

void test_confirm_discard_overlay_no_maps_to_cancel_discard()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Month;
    view.overlay = AgendaUiOverlay::ConfirmDiscard;

    constexpr UiRect bounds{
        132,
        144,
        60,
        30};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            4600);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::CancelDiscard);
}

void test_operation_error_retry_maps_to_retry_operation()
{
    AgendaUiView view;
    view.screen = AgendaUiScreen::Month;
    view.overlay = AgendaUiOverlay::OperationError;

    constexpr UiRect bounds{
        44,
        148,
        70,
        30};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2},
            4700);

    assert(action.has_value());
    assert(
        action->type ==
        AgendaUiActionType::RetryOperation);
}

void test_operation_error_cancel_maps_to_cancel_operation()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Month;

    view.overlay =
        AgendaUiOverlay::OperationError;

    constexpr UiRect cancelButton{
        126,
        148,
        70,
        30};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                cancelButton.x +
                    cancelButton.width / 2,
                cancelButton.y +
                    cancelButton.height / 2},
            4800);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::CancelOperation);
}

void test_clock_error_ok_maps_to_cancel_operation()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::Month;

    view.overlay =
        AgendaUiOverlay::ClockError;

    constexpr UiRect okButton{
        82,
        150,
        76,
        30};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                okButton.x +
                    okButton.width / 2,
                okButton.y +
                    okButton.height / 2},
            4900);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::CancelOperation);
}

void test_t9_bottom_left_key_toggles_language()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorText;

    constexpr UiRect languageKey{
        45,
        166,
        50,
        22};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                languageKey.x +
                    languageKey.width / 2,
                languageKey.y +
                    languageKey.height / 2},
            5000);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::ToggleT9Language);
}

void test_t9_bottom_right_key_maps_to_backspace()
{
    AgendaUiView view;

    view.screen =
        AgendaUiScreen::EditorText;

    constexpr UiRect backspaceKey{
        155,
        166,
        50,
        22};

    const auto action =
        AgendaTouchMapper{}.map(
            view,
            UiPoint{
                backspaceKey.x +
                    backspaceKey.width / 2,
                backspaceKey.y +
                    backspaceKey.height / 2},
            5100);

    assert(action.has_value());

    assert(
        action->type ==
        AgendaUiActionType::T9Backspace);
}

int main()
{
    test_month_previous_arrow_maps_to_previous_month();
    test_month_next_arrow_maps_to_next_month();
    test_month_cell_maps_to_its_actual_date();
    test_day_previous_page_maps_when_enabled();
    test_day_next_page_maps_when_enabled();
    test_day_event_row_maps_to_event_id();
    test_day_back_button_maps_to_back();
    test_day_create_button_maps_to_start_create();
    test_event_details_back_maps_to_back();
    test_event_details_edit_maps_to_start_edit();
    test_event_details_delete_maps_to_request_delete();
    test_editor_date_previous_maps_to_previous_day();
    test_editor_date_next_maps_to_next_day();
    test_editor_date_next_stage_maps_to_editor_next();
    test_editor_date_back_maps_to_editor_back();
    test_editor_date_cancel_maps_to_editor_cancel();
    test_editor_time_all_day_mode_maps_to_toggle();
    test_editor_time_hour_buttons_map_to_hour_changes();
    test_editor_time_minute_buttons_map_to_minute_changes();
    test_editor_time_timed_mode_enables_time_for_all_day_event();
    test_editor_time_back_button_maps_to_editor_back();
    test_editor_time_cancel_button_maps_to_editor_cancel();
    test_editor_time_next_button_maps_to_editor_next();
    test_editor_review_back_button_maps_to_editor_back();
    test_editor_review_save_button_maps_to_save_editor();
    test_editor_text_key_two_maps_to_t9_digit_action();
    test_editor_text_back_button_maps_to_editor_back();
    test_editor_text_next_button_maps_to_editor_next();
    test_editor_text_language_status_maps_to_language_toggle();
    test_editor_text_mode_status_maps_to_mode_toggle();
    test_editor_text_case_status_maps_to_case_toggle();
    test_clock_setup_previous_day_maps_to_previous_day_action();
    test_clock_setup_next_day_maps_to_next_day_action();
    test_clock_setup_confirm_button_maps_to_confirm_clock();
    test_clock_setup_hour_buttons_map_to_hour_actions();
    test_clock_setup_minute_buttons_map_to_minute_actions();
    test_startup_error_retry_button_maps_to_retry_startup();
    test_reminder_overlay_ok_maps_to_acknowledge_reminder();
    test_confirm_delete_overlay_yes_maps_to_confirm_delete();
    test_confirm_delete_overlay_no_maps_to_cancel_delete();
    test_confirm_discard_overlay_yes_maps_to_confirm_discard();
    test_confirm_discard_overlay_no_maps_to_cancel_discard();
    test_operation_error_retry_maps_to_retry_operation();
    test_operation_error_cancel_maps_to_cancel_operation();
    test_clock_error_ok_maps_to_cancel_operation();
    test_t9_bottom_left_key_toggles_language();
    test_t9_bottom_right_key_maps_to_backspace();

    std::cout
        << "All agenda touch mapper tests passed.\n";

    return 0;
}