#include <cassert>
#include <iostream>

#include "ui/touch_release_detector.h"

namespace
{
    void test_press_move_release_emits_final_position()
    {
        TouchReleaseDetector detector;

        assert(!detector.update({true, 30, 40}));
        assert(!detector.update({true, 35, 47}));

        const auto released =
            detector.update({false, 0, 0});

        assert(released.has_value());
        assert(released->x == 35);
        assert(released->y == 47);
    }

    void test_release_is_emitted_only_once()
    {
        TouchReleaseDetector detector;

        assert(!detector.update({true, 100, 120}));
        assert(detector.update({false, 0, 0}));
        assert(!detector.update({false, 0, 0}));
        assert(!detector.update({false, 0, 0}));
    }

    void test_out_of_range_touch_does_not_create_release()
    {
        TouchReleaseDetector detector;

        assert(!detector.update({true, 240, 20}));
        assert(!detector.update({true, 20, 240}));
        assert(!detector.update({false, 0, 0}));
    }
}

int main()
{
    test_press_move_release_emits_final_position();
    test_release_is_emitted_only_once();
    test_out_of_range_touch_does_not_create_release();

    std::cout
        << "All touch release detector tests passed.\n";

    return 0;
}