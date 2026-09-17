#pragma once

#include <optional>

#include "touch_sample.h"
#include "ui_geometry.h"

class TouchReleaseDetector
{
public:
    void reset();

    std::optional<UiPoint> update(
        const TouchSample &sample);

private:
    std::optional<UiPoint> latestPoint_;
};
