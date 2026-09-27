#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "layout.h"

// A function to pick each setting from the menu, else from [Layout], else the built-in default
LayoutOverrides layout_resolve(LayoutOverrides menu, LayoutOverrides global, LayoutOverrides builtin)
{
    LayoutOverrides result;
    result.rows = menu.rows ? menu.rows : (global.rows ? global.rows : builtin.rows);
    result.columns = menu.columns ? menu.columns : (global.columns ? global.columns : builtin.columns);
    result.icon_cap = menu.icon_cap ? menu.icon_cap : (global.icon_cap ? global.icon_cap : builtin.icon_cap);
    return result;
}

// A function to read a positive whole number, rejecting anything else ("3x", "-1", "0", "")
bool layout_parse_count(const char *value, int *count)
{
    if (value == NULL || value[0] == '\0' || strlen(value) > 6)
        return false;
    for (const char *p = value; *p != '\0'; p++) {
        if (!isdigit((unsigned char) *p))
            return false;
    }
    int number = atoi(value);
    if (number <= 0)
        return false;
    *count = number;
    return true;
}

// A function to return the smaller of two ints
static int min_int(int a, int b)
{
    return a < b ? a : b;
}

// A function to return the larger of two ints
static int max_int(int a, int b)
{
    return a > b ? a : b;
}

// A function to find the largest button for which `count` slots fit in `length` px, with
// `spacing` between slots, `pad` px of highlight at each end and `extra` px under each slot
static int fit(int length, int count, int spacing, int pad, int extra)
{
    return (length - (count - 1) * spacing - 2 * pad) / count - extra;
}

// A function to size and place a menu's buttons for its grid shape
int layout_compute(const LayoutParams *params, const LayoutArea *area, int entry_count,
                   LayoutGeometry *geometry, char *why, size_t why_size)
{
    LayoutGeometry g;
    int spacing = max_int(params->spacing, 0);
    int cap = params->icon_cap > 0 ? min_int(params->icon_cap, LAYOUT_MAX_BUTTON) : LAYOUT_MAX_BUTTON;
    int width_fit, height_fit;
    if (why_size > 0)
        why[0] = '\0';

    // Shrink only the axis that overflows until the smallest button fits. Highlight padding
    // is capped at half the gap between buttons; rows only have a gap between them in a grid.
    g.rows = max_int(params->rows, 1);
    g.columns = max_int(params->columns, 1);
    g.hpad = min_int(max_int(params->hpad, 0), spacing / 2);
    for (;;) {
        g.vpad = max_int(params->vpad, 0);
        if (g.rows > 1)
            g.vpad = min_int(g.vpad, spacing / 2);
        width_fit = fit(area->w, g.columns, spacing, g.hpad, 0);
        height_fit = fit(area->h, g.rows, spacing, g.vpad, params->title_block);
        if (width_fit < LAYOUT_MIN_BUTTON && g.columns > 1)
            g.columns--;
        else if (height_fit < LAYOUT_MIN_BUTTON && g.rows > 1)
            g.rows--;
        else
            break;
    }
    g.button = min_int(cap, min_int(width_fit, height_fit));
    if (g.button < LAYOUT_MIN_BUTTON) {
        if (why_size > 0)
            snprintf(why, why_size, "not even one %i px button fits in %i x %i px",
                LAYOUT_MIN_BUTTON, area->w, area->h);
        return -1;
    }
    if ((g.columns != params->columns || g.rows != params->rows) && why_size > 0)
        snprintf(why, why_size, "not enough screen space for %i x %i buttons, reducing to %i x %i",
            params->columns, params->rows, g.columns, g.rows);

    // Centre the block horizontally on the configured columns, so columns stay put as it
    // scrolls. A menu that fills less than one row is centred on its own buttons instead.
    int count = max_int(entry_count, 1);
    int used_columns = min_int(count, g.columns);
    int used_rows = min_int(g.rows, (count + g.columns - 1) / g.columns);
    int block_w = used_columns * g.button + (used_columns - 1) * spacing;
    int block_h = used_rows * (g.button + params->title_block) + (used_rows - 1) * spacing;
    g.x_advance = g.button + spacing;
    g.y_advance = g.button + params->title_block + spacing;
    g.x_origin = area->x + (area->w - block_w) / 2;

    // Centre the occupied rows on VCenter, then keep the block and its highlight inside the area
    g.y_origin = area->vcenter - block_h / 2;
    g.y_origin = min_int(g.y_origin, area->y + area->h - g.vpad - block_h);
    g.y_origin = max_int(g.y_origin, area->y + g.vpad);

    *geometry = g;
    return 0;
}
