#include <cassert>
#include <iostream>
#include <string>

#include "core/utf8.h"

void test_counts_ascii_characters()
{
    assert(utf8CodePointCount("") == 0);
    assert(utf8CodePointCount("abc") == 3);
    assert(utf8CodePointCount("Digital Diary") == 13);
}

void test_counts_unicode_characters_not_bytes()
{
    assert(utf8CodePointCount(u8"Σήμερα") == 6);
    assert(utf8CodePointCount(u8"abcά") == 4);
    assert(utf8CodePointCount(u8"Aά🙂") == 3);
}

void test_returns_last_complete_character()
{
    assert(utf8LastCodePoint("") == "");
    assert(utf8LastCodePoint("abc") == "c");
    assert(utf8LastCodePoint(u8"abcά") == u8"ά");
    assert(utf8LastCodePoint(u8"A🙂") == u8"🙂");
}

void test_removes_one_complete_character()
{
    std::string value = u8"abcά";

    assert(utf8PopBack(value));
    assert(value == "abc");

    assert(utf8PopBack(value));
    assert(value == "ab");
}

void test_removes_four_byte_character_safely()
{
    std::string value = u8"A🙂";

    assert(utf8PopBack(value));
    assert(value == "A");

    assert(utf8PopBack(value));
    assert(value.empty());
}

void test_empty_text_cannot_be_removed()
{
    std::string value;

    assert(!utf8PopBack(value));
    assert(value.empty());
}

int main()
{
    test_counts_ascii_characters();
    test_counts_unicode_characters_not_bytes();
    test_returns_last_complete_character();
    test_removes_one_complete_character();
    test_removes_four_byte_character_safely();
    test_empty_text_cannot_be_removed();

    std::cout << "All UTF-8 tests passed.\n";

    return 0;
}