#pragma once

#include "touch_sample.h"

enum class TouchInputError
{
    None,
    Unavailable,
    ReadFailed
};

struct TouchInputReadResult
{
    TouchInputError error =
        TouchInputError::Unavailable;

    TouchSample sample{
        false,
        0,
        0};
};

class TouchInput
{
public:
    virtual ~TouchInput() = default;

    virtual TouchInputReadResult read() = 0;
};