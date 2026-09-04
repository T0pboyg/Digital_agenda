#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

enum class T9Language
{
    English,
    Greek
};

enum class T9Mode
{
    Letters,
    Numbers
};

class T9Engine
{
public:
    T9Engine();

    const std::string& text() const;

    T9Language language() const;
    T9Mode mode() const;

    void setText(const std::string& newText);

    bool pressDigit(
        char digit,
        std::uint32_t timeMilliseconds
    );

    void toggleLanguage();
    void toggleMode();
    void toggleCase();
    void commitPending();

    bool backspace();

private:
    std::string text_;
    T9Language language_;
    T9Mode mode_;
    bool uppercase_;

    bool hasPending_;
    char pendingDigit_;
    std::size_t pendingIndex_;
    std::uint32_t lastPressTime_;
};