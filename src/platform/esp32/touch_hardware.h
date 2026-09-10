#pragma once

#include "touch_sample.h"

class TouchHardware
{
public:
    bool begin();
    bool read(TouchSample& sample);
};
