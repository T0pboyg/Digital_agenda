#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include "core/agenda_controller.h"
#include "core/agenda_repository.h"
#include "core/agenda_ui_state_machine.h"
#include "ui/agenda_renderer.h"
#include "ui/agenda_runtime.h"
#include "ui/agenda_touch_mapper.h"
#include "ui/touch_release_detector.h"

namespace
{
    class RuntimeStorage final : public ByteStorage
    {
    public:
        ByteReadResult read() const override
        {
            return ByteReadResult{
                ByteStorageError::NotFound,
                {}};
        }

        ByteStorageError write(
            const std::vector<std::uint8_t> &) const override
        {
            return ByteStorageError::None;
        }
    };

    class RuntimeClock final : public Clock
    {
    public:
        mutable std::size_t readCount = 0;

        DateTime value{
            Date{2026, 9, 15},
            TimeOfDay{10, 0}};

        ClockReadResult read() const override
        {
            ++readCount;

            return ClockReadResult{
                ClockError::None,
                value};
        }

        ClockError set(const DateTime &newValue) override
        {
            value = newValue;
            return ClockError::None;
        }
    };

    class ScriptedTouchInput final : public TouchInput
    {
    public:
        explicit ScriptedTouchInput(
            std::vector<TouchInputReadResult> results)
            : results_(std::move(results))
        {
        }

        TouchInputReadResult read() override
        {
            if (next_ >= results_.size())
            {
                return TouchInputReadResult{
                    TouchInputError::None,
                    TouchSample{false, 0, 0}};
            }

            return results_[next_++];
        }

    private:
        std::vector<TouchInputReadResult> results_;
        std::size_t next_ = 0;
    };

    class CountingDrawSurface final : public DrawSurface
    {
    public:
        std::size_t clearCount = 0;

        void clear(UiColor) override
        {
            ++clearCount;
        }

        void fillRectangle(const UiRect &, UiColor) override
        {
        }

        void drawRectangle(
            const UiRect &,
            UiColor,
            int) override
        {
        }

        void fillRoundedRectangle(
            const UiRect &,
            int,
            UiColor) override
        {
        }

        void drawRoundedRectangle(
            const UiRect &,
            int,
            UiColor,
            int) override
        {
        }

        void fillCircle(UiPoint, int, UiColor) override
        {
        }

        void drawCircle(
            UiPoint,
            int,
            UiColor,
            int) override
        {
        }

        void drawLine(
            UiPoint,
            UiPoint,
            UiColor,
            int) override
        {
        }

        void drawText(
            const UiRect &,
            std::string_view,
            UiTextStyle,
            UiColor,
            UiTextAlignment) override
        {
        }
    };

    void test_one_release_handles_one_action_and_redraws_once()
    {
        RuntimeStorage storage;
        RuntimeClock clock;
        AgendaRepository repository(storage);
        AgendaController controller(repository);

        AgendaUiStateMachine stateMachine(
            controller,
            clock,
            clock.value);

        stateMachine.start();

        ScriptedTouchInput touch({{TouchInputError::None, {true, 200, 20}},
                                  {TouchInputError::None, {false, 0, 0}},
                                  {TouchInputError::None, {false, 0, 0}}});

        TouchReleaseDetector releaseDetector;
        AgendaTouchMapper touchMapper;
        AgendaRenderer renderer;
        CountingDrawSurface surface;

        AgendaRuntime runtime(
            touch,
            releaseDetector,
            touchMapper,
            stateMachine,
            renderer,
            surface);

        runtime.begin(0);

        const auto pressed = runtime.update(10);
        const auto released = runtime.update(20);
        const auto idle = runtime.update(30);

        assert(!pressed.actionHandled);
        assert(!pressed.rendered);
        assert(released.actionHandled);
        assert(released.rendered);
        assert(!idle.actionHandled);
        assert(!idle.rendered);
        assert(surface.clearCount == 1);
        assert(stateMachine.view().displayedMonth.year == 2026);
        assert(stateMachine.view().displayedMonth.month == 10);
    }

    void test_minute_tick_uses_wrap_safe_elapsed_time()
    {
        RuntimeStorage storage;
        RuntimeClock clock;
        AgendaRepository repository(storage);
        AgendaController controller(repository);

        AgendaUiStateMachine stateMachine(
            controller,
            clock,
            clock.value);

        stateMachine.start();
        assert(clock.readCount == 1);

        ScriptedTouchInput touch(
            std::vector<TouchInputReadResult>{});

        TouchReleaseDetector releaseDetector;
        AgendaTouchMapper touchMapper;
        AgendaRenderer renderer;
        CountingDrawSurface surface;

        AgendaRuntime runtime(
            touch,
            releaseDetector,
            touchMapper,
            stateMachine,
            renderer,
            surface);

        constexpr std::uint32_t start =
            UINT32_MAX - 20U;

        runtime.begin(start);

        const auto before =
            runtime.update(start + 59999U);

        assert(!before.ticked);
        assert(surface.clearCount == 0);
        assert(clock.readCount == 1);

        const auto due =
            runtime.update(start + 60000U);

        assert(due.ticked);
        assert(due.rendered);
        assert(surface.clearCount == 1);
        assert(clock.readCount == 2);
    }
}

int main()
{
    test_one_release_handles_one_action_and_redraws_once();
    test_minute_tick_uses_wrap_safe_elapsed_time();

    std::cout
        << "All agenda runtime tests passed.\n";

    return 0;
}