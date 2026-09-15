#pragma once

#include <string_view>

#include "date_time.h"

DateTime compileDateTimeOrFallback(
    std::string_view dateText,
    std::string_view timeText);