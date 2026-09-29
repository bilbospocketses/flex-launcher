// Keeping images off the chroma key. On Windows, Transparent mode makes every pixel of the window
// that is exactly the key colour see-through, so an icon's opaque pixel of that colour would be a
// hole in it. Pure: no SDL, so tests/test_chroma.c builds it on its own; the caller hands it the
// pixels as RGBA bytes.
#ifndef CHROMA_H
#define CHROMA_H

// Moves each fully opaque pixel exactly equal to the key one step off it; returns how many moved
int chroma_keep_off(unsigned char *rgba, int width, int height, int pitch,
                    unsigned char key_r, unsigned char key_g, unsigned char key_b);

#endif
