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

void test_accepts_valid_utf8_sequences()
{
    assert(isValidUtf8(""));
    assert(isValidUtf8("ASCII"));

    assert(
        isValidUtf8(
            std::string("\xCE\x91", 2)
        )
    );

    assert(
        isValidUtf8(
            std::string("\xF0\x9F\x99\x82", 4)
        )
    );
}

void test_rejects_invalid_utf8_sequences()
{
    assert(
        !isValidUtf8(
            std::string("\x80", 1)
        )
    );

    assert(
        !isValidUtf8(
            std::string("\xC0\xAF", 2)
        )
    );

    assert(
        !isValidUtf8(
            std::string("\xED\xA0\x80", 3)
        )
    );

    assert(
        !isValidUtf8(
            std::string("\xF4\x90\x80\x80", 4)
        )
    );

    assert(
        !isValidUtf8(
            std::string("\xE2\x82", 2)
        )
    );
}

void test_utf8_prefix_limits_ascii_text()
{
    assert(
        utf8Prefix("abcdef", 3) ==
        "abc"
    );
}

void test_utf8_prefix_preserves_greek_code_points()
{
    const std::string result =
        utf8Prefix(
            "Ημερολόγιο",
            4
        );

    assert(result == "Ημερ");
    assert(isValidUtf8(result));

    assert(
        utf8CodePointCount(result) == 4
    );
}

void test_utf8_prefix_handles_limit_edges()
{
    assert(
        utf8Prefix("abc", 0).empty()
    );

    assert(
        utf8Prefix("abc", 9) ==
        "abc"
    );
}

int main()
{
    test_counts_ascii_characters();
    test_counts_unicode_characters_not_bytes();
    test_returns_last_complete_character();
    test_removes_one_complete_character();
    test_removes_four_byte_character_safely();
    test_empty_text_cannot_be_removed();
    test_accepts_valid_utf8_sequences();
    test_rejects_invalid_utf8_sequences();
    test_utf8_prefix_limits_ascii_text();
    test_utf8_prefix_preserves_greek_code_points();
    test_utf8_prefix_handles_limit_edges();

    std::cout << "All UTF-8 tests passed.\n";

    return 0;
}