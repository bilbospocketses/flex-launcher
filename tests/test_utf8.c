#include <string.h>
#include "check.h"
#include "utf8.h"

// utf8_truncate works in place, inside a heap copy of a title. These tests put the title in a
// larger buffer with guard bytes in front of it, so a write before the title's first byte shows
// up as a changed guard instead of as silent heap corruption.
#define GUARD 8

static void place(char *buffer, size_t size, const char *title)
{
    memset(buffer, 'G', size);
    strcpy(buffer + GUARD, title);
}

static int guard_intact(const char *buffer)
{
    for (int i = 0; i < GUARD; i++) {
        if (buffer[i] != 'G')
            return 0;
    }
    return 1;
}

// A function to test that ordinary truncation keeps today's result
static void test_truncate_ordinary(void)
{
    char b[64];
    place(b, sizeof(b), "Television");
    utf8_truncate(b + GUARD, 200, 100);        // 20 px per character, 5 fit
    CHECK(strcmp(b + GUARD, "Te...") == 0);
    CHECK(guard_intact(b));
}

// A function to test that a title with room for fewer than three characters stays in its buffer
static void test_truncate_few_chars_fit(void)
{
    char b[64];
    place(b, sizeof(b), "Kodi");
    utf8_truncate(b + GUARD, 76, 38);          // 19 px per character, only 2 fit
    CHECK(guard_intact(b));
    CHECK(strcmp(b + GUARD, "...") == 0);
}

// A function to test that a two-character title is too short to shorten and is left alone
static void test_truncate_two_chars(void)
{
    char b[64];
    place(b, sizeof(b), "TV");
    utf8_truncate(b + GUARD, 60, 20);          // 30 px per character, none fit
    CHECK(guard_intact(b));
    CHECK(strcmp(b + GUARD, "TV") == 0);
}

// A function to test the multi-byte path when the walk reaches the start of the title
static void test_truncate_multibyte(void)
{
    char b[64];
    place(b, sizeof(b), "\xC3\xA9t\xC3\xA9");  // 3 characters in 5 bytes
    utf8_truncate(b + GUARD, 60, 20);          // 20 px per character, 1 fits
    CHECK(guard_intact(b));
    CHECK(strcmp(b + GUARD, "...") == 0);
}

// A function to test the degenerate inputs: an empty title, and one narrower in px than in characters
static void test_truncate_degenerate(void)
{
    char b[64];
    place(b, sizeof(b), "");
    utf8_truncate(b + GUARD, 0, 10);
    CHECK(guard_intact(b));
    CHECK(strcmp(b + GUARD, "") == 0);

    place(b, sizeof(b), "Kodi");
    utf8_truncate(b + GUARD, 2, 1);            // average width rounds to 0
    CHECK(guard_intact(b));
    CHECK(strcmp(b + GUARD, "...") == 0);
}

// A function to test that a cut title loses the character before its "...", a whole UTF-8
// character at a time, down to "...", and that an uncut title is left alone
static void test_shorten(void)
{
    char b[64];
    place(b, sizeof(b), "Tel...");
    CHECK(utf8_shorten(b + GUARD) == 1);
    CHECK(strcmp(b + GUARD, "Te...") == 0);

    place(b, sizeof(b), "T\xC3\xA9...");       // a two-byte character before the dots
    CHECK(utf8_shorten(b + GUARD) == 1);
    CHECK(strcmp(b + GUARD, "T...") == 0);
    CHECK(utf8_shorten(b + GUARD) == 1);
    CHECK(strcmp(b + GUARD, "...") == 0);
    CHECK(utf8_shorten(b + GUARD) == 0);       // nothing left to remove
    CHECK(strcmp(b + GUARD, "...") == 0);
    CHECK(guard_intact(b));

    place(b, sizeof(b), "TV");                 // never cut, so not shortened
    CHECK(utf8_shorten(b + GUARD) == 0);
    CHECK(strcmp(b + GUARD, "TV") == 0);
}

int main(void)
{
    test_truncate_ordinary();
    test_truncate_few_chars_fit();
    test_truncate_two_chars();
    test_truncate_multibyte();
    test_truncate_degenerate();
    test_shorten();
    return check_report();
}
