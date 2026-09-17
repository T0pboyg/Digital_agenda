#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "core/agenda_repository.h"
#include "ui/agenda_power_coordinator.h"

namespace
{
    class TestStorage final : public ByteStorage
    {
    public:
        ByteReadResult read() const override
        {
            return ByteReadResult{
                ByteStorageError::NotFound,
                {}};
        }

        ByteStorageError write(
            const std::vector<std::uint8_t>&
        ) const override
        {
            return ByteStorageError::None;
        }
    };

    class TestClock final : public Clock
    {
    public:
        ClockError readError = ClockError::None;
        DateTime value{
            Date{2026, 9, 16},
            TimeOfDay{10, 0}};

        ClockReadResult read() const override
        {
            return ClockReadResult{readError, value};
        }

        ClockError set(const DateTime& next) override
        {
            value = next;
            return ClockError::None;
        }
    };

    class TestWakeDelayClock final : public WakeDelayClock
    {
    public:
        WakeDelayResult result{
            ClockError::None,
            false,
            120000000ULL};

        WakeDelayResult delayUntil(
            const DateTime&
        ) const override
        {
            return result;
        }
    };

    AgendaUiView monthView()
    {
        AgendaUiView view;
        view.screen = AgendaUiScreen::Month;
        view.overlay = AgendaUiOverlay::None;
        return view;
    }

    void addEvent(
        AgendaController& controller,
        const Date& date
    )
    {
        const AgendaControllerCreateResult created =
            controller.createEvent(
                EventDraft{
                    date,
                    std::nullopt,
                    "Future event"},
                DateTime{
                    Date{2026, 9, 16},
                    TimeOfDay{10, 0}});

        assert(created.error == AgendaControllerError::None);
        assert(created.eventError == EventError::None);
    }

    void test_earlier_reminder_timer_wins_over_policy_timer()
    {
        TestStorage storage;
        AgendaRepository repository(storage);
        AgendaController controller(repository);
        assert(controller.start().error == AgendaControllerError::None);
        addEvent(controller, Date{2026, 9, 18});

        TestClock clock;
        TestWakeDelayClock wakeDelayClock;
        AgendaPowerManager manager;
        AgendaPowerCoordinator coordinator(
            manager,
            controller,
            clock,
            wakeDelayClock);

        coordinator.begin(0);

        const AgendaPowerPlan plan =
            coordinator.update(45000, false, monthView());

        assert(plan.mode == AgendaSleepMode::Light);
        assert(plan.timerMicroseconds == 120000000ULL);
        assert(!plan.reminderDueNow);
        assert(plan.error == AgendaPowerCoordinatorError::None);
    }

    void test_due_reminder_blocks_sleep()
    {
        TestStorage storage;
        AgendaRepository repository(storage);
        AgendaController controller(repository);
        assert(controller.start().error == AgendaControllerError::None);
        addEvent(controller, Date{2026, 9, 17});

        TestClock clock;
        clock.value.time = TimeOfDay{17, 0};
        TestWakeDelayClock wakeDelayClock;
        AgendaPowerManager manager;
        AgendaPowerCoordinator coordinator(
            manager,
            controller,
            clock,
            wakeDelayClock);

        coordinator.begin(0);
        const AgendaPowerPlan plan =
            coordinator.update(45000, false, monthView());

        assert(plan.mode == AgendaSleepMode::None);
        assert(!plan.timerMicroseconds.has_value());
        assert(plan.reminderDueNow);
        assert(plan.error == AgendaPowerCoordinatorError::None);
    }

    void test_clock_failure_allows_light_but_blocks_deep_sleep()
    {
        TestStorage storage;
        AgendaRepository repository(storage);
        AgendaController controller(repository);
        assert(controller.start().error == AgendaControllerError::None);

        TestClock clock;
        clock.readError = ClockError::ReadFailed;
        TestWakeDelayClock wakeDelayClock;
        AgendaPowerManager manager;
        AgendaPowerCoordinator coordinator(
            manager,
            controller,
            clock,
            wakeDelayClock);

        coordinator.begin(0);

        const AgendaPowerPlan light =
            coordinator.update(45000, false, monthView());

        assert(light.mode == AgendaSleepMode::Light);
        assert(light.timerMicroseconds == 255000000ULL);
        assert(
            light.error ==
            AgendaPowerCoordinatorError::ClockReadFailed);

        const AgendaPowerPlan deep =
            coordinator.update(300000, false, monthView());

        assert(deep.mode == AgendaSleepMode::None);
        assert(!deep.timerMicroseconds.has_value());
        assert(
            deep.error ==
            AgendaPowerCoordinatorError::ClockReadFailed);
    }
}

int main()
{
    test_earlier_reminder_timer_wins_over_policy_timer();
    test_due_reminder_blocks_sleep();
    test_clock_failure_allows_light_but_blocks_deep_sleep();

    std::cout << "All agenda power coordinator tests passed.\n";
    return 0;
}
