#pragma once

#include <cstddef>
#include <string>
#include <string_view>

std::size_t utf8CodePointCount(std::string_view text);

std::string utf8LastCodePoint(std::string_view text);

bool utf8PopBack(std::string& text);

bool isValidUtf8(std::string_view text);

std::string utf8Prefix(
    std::string_view text,
    std::size_t maximumCodePoints
);