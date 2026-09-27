#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#include <ini.h>
#include "check.h"

// Every generic icon listed in the shipped manifest must parse with nanosvg, the launcher's own SVG
// parser, into a 512 px plate plus a glyph that stays on it. Catches SVG features nanosvg cannot draw.

#define MAX_FILES 128
static char files[MAX_FILES][512];
static int file_count = 0;

static int collect(void *user, const char *section, const char *name, const char *value)
{
    (void) user;
    (void) section;
    size_t length = strlen(value);
    if (!strcmp(name, "file") && length > 4 && !strcmp(value + length - 4, ".svg") && file_count < MAX_FILES) {
        snprintf(files[file_count], sizeof(files[0]), "%s/%s", LIBRARY_ROOT, value);
        file_count++;
    }
    return 1;
}

int main(void)
{
    CHECK_INT(ini_parse(LIBRARY_ROOT "/icons.ini", collect, NULL), 0);
    CHECK(file_count >= 36);
    for (int i = 0; i < file_count; i++) {
        NSVGimage *image = nsvgParseFromFile(files[i], "px", 96.0f);
        CHECK(image != NULL);
        if (image == NULL) {
            fprintf(stderr, "  could not parse %s\n", files[i]);
            continue;
        }
        CHECK((int) image->width == 512 && (int) image->height == 512);
        int shapes = 0;
        float glyph_area = 0.0f;
        for (NSVGshape *shape = image->shapes; shape != NULL; shape = shape->next) {
            shapes++;
            bool inside = shape->bounds[0] >= -0.5f && shape->bounds[1] >= -0.5f
                       && shape->bounds[2] <= 512.5f && shape->bounds[3] <= 512.5f;
            if (!inside)
                fprintf(stderr, "  %s: shape %d leaves the plate\n", files[i], shapes);
            CHECK(inside);
            if (shapes > 1)
                glyph_area += (shape->bounds[2] - shape->bounds[0]) * (shape->bounds[3] - shape->bounds[1]);
        }
        if (shapes < 2)
            fprintf(stderr, "  %s: %d shape(s), expected a plate and a glyph\n", files[i], shapes);
        CHECK(shapes >= 2);
        // The glyph's box should cover a real share of the plate: a misparsed path is tiny or huge
        CHECK(glyph_area > 0.05f * 512 * 512 && glyph_area < 0.60f * 512 * 512);
        nsvgDelete(image);
    }
    return check_report();
}
