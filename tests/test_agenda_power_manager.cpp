#include <cassert>
#include <cstdint>
#include <iostream>

#include "ui/agenda_power_manager.h"

namespace
{
    AgendaUiView viewFor(
        AgendaUiScreen screen,
        AgendaUiOverlay overlay = AgendaUiOverlay::None
    )
    {
        AgendaUiView view;
        view.screen = screen;
        view.overlay = overlay;
        return view;
    }

    void test_policy_thresholds_and_safe_deep_sleep()
    {
        AgendaPowerManager manager;
        const AgendaUiView month =
            viewFor(AgendaUiScreen::Month);

        manager.begin(1000);

        assert(
            manager.decide(45999, month).action ==
            AgendaPowerAction::None);

        const AgendaPowerDecision light =
            manager.decide(46000, month);

        assert(
            light.action ==
            AgendaPowerAction::EnterLightSleep);
        assert(
            light.policyWakeAfterMilliseconds ==
            255000);

        assert(
            manager.decide(301000, month).action ==
            AgendaPowerAction::EnterDeepSleep);
    }

    void test_unsafe_and_reminder_views_never_request_deep_sleep()
    {
        AgendaPowerManager manager;
        manager.begin(0);

        const AgendaPowerDecision editor =
            manager.decide(
                300000,
                viewFor(AgendaUiScreen::EditorText));

        assert(
            editor.action ==
            AgendaPowerAction::EnterLightSleep);
        assert(
            !editor.policyWakeAfterMilliseconds.has_value());

        const AgendaPowerDecision reminder =
            manager.decide(
                300000,
                viewFor(
                    AgendaUiScreen::Month,
                    AgendaUiOverlay::Reminder));

        assert(
            reminder.action ==
            AgendaPowerAction::None);
    }

    void test_activity_reset_is_wrap_safe()
    {
        AgendaPowerManager manager;
        const AgendaUiView month =
            viewFor(AgendaUiScreen::Month);

        constexpr std::uint32_t start =
            UINT32_MAX - 100U;

        manager.begin(start);
        manager.recordActivity(start + 50U);

        assert(
            manager.decide(
                start + 50U + 44999U,
                month).action ==
            AgendaPowerAction::None);

        assert(
            manager.decide(
                start + 50U + 45000U,
                month).action ==
            AgendaPowerAction::EnterLightSleep);
    }
}

int main()
{
    test_policy_thresholds_and_safe_deep_sleep();
    test_unsafe_and_reminder_views_never_request_deep_sleep();
    test_activity_reset_is_wrap_safe();

    std::cout << "All agenda power manager tests passed.\n";
    return 0;
}
