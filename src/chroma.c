#include <stddef.h>
#include "chroma.h"

// A function to tell how far a channel is from the nearer end of its range, 0 or 255
static int distance_to_end(unsigned char value)
{
    return value < 128 ? value : 255 - value;
}

// A function to move one channel of a colour a step, so it is no longer the key: the channel
// nearest 0 or 255 (the first of them on a tie) moves one step towards that end, or away from it
// when it is already there. The colour changes by the least a pixel can.
static void step_off(unsigned char *rgb)
{
    int channel = 0;
    for (int i = 1; i < 3; i++) {
        if (distance_to_end(rgb[i]) < distance_to_end(rgb[channel]))
            channel = i;
    }
    unsigned char value = rgb[channel];
    if (value == 0)
        rgb[channel] = 1;
    else if (value == 255)
        rgb[channel] = 254;
    else if (value < 128)
        rgb[channel] = (unsigned char) (value - 1);
    else
        rgb[channel] = (unsigned char) (value + 1);
}

// A function to move each fully opaque pixel of an RGBA image exactly equal to the key a step off
// it; a pixel with any transparency, or of any other colour, is left alone. `pitch` is the bytes
// from one row to the next. Returns how many pixels moved.
int chroma_keep_off(unsigned char *rgba, int width, int height, int pitch,
                    unsigned char key_r, unsigned char key_g, unsigned char key_b)
{
    int moved = 0;
    for (int y = 0; y < height; y++) {
        unsigned char *pixel = rgba + (size_t) y * (size_t) pitch;
        for (int x = 0; x < width; x++, pixel += 4) {
            if (pixel[3] == 255 && pixel[0] == key_r && pixel[1] == key_g && pixel[2] == key_b) {
                step_off(pixel);
                moved++;
            }
        }
    }
    return moved;
}
