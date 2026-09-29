#include <string.h>
#include "check.h"
#include "chroma.h"

// A function to tell whether a pixel of an RGBA buffer holds a colour
static int pixel_is(const unsigned char *pixel, int r, int g, int b, int a)
{
    return pixel[0] == r && pixel[1] == g && pixel[2] == b && pixel[3] == a;
}

// A function to test the default key, #010101: an opaque key pixel loses one step of red, the
// first channel nearest 0; a pixel with any transparency, or one step off in any channel, stays
static void test_default_key(void)
{
    unsigned char rgba[] = {
        1, 1, 1, 255,     // the key: moves
        1, 1, 1, 254,     // not fully opaque: stays
        1, 1, 1, 0,       // transparent: stays
        1, 1, 2, 255,     // one step off already: stays
        0, 0, 0, 255      // black: stays
    };
    CHECK_INT(chroma_keep_off(rgba, 5, 1, (int) sizeof(rgba), 1, 1, 1), 1);
    CHECK(pixel_is(rgba, 0, 1, 1, 255));
    CHECK(pixel_is(rgba + 4, 1, 1, 1, 254));
    CHECK(pixel_is(rgba + 8, 1, 1, 1, 0));
    CHECK(pixel_is(rgba + 12, 1, 1, 2, 255));
    CHECK(pixel_is(rgba + 16, 0, 0, 0, 255));
}

// A function to test the step for other keys: the channel nearest 0 or 255 moves towards that
// end, or away from it when it is already there
static void test_other_keys(void)
{
    unsigned char black[] = { 0, 0, 0, 255 };
    CHECK_INT(chroma_keep_off(black, 1, 1, 4, 0, 0, 0), 1);
    CHECK(pixel_is(black, 1, 0, 0, 255));

    unsigned char white[] = { 255, 255, 255, 255 };
    CHECK_INT(chroma_keep_off(white, 1, 1, 4, 255, 255, 255), 1);
    CHECK(pixel_is(white, 254, 255, 255, 255));

    unsigned char magenta[] = { 255, 0, 255, 255 };
    CHECK_INT(chroma_keep_off(magenta, 1, 1, 4, 255, 0, 255), 1);
    CHECK(pixel_is(magenta, 254, 0, 255, 255));

    unsigned char green[] = { 200, 10, 128, 255 };   // green is nearest an end (10 from 0)
    CHECK_INT(chroma_keep_off(green, 1, 1, 4, 200, 10, 128), 1);
    CHECK(pixel_is(green, 200, 9, 128, 255));

    unsigned char blue[] = { 128, 127, 250, 255 };   // blue is nearest an end (5 from 255)
    CHECK_INT(chroma_keep_off(blue, 1, 1, 4, 128, 127, 250), 1);
    CHECK(pixel_is(blue, 128, 127, 251, 255));
}

// A function to test that the rows' padding (pitch past the width) is never read as a pixel or
// written, and that every row is visited
static void test_pitch(void)
{
    // Two rows of two pixels, each row padded to 12 bytes with key-coloured bytes
    unsigned char rgba[24];
    for (size_t i = 0; i < sizeof(rgba); i++)
        rgba[i] = (i % 4 == 3) ? 255 : 1;
    CHECK_INT(chroma_keep_off(rgba, 2, 2, 12, 1, 1, 1), 4);
    CHECK(pixel_is(rgba, 0, 1, 1, 255));
    CHECK(pixel_is(rgba + 4, 0, 1, 1, 255));
    CHECK(pixel_is(rgba + 8, 1, 1, 1, 255));    // padding, untouched
    CHECK(pixel_is(rgba + 12, 0, 1, 1, 255));
    CHECK(pixel_is(rgba + 16, 0, 1, 1, 255));
    CHECK(pixel_is(rgba + 20, 1, 1, 1, 255));   // padding, untouched
}

// A function to test that an image with no key pixel, or no pixels at all, is left as it was
static void test_nothing_to_move(void)
{
    unsigned char rgba[] = { 10, 20, 30, 255, 1, 1, 1, 128 };
    unsigned char before[sizeof(rgba)];
    memcpy(before, rgba, sizeof(rgba));
    CHECK_INT(chroma_keep_off(rgba, 2, 1, (int) sizeof(rgba), 1, 1, 1), 0);
    CHECK(memcmp(rgba, before, sizeof(rgba)) == 0);
    CHECK_INT(chroma_keep_off(rgba, 0, 0, 0, 1, 1, 1), 0);
    CHECK(memcmp(rgba, before, sizeof(rgba)) == 0);
}

// A function to test that a second pass moves nothing: a moved pixel is off the key for good
static void test_idempotent(void)
{
    unsigned char rgba[] = { 1, 1, 1, 255, 1, 1, 1, 255 };
    CHECK_INT(chroma_keep_off(rgba, 2, 1, 8, 1, 1, 1), 2);
    CHECK_INT(chroma_keep_off(rgba, 2, 1, 8, 1, 1, 1), 0);
}

int main(void)
{
    test_default_key();
    test_other_keys();
    test_pitch();
    test_nothing_to_move();
    test_idempotent();
    return check_report();
}
