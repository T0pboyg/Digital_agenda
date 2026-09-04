#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "core/t9_engine.h"

std::string repeatedGreekCharacter(std::size_t count)
{
    std::string result;

    for (std::size_t index = 0; index < count; ++index)
    {
        result += u8"ά";
    }

    return result;
}

void expectCycle(
    T9Engine& input,
    char digit,
    const std::vector<std::string>& expected
)
{
    input.setText("");

    std::uint32_t time = 0;

    for (const std::string& character : expected)
    {
        assert(input.pressDigit(digit, time));
        assert(input.text() == character);

        time += 100;
    }

    assert(input.pressDigit(digit, time));
    assert(input.text() == expected.front());
}

void test_starts_in_english_letters_mode()
{
    T9Engine input;

    assert(input.text().empty());
    assert(input.language() == T9Language::English);
    assert(input.mode() == T9Mode::Letters);
}

void test_contains_all_english_groups()
{
    T9Engine input;

    expectCycle(input, '1', {".", ",", "?", "!"});
    expectCycle(input, '2', {"a", "b", "c"});
    expectCycle(input, '3', {"d", "e", "f"});
    expectCycle(input, '4', {"g", "h", "i"});
    expectCycle(input, '5', {"j", "k", "l"});
    expectCycle(input, '6', {"m", "n", "o"});
    expectCycle(input, '7', {"p", "q", "r", "s"});
    expectCycle(input, '8', {"t", "u", "v"});
    expectCycle(input, '9', {"w", "x", "y", "z"});
}

void test_contains_all_greek_groups()
{
    T9Engine input;
    input.toggleLanguage();

    expectCycle(input, '1', {".", ",", ";", "?", "!"});
    expectCycle(input, '2', {u8"α", u8"ά", u8"β", u8"γ"});
    expectCycle(input, '3', {u8"δ", u8"ε", u8"έ", u8"ζ"});

    expectCycle(
        input,
        '4',
        {u8"η", u8"ή", u8"θ", u8"ι",
         u8"ί", u8"ϊ", u8"ΐ"}
    );

    expectCycle(input, '5', {u8"κ", u8"λ", u8"μ"});
    expectCycle(input, '6', {u8"ν", u8"ξ", u8"ο", u8"ό"});
    expectCycle(input, '7', {u8"π", u8"ρ", u8"σ", u8"ς"});

    expectCycle(
        input,
        '8',
        {u8"τ", u8"υ", u8"ύ", u8"ϋ", u8"ΰ", u8"φ"}
    );

    expectCycle(input, '9', {u8"χ", u8"ψ", u8"ω", u8"ώ"});
}

void test_cycles_at_900_ms_and_commits_after_timeout()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    assert(input.text() == "a");

    assert(input.pressDigit('2', 900));
    assert(input.text() == "b");

    assert(input.pressDigit('2', 1801));
    assert(input.text() == "ba");
}

void test_different_digit_commits_previous_character()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    assert(input.pressDigit('3', 100));
    assert(input.text() == "ad");

    assert(input.pressDigit('3', 200));
    assert(input.text() == "ae");
}

void test_zero_adds_spaces_in_letters_mode()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    assert(input.pressDigit('0', 100));
    assert(input.text() == "a ");

    assert(input.pressDigit('0', 200));
    assert(input.text() == "a  ");
}

void test_number_mode_adds_digits_immediately()
{
    T9Engine input;
    input.toggleMode();

    for (char digit = '0'; digit <= '9'; ++digit)
    {
        assert(input.pressDigit(digit, 0));
    }

    assert(input.text() == "0123456789");
}

void test_rejects_non_digit_input()
{
    T9Engine input;

    assert(!input.pressDigit('x', 0));
    assert(!input.pressDigit('-', 100));
    assert(input.text().empty());
}

void test_uppercase_english_letters()
{
    T9Engine input;
    input.toggleCase();

    expectCycle(input, '2', {"A", "B", "C"});
}

void test_uppercase_greek_letters()
{
    T9Engine input;
    input.toggleLanguage();
    input.toggleCase();

    expectCycle(input, '2', {u8"Α", u8"Ά", u8"Β", u8"Γ"});
}

void test_language_change_commits_pending_character()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    input.toggleLanguage();

    assert(input.language() == T9Language::Greek);
    assert(input.pressDigit('2', 100));
    assert(input.text() == u8"aα");
}

void test_mode_change_commits_pending_character()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    input.toggleMode();

    assert(input.mode() == T9Mode::Numbers);
    assert(input.pressDigit('2', 100));
    assert(input.text() == "a2");
}

void test_case_change_commits_pending_character()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    input.toggleCase();

    assert(input.pressDigit('2', 100));
    assert(input.text() == "aA");
}

void test_manual_commit_starts_new_character()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    input.commitPending();

    assert(input.pressDigit('2', 100));
    assert(input.text() == "aa");
}

void test_backspace_removes_complete_utf8_characters()
{
    T9Engine input;
    input.setText(u8"Aά🙂");

    assert(input.backspace());
    assert(input.text() == u8"Aά");

    assert(input.backspace());
    assert(input.text() == "A");

    assert(input.backspace());
    assert(input.text().empty());

    assert(!input.backspace());
}

void test_backspace_clears_pending_state()
{
    T9Engine input;

    assert(input.pressDigit('2', 0));
    assert(input.pressDigit('2', 100));
    assert(input.text() == "b");

    assert(input.backspace());
    assert(input.text().empty());

    assert(input.pressDigit('2', 200));
    assert(input.text() == "a");
}

void test_accepts_character_at_500_character_boundary()
{
    T9Engine input;
    input.setText(repeatedGreekCharacter(499));

    assert(input.pressDigit('2', 0));
    assert(input.text().size() > 500);
}

void test_rejects_new_character_above_500_character_limit()
{
    T9Engine input;
    std::string original = repeatedGreekCharacter(500);

    input.setText(original);

    assert(!input.pressDigit('2', 0));
    assert(input.text() == original);
}

int main()
{
    test_starts_in_english_letters_mode();
    test_contains_all_english_groups();
    test_contains_all_greek_groups();
    test_cycles_at_900_ms_and_commits_after_timeout();
    test_different_digit_commits_previous_character();
    test_zero_adds_spaces_in_letters_mode();
    test_number_mode_adds_digits_immediately();
    test_rejects_non_digit_input();
    test_uppercase_english_letters();
    test_uppercase_greek_letters();
    test_language_change_commits_pending_character();
    test_mode_change_commits_pending_character();
    test_case_change_commits_pending_character();
    test_manual_commit_starts_new_character();
    test_backspace_removes_complete_utf8_characters();
    test_backspace_clears_pending_state();
    test_accepts_character_at_500_character_boundary();
    test_rejects_new_character_above_500_character_limit();

    std::cout << "All T9 engine tests passed.\n";

    return 0;
}