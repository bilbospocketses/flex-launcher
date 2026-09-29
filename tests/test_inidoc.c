#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "inidoc.h"
#include "fileio.h"

// A function to parse a file's text, write it back out and give the result (caller frees)
static char *round_trip(const char *text, size_t length, size_t *out_length)
{
    IniDoc *doc = inidoc_parse(text, length);
    char *out = inidoc_serialize(doc, out_length);
    inidoc_free(doc);
    return out;
}

// A function to test that a file read and written back without edits is byte-identical
static void test_round_trip_is_identical(void)
{
    static const char *const texts[] = {
        "[General]\nDefaultMenu=Main\n",
        "[General]\r\nDefaultMenu=Main\r\n\r\n; comment\r\n",
        "\xEF\xBB\xBF[General]\nDefaultMenu=Main\n",
        "[General]\nDefaultMenu=Main",
        "# hash comment\n; semicolon comment\n[Layout]\nColumns = 4 ; four across\nRows: 2\n",
        "[Main]\nEntry1=A;apps;:quit\n[Main]\nEntry1=B;apps;:quit\n",
        "[Layout]\nRows=1\n  continued\n",
        "[Caf\xC3\xA9]\nEntry1=Th\xC3\xA9;apps;:quit\n",
        "",
        "\n\n",
        "[Broken\nNoSeparator\n"
    };
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
        size_t length = 0;
        char *out = round_trip(texts[i], strlen(texts[i]), &length);
        CHECK(out != NULL && length == strlen(texts[i]) && memcmp(out, texts[i], length) == 0);
        free(out);
    }

    // The sample config the build generates, exactly as shipped
    size_t sample_length = 0;
    char *sample = fileio_read_all(SAMPLE_CONFIG, &sample_length);
    CHECK(sample != NULL);
    if (sample != NULL) {
        size_t length = 0;
        char *out = round_trip(sample, sample_length, &length);
        CHECK(out != NULL && length == sample_length && memcmp(out, sample, length) == 0);
        free(out);
        free(sample);
    }
}

// A function to test that a key is found, and its value read, the way inih reads it
static void test_get_reads_like_inih(void)
{
    const char *text =
        "Loose=before any section\n"
        "[Background]\n"
        "Mode = Slideshow ; the photos\n"
        "Semi=;x\n"
        "Spaced= ;x\n"
        "[ Games ]\n"
        "Rows: 3\n"
        "  Columns=6\n"
        "[Layout]\n"
        "Rows=1\n"
        "Rows=2\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, "", "Loose"), "before any section");
    CHECK_STR(inidoc_get(doc, "Background", "Mode"), "Slideshow");
    CHECK_STR(inidoc_get(doc, "Background", "Semi"), ";x");       // No space before ';': not a comment
    CHECK_STR(inidoc_get(doc, "Background", "Spaced"), "");       // A space before ';': a comment
    CHECK_STR(inidoc_get(doc, " Games ", "Rows"), "3");           // The name keeps its spaces, as inih's does
    CHECK(inidoc_get(doc, "Games", "Rows") == NULL);
    CHECK(inidoc_get(doc, " Games ", "Columns") == NULL);         // Indented after a key: a continuation
    CHECK_STR(inidoc_get(doc, "Layout", "Rows"), "2");            // The last one wins, as in the parser
    CHECK(inidoc_get(doc, "layout", "Rows") == NULL);             // Names match exactly, case included
    inidoc_free(doc);
}

// A function to apply one set or remove (value NULL) and compare the whole file with what is expected
static void check_edit(int line, const char *text, const char *section, const char *key, const char *value,
                       IniDocPlacement placement, bool expected_ok, const char *expected)
{
    IniDoc *doc = inidoc_parse(text, strlen(text));
    bool ok = value != NULL ? inidoc_set(doc, section, key, value, placement) : inidoc_remove(doc, section, key);
    char *out = inidoc_serialize(doc, NULL);
    check_count++;
    if (ok != expected_ok || out == NULL || strcmp(out, expected) != 0) {
        check_failures++;
        fprintf(stderr, "test_inidoc.c:%d: edit gave %s and\n[%s]\nexpected %s and\n[%s]\n", line,
            ok ? "true" : "false", out != NULL ? out : "(null)", expected_ok ? "true" : "false", expected);
    }
    free(out);
    inidoc_free(doc);
}

// A function to test every way a value is set or removed
static void test_edits(void)
{
    // An existing key: only the value changes; spacing and the trailing comment stay
    check_edit(__LINE__, "[Layout]\nIconSize = 256 ; cap\n", "Layout", "IconSize", "128", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nIconSize = 128 ; cap\n");

    // An empty value before a comment: the value goes straight after '=', so the comment stays a comment
    check_edit(__LINE__, "[Background]\nImage=   ; pick one\n", "Background", "Image", "/pics/a.png", INIDOC_AFTER_LAST_KEY, true,
               "[Background]\nImage=/pics/a.png   ; pick one\n");

    // A repeated key: the last one, which the parser uses, is the one edited
    check_edit(__LINE__, "[Layout]\nRows=1\nRows=2\n", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\nRows=3\n");

    // A new key after the section's last key, before its trailing blank and comment lines
    check_edit(__LINE__, "[Layout]\nRows=1\nColumns=4\n\n; the menus\n[Main]\nEntry1=A;apps;:quit\n",
               "Layout", "IconSize", "256", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\nColumns=4\nIconSize=256\n\n; the menus\n[Main]\nEntry1=A;apps;:quit\n");

    // A new key in a menu section goes directly under its header, above the entries
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\n", "Games", "Rows", "3", INIDOC_UNDER_HEADER, true,
               "[Games]\nRows=3\nEntry1=A;apps;:quit\n");

    // An indented key under the header would become the new key's continuation, so the new key goes after it
    check_edit(__LINE__, "[Games]\n  Rows=3\n", "Games", "Columns", "6", INIDOC_UNDER_HEADER, true,
               "[Games]\n  Rows=3\nColumns=6\n");

    // A continuation stays with its key
    check_edit(__LINE__, "[Layout]\nRows=1\n  more\n", "Layout", "Columns", "4", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\n  more\nColumns=4\n");

    // A missing section is added at the end after a blank line, in the file's own line endings, and a
    // file without a final newline still has none
    check_edit(__LINE__, "[General]\r\nDefaultMenu=Main", "Layout", "Columns", "5", INIDOC_AFTER_LAST_KEY, true,
               "[General]\r\nDefaultMenu=Main\r\n\r\n[Layout]\r\nColumns=5");
    check_edit(__LINE__, "[General]\nDefaultMenu=Main\n", "Layout", "Rows", "2", INIDOC_AFTER_LAST_KEY, true,
               "[General]\nDefaultMenu=Main\n\n[Layout]\nRows=2\n");

    // The BOM is kept
    check_edit(__LINE__, "\xEF\xBB\xBF[General]\nX=1\n", "General", "X", "2", INIDOC_AFTER_LAST_KEY, true,
               "\xEF\xBB\xBF[General]\nX=2\n");

    // Removing takes every occurrence, so the key is truly gone; the last line keeps "no newline"
    check_edit(__LINE__, "[Games]\nRows=3\nRows=2\nEntry1=A;apps;:quit\n", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, true,
               "[Games]\nEntry1=A;apps;:quit\n");
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\nRows=3", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, true,
               "[Games]\nEntry1=A;apps;:quit");
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\n", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, false,
               "[Games]\nEntry1=A;apps;:quit\n");

    // Values the parser would read back differently are refused, and the file is left alone
    check_edit(__LINE__, "[Background]\n", "Background", "Image", "/a ;b", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", " /a", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", "/a\n", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", ";x", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
}

// A function to test that a key is edited together with its continuation lines. inih reads each
// indented line after a key as that key's value again, even past blank, comment and unreadable
// lines, and the launcher keeps the last one.
static void test_continuations(void)
{
    // Setting an existing key removes its continuation lines, so it reads exactly the new value
    check_edit(__LINE__, "[Layout]\nRows=2\n  4\n", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=3\n");
    check_edit(__LINE__, "[Layout]\nRows=2\n\n; note\n  4\nColumns=5\n", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=3\n\n; note\nColumns=5\n");
    check_edit(__LINE__, "[Layout]\nRows=2\n  4", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=3");
    const char *text = "[Layout]\nRows=2\n  4\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK(inidoc_set(doc, "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_get(doc, "Layout", "Rows"), "3");
    inidoc_free(doc);

    // A new key that would take an indented line as its continuation goes after it instead, at the
    // end of the section when the section has no key to follow
    check_edit(__LINE__, "[Games]\n   junk", "Games", "Rows", "3", INIDOC_UNDER_HEADER, true, "[Games]\n   junk\nRows=3");

    // A new key goes after the last key's continuation lines, even one past a blank line
    check_edit(__LINE__, "[Layout]\nRows=1\n\n  more\n", "Layout", "Columns", "4", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\n\n  more\nColumns=4\n");

    // Removing a key removes its continuation lines, leaving none to join the key before it
    check_edit(__LINE__, "[Layout]\nColumns=4\nRows=2\n  4\n", "Layout", "Rows", NULL, INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nColumns=4\n");
    check_edit(__LINE__, "[Layout]\nColumns=4\nRows=2\n\n  4\n", "Layout", "Rows", NULL, INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nColumns=4\n\n");
}

// A function to test inih's 199-byte line limit: a longer line would be misread
static void test_line_limit(void)
{
    char value[256];
    memset(value, 'a', 193);
    value[193] = '\0';                                   // "Image=" + 193 bytes = 199
    CHECK(inidoc_check("Image", value) == NULL);
    IniDoc *doc = inidoc_parse("[Background]\n", 13);
    CHECK(inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    value[193] = 'a';
    value[194] = '\0';                                   // 200 bytes
    CHECK(inidoc_check("Image", value) != NULL);
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    const char *kept = inidoc_get(doc, "Background", "Image");
    CHECK(kept != NULL && strlen(kept) == 193);
    inidoc_free(doc);

    // An existing line's trailing comment counts too
    const char *commented = "[Background]\nImage=x ; a long comment that takes up the room on this line\n";
    doc = inidoc_parse(commented, strlen(commented));
    value[150] = '\0';
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_get(doc, "Background", "Image"), "x");
    inidoc_free(doc);
}

// A function to test that a section's name is matched on its first 49 bytes, as inih keeps it,
// while the header line itself is written back exactly as it was
static void test_long_section_name(void)
{
    char name[61];
    memset(name, 'M', 60);
    name[60] = '\0';
    char prefix[INIDOC_MAX_SECTION + 1];                 // The name as inih gives it to the launcher
    memcpy(prefix, name, INIDOC_MAX_SECTION);
    prefix[INIDOC_MAX_SECTION] = '\0';
    char text[128];
    char expected[128];
    snprintf(text, sizeof(text), "[%s]\nRows=3\n", name);

    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, prefix, "Rows"), "3");
    CHECK_STR(inidoc_get(doc, name, "Rows"), "3");       // Agrees for 49 bytes: the same section to inih

    // Setting changes the existing key and adds no header; the 60-byte header line stays
    CHECK(inidoc_set(doc, prefix, "Rows", "4", INIDOC_UNDER_HEADER));
    snprintf(expected, sizeof(expected), "[%s]\nRows=4\n", name);
    char *out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);

    // Removing takes the key away, and the header line still stays
    CHECK(inidoc_remove(doc, prefix, "Rows"));
    CHECK(inidoc_get(doc, prefix, "Rows") == NULL);
    snprintf(expected, sizeof(expected), "[%s]\n", name);
    out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);
    inidoc_free(doc);

    // A new section with a long name is read back under the name inih gives it
    doc = inidoc_parse("[General]\n", 10);
    CHECK(inidoc_set(doc, name, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_get(doc, prefix, "Rows"), "2");
    snprintf(expected, sizeof(expected), "[General]\n\n[%s]\nRows=2\n", name);
    out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);
    inidoc_free(doc);
}

// A function to test the reasons given for a value that cannot be written
static void test_check(void)
{
    CHECK(inidoc_check("Image", "/home/me/Pictures/a b.png") == NULL);
    CHECK(inidoc_check("Image", "C:\\Pics\\a;b.png") == NULL);   // ';' after a non-space is fine
    CHECK(inidoc_check("Image", "/a ;b") != NULL);
    CHECK(inidoc_check("Image", "\t/a") != NULL);
    CHECK(inidoc_check("Image", "/a ") != NULL);
    CHECK(inidoc_check("Image", "/a\r") != NULL);
}

// A function to test that two different 60-byte section names that agree for 49 bytes are one
// section, as inih reads them: a key under either header is found, and set, through either name
static void test_long_names_agreeing_are_one_section(void)
{
    char first[61];
    char second[61];
    memset(first, 'M', 60);
    memset(second, 'M', 60);
    memset(first + INIDOC_MAX_SECTION, 'A', 60 - INIDOC_MAX_SECTION);
    memset(second + INIDOC_MAX_SECTION, 'B', 60 - INIDOC_MAX_SECTION);
    first[60] = '\0';
    second[60] = '\0';
    char text[256];
    char expected[256];
    snprintf(text, sizeof(text), "[%s]\nRows=3\n[%s]\nColumns=4\n", first, second);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, second, "Rows"), "3");
    CHECK_STR(inidoc_get(doc, first, "Columns"), "4");
    CHECK(inidoc_set(doc, first, "Columns", "5", INIDOC_AFTER_LAST_KEY));
    snprintf(expected, sizeof(expected), "[%s]\nRows=3\n[%s]\nColumns=5\n", first, second);
    char *out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);
    inidoc_free(doc);
}

// A function to test that a new key finds a safe place when the one asked for is not: under the
// header an indented key would become its continuation, so it goes after the section's last key
static void test_safe_places(void)
{
    check_edit(__LINE__, "[Games]\n\n  Rows=3\nEntry1=A;apps;:quit\n", "Games", "Columns", "6", INIDOC_UNDER_HEADER, true,
               "[Games]\n\n  Rows=3\nEntry1=A;apps;:quit\nColumns=6\n");
    check_edit(__LINE__, "[Games]\n\n  Rows=3\n", "Games", "Columns", "6", INIDOC_UNDER_HEADER, true,
               "[Games]\n\n  Rows=3\nColumns=6\n");

    // At the end of a section with no key, before the next header
    check_edit(__LINE__, "[Games]\n  junk\n[Main]\nEntry1=A;apps;:quit\n", "Games", "Rows", "3", INIDOC_UNDER_HEADER, true,
               "[Games]\n  junk\nRows=3\n[Main]\nEntry1=A;apps;:quit\n");
}

// A function to test a key with an empty name ("=x"): inih empties its prev_name for it, so an
// indented line after it is a line of its own, not its continuation
static void test_empty_key_name(void)
{
    const char *text = "[Layout]\n=x\n  Rows=3\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, "Layout", "Rows"), "3");
    inidoc_free(doc);
    check_edit(__LINE__, text, "Layout", "Rows", "4", INIDOC_AFTER_LAST_KEY, true, "[Layout]\n=x\n  Rows=4\n");

    // Removing it is refused: the indented line after it would join the key before it
    check_edit(__LINE__, "[Layout]\nRows=1\n=x\n  2\n", "Layout", "", NULL, INIDOC_AFTER_LAST_KEY, false,
               "[Layout]\nRows=1\n=x\n  2\n");
}

// A function to test that a new section's header must fit on one line too
static void test_new_header_limit(void)
{
    char section[256];
    memset(section, 'S', 197);
    section[197] = '\0';                                 // "[" + 197 + "]" = 199 bytes: it fits
    IniDoc *doc = inidoc_parse("[General]\n", 10);
    CHECK(inidoc_set(doc, section, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    inidoc_free(doc);
    section[197] = 'S';
    section[198] = '\0';                                 // 200 bytes: refused, and nothing added
    doc = inidoc_parse("[General]\n", 10);
    CHECK(!inidoc_set(doc, section, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    CHECK(strstr(inidoc_why(doc), "section") != NULL);
    char *out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, "[General]\n");
    free(out);
    inidoc_free(doc);
}

// A function to test that a NUL inside a line survives: the line is kept by its length, so the
// bytes after the NUL are written back, whether its own key or another is set
static void test_embedded_nul(void)
{
    static const char text[] = "[S]\nA=1\nB=x\0tail\nC=3\n";
    size_t length = sizeof(text) - 1;
    size_t out_length = 0;
    char *out = round_trip(text, length, &out_length);
    CHECK(out != NULL && out_length == length && memcmp(out, text, length) == 0);
    free(out);

    IniDoc *doc = inidoc_parse(text, length);
    CHECK_STR(inidoc_get(doc, "S", "B"), "x");           // inih stops at the NUL too
    CHECK(inidoc_set(doc, "S", "A", "2", INIDOC_AFTER_LAST_KEY));
    CHECK(inidoc_set(doc, "S", "B", "y", INIDOC_AFTER_LAST_KEY));
    static const char expected[] = "[S]\nA=2\nB=y\0tail\nC=3\n";
    out = inidoc_serialize(doc, &out_length);
    CHECK(out != NULL && out_length == sizeof(expected) - 1 && memcmp(out, expected, out_length) == 0);
    free(out);
    inidoc_free(doc);
}

// A function to test continuation lines after a key of 50 bytes or more: inih gives them the key's
// name cut to 49 bytes, so they set that other key, not the one they follow
static void test_long_key_continuations(void)
{
    char long_key[61];
    char short_key[INIDOC_MAX_NAME + 1];
    memset(long_key, 'K', 60);
    long_key[60] = '\0';
    memcpy(short_key, long_key, INIDOC_MAX_NAME);
    short_key[INIDOC_MAX_NAME] = '\0';
    char text[256];
    char expected[256];

    // Setting the 49-byte key removes the continuation that inih would read as its value last
    snprintf(text, sizeof(text), "[S]\n%s=0\n%s=1\n  2\n", short_key, long_key);
    snprintf(expected, sizeof(expected), "[S]\n%s=5\n%s=1\n", short_key, long_key);
    check_edit(__LINE__, text, "S", short_key, "5", INIDOC_AFTER_LAST_KEY, true, expected);

    // Setting the long key keeps it: it sets the 49-byte key, which the edit is not about
    snprintf(text, sizeof(text), "[S]\n%s=1\n  2\n", long_key);
    snprintf(expected, sizeof(expected), "[S]\n%s=3\n  2\n", long_key);
    check_edit(__LINE__, text, "S", long_key, "3", INIDOC_AFTER_LAST_KEY, true, expected);

    // Removing the 49-byte key removes it too, though no line of its own exists
    snprintf(expected, sizeof(expected), "[S]\n%s=1\n", long_key);
    check_edit(__LINE__, text, "S", short_key, NULL, INIDOC_AFTER_LAST_KEY, true, expected);

    // A new 49-byte key goes where no such continuation comes after it
    snprintf(text, sizeof(text), "[S]\n%s=1\n  2\n", long_key);
    snprintf(expected, sizeof(expected), "[S]\n%s=1\n  2\n%s=7\n", long_key, short_key);
    check_edit(__LINE__, text, "S", short_key, "7", INIDOC_UNDER_HEADER, true, expected);
}

// A function to test the reasons inidoc_why() gives for a set that failed
static void test_why(void)
{
    const char *commented = "[Background]\nImage=x ; a long comment that takes up the room on this line\n";
    IniDoc *doc = inidoc_parse(commented, strlen(commented));
    char value[256];
    memset(value, 'a', 150);
    value[150] = '\0';
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK(strstr(inidoc_why(doc), "too long for one line of config.ini") != NULL);
    CHECK(strstr(inidoc_why(doc), "with its comment") != NULL);
    CHECK(!inidoc_set(doc, "Background", "Image", "/a ;b", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_why(doc), inidoc_check("Image", "/a ;b"));
    CHECK(inidoc_set(doc, "Background", "Image", "/a.png", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_why(doc), "");
    inidoc_free(doc);

    // Spacing alone, with no comment, is named as such
    char spaced[256];
    snprintf(spaced, sizeof(spaced), "[Background]\nImage                                                  =   x\n");
    doc = inidoc_parse(spaced, strlen(spaced));
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK(strstr(inidoc_why(doc), "spacing") != NULL);
    inidoc_free(doc);
}

// A function to test that asking first (inidoc_check_in) agrees with setting: for every length of
// value, it refuses exactly what the set would refuse, with the same reason
static void test_check_in_agrees_with_set(void)
{
    static const char *const texts[] = {
        "[Background]\nImage=x ; a long comment that takes up the room on this line\n",
        "[Background]\nImage                                                  =   x\n",
        "[Background]\nMode=Image\n",
        "[General]\n"
    };
    char value[256];
    for (size_t t = 0; t < sizeof(texts) / sizeof(texts[0]); t++) {
        for (size_t length = 1; length <= 200; length++) {
            memset(value, 'a', length);
            value[length] = '\0';
            IniDoc *doc = inidoc_parse(texts[t], strlen(texts[t]));
            const char *asked = inidoc_check_in(doc, "Background", "Image", value);
            bool set = inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY);
            check_count++;
            if ((asked == NULL) != set || (asked != NULL && strcmp(asked, inidoc_why(doc)) != 0)) {
                check_failures++;
                fprintf(stderr, "test_inidoc.c:%d: text %d, %d bytes: asked \"%s\", the set said %s \"%s\"\n", __LINE__,
                    (int) t, (int) length, asked != NULL ? asked : "(null)", set ? "yes" : "no", inidoc_why(doc));
            }
            inidoc_free(doc);
        }
    }

    // A section too long for its header is refused before it is added
    char section[256];
    memset(section, 'S', 198);
    section[198] = '\0';
    IniDoc *doc = inidoc_parse("[General]\n", 10);
    CHECK(inidoc_check_in(doc, section, "Rows", "2") != NULL);
    CHECK(!inidoc_set(doc, section, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_check_in(doc, section, "Rows", "2"), inidoc_why(doc));
    inidoc_free(doc);
}

int main(void)
{
    test_round_trip_is_identical();
    test_get_reads_like_inih();
    test_edits();
    test_continuations();
    test_line_limit();
    test_long_section_name();
    test_check();
    test_long_names_agreeing_are_one_section();
    test_safe_places();
    test_empty_key_name();
    test_new_header_limit();
    test_embedded_nul();
    test_long_key_continuations();
    test_why();
    test_check_in_agrees_with_set();
    return check_report();
}
