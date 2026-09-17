#include "touch_release_detector.h"

void TouchReleaseDetector::reset()
{
    latestPoint_.reset();
}

std::optional<UiPoint> TouchReleaseDetector::update(
    const TouchSample &sample)
{
    if (sample.touched)
    {
        if (
            sample.x < AGENDA_UI_DISPLAY_WIDTH &&
            sample.y < AGENDA_UI_DISPLAY_HEIGHT)
        {
            latestPoint_ = UiPoint{
                static_cast<int>(sample.x),
                static_cast<int>(sample.y)};
        }

        return std::nullopt;
    }

    if (!latestPoint_.has_value())
    {
        return std::nullopt;
    }

    const UiPoint released = *latestPoint_;
    latestPoint_.reset();

    return released;
}
