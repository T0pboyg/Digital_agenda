#include "t9_engine.h"

#include <array>
#include <vector>

#include "utf8.h"

namespace
{
constexpr std::uint32_t MULTI_TAP_TIMEOUT_MS = 900;
constexpr std::size_t MAX_TEXT_LENGTH = 500;

using CharacterGroup = std::vector<std::string>;
using CharacterTable = std::array<CharacterGroup, 10>;

const CharacterTable& englishTable(bool uppercase)
{
    static const CharacterTable lowercaseGroups{{
        {},
        {".", ",", "?", "!"},
        {"a", "b", "c"},
        {"d", "e", "f"},
        {"g", "h", "i"},
        {"j", "k", "l"},
        {"m", "n", "o"},
        {"p", "q", "r", "s"},
        {"t", "u", "v"},
        {"w", "x", "y", "z"}
    }};

    static const CharacterTable uppercaseGroups{{
        {},
        {".", ",", "?", "!"},
        {"A", "B", "C"},
        {"D", "E", "F"},
        {"G", "H", "I"},
        {"J", "K", "L"},
        {"M", "N", "O"},
        {"P", "Q", "R", "S"},
        {"T", "U", "V"},
        {"W", "X", "Y", "Z"}
    }};

    return uppercase ? uppercaseGroups : lowercaseGroups;
}

const CharacterTable& greekTable(bool uppercase)
{
    static const CharacterTable lowercaseGroups{{
        {},
        {".", ",", ";", "?", "!"},
        {u8"α", u8"ά", u8"β", u8"γ"},
        {u8"δ", u8"ε", u8"έ", u8"ζ"},
        {
            u8"η", u8"ή", u8"θ", u8"ι",
            u8"ί", u8"ϊ", u8"ΐ"
        },
        {u8"κ", u8"λ", u8"μ"},
        {u8"ν", u8"ξ", u8"ο", u8"ό"},
        {u8"π", u8"ρ", u8"σ", u8"ς"},
        {
            u8"τ", u8"υ", u8"ύ", u8"ϋ",
            u8"ΰ", u8"φ"
        },
        {u8"χ", u8"ψ", u8"ω", u8"ώ"}
    }};

    static const CharacterTable uppercaseGroups{{
        {},
        {".", ",", ";", "?", "!"},
        {u8"Α", u8"Ά", u8"Β", u8"Γ"},
        {u8"Δ", u8"Ε", u8"Έ", u8"Ζ"},
        {
            u8"Η", u8"Ή", u8"Θ", u8"Ι",
            u8"Ί", u8"Ϊ", u8"Ϊ"
        },
        {u8"Κ", u8"Λ", u8"Μ"},
        {u8"Ν", u8"Ξ", u8"Ο", u8"Ό"},
        {u8"Π", u8"Ρ", u8"Σ", u8"Σ"},
        {
            u8"Τ", u8"Υ", u8"Ύ", u8"Ϋ",
            u8"Ϋ", u8"Φ"
        },
        {u8"Χ", u8"Ψ", u8"Ω", u8"Ώ"}
    }};

    return uppercase ? uppercaseGroups : lowercaseGroups;
}

const CharacterGroup& characterGroup(
    T9Language language,
    bool uppercase,
    char digit
)
{
    const std::size_t index =
        static_cast<std::size_t>(digit - '0');

    if (language == T9Language::Greek)
    {
        return greekTable(uppercase)[index];
    }

    return englishTable(uppercase)[index];
}

void eraseLastUtf8Character(std::string& text)
{
    if (text.empty())
    {
        return;
    }

    std::size_t position = text.size() - 1;

    while (
        position > 0 &&
        (
            static_cast<unsigned char>(text[position]) &
            0xC0
        ) == 0x80
    )
    {
        --position;
    }

    text.erase(position);
}

bool hasRoomForCharacter(const std::string& text)
{
    return utf8CodePointCount(text) < MAX_TEXT_LENGTH;
}
}

T9Engine::T9Engine()
    : text_(),
      language_(T9Language::English),
      mode_(T9Mode::Letters),
      uppercase_(false),
      hasPending_(false),
      pendingDigit_('\0'),
      pendingIndex_(0),
      lastPressTime_(0)
{
}

const std::string& T9Engine::text() const
{
    return text_;
}

T9Language T9Engine::language() const
{
    return language_;
}

T9Mode T9Engine::mode() const
{
    return mode_;
}

bool T9Engine::uppercase() const
{
    return uppercase_;
}

void T9Engine::setText(const std::string& newText)
{
    text_ = newText;
    commitPending();
}

bool T9Engine::pressDigit(
    char digit,
    std::uint32_t timeMilliseconds
)
{
    if (digit < '0' || digit > '9')
    {
        return false;
    }

    if (mode_ == T9Mode::Numbers)
    {
        commitPending();

        if (!hasRoomForCharacter(text_))
        {
            return false;
        }

        text_ += digit;
        return true;
    }

    if (digit == '0')
    {
        commitPending();

        if (!hasRoomForCharacter(text_))
        {
            return false;
        }

        text_ += ' ';
        return true;
    }

    const CharacterGroup& group =
        characterGroup(language_, uppercase_, digit);

    const bool cyclesPendingCharacter =
        hasPending_ &&
        pendingDigit_ == digit &&
        timeMilliseconds - lastPressTime_ <=
            MULTI_TAP_TIMEOUT_MS;

    if (cyclesPendingCharacter)
    {
        pendingIndex_ =
            (pendingIndex_ + 1) % group.size();

        eraseLastUtf8Character(text_);
        text_ += group[pendingIndex_];
        lastPressTime_ = timeMilliseconds;

        return true;
    }

    commitPending();

    if (!hasRoomForCharacter(text_))
    {
        return false;
    }

    text_ += group.front();

    hasPending_ = true;
    pendingDigit_ = digit;
    pendingIndex_ = 0;
    lastPressTime_ = timeMilliseconds;

    return true;
}

void T9Engine::toggleLanguage()
{
    commitPending();

    if (language_ == T9Language::English)
    {
        language_ = T9Language::Greek;
    }
    else
    {
        language_ = T9Language::English;
    }
}

void T9Engine::toggleMode()
{
    commitPending();

    if (mode_ == T9Mode::Letters)
    {
        mode_ = T9Mode::Numbers;
    }
    else
    {
        mode_ = T9Mode::Letters;
    }
}

void T9Engine::toggleCase()
{
    commitPending();
    uppercase_ = !uppercase_;
}

void T9Engine::commitPending()
{
    hasPending_ = false;
    pendingDigit_ = '\0';
    pendingIndex_ = 0;
}

bool T9Engine::backspace()
{
    if (text_.empty())
    {
        return false;
    }

    eraseLastUtf8Character(text_);
    commitPending();

    return true;
}