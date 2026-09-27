#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "layout.h"

// A function to test that settings come from the menu, then [Layout], then the default,
// with all three levels set at once so the test proves which one wins
static void test_resolve(void)
{
    LayoutOverrides menu = { 3, 6, 200 };
    LayoutOverrides global = { 2, 5, 256 };
    LayoutOverrides builtin = { 1, 4, 0 };
    LayoutOverrides none = { 0, 0, 0 };
    LayoutOverrides r;

    r = layout_resolve(menu, global, builtin);
    CHECK_INT(r.rows, 3);
    CHECK_INT(r.columns, 6);
    CHECK_INT(r.icon_cap, 200);

    r = layout_resolve(none, global, builtin);
    CHECK_INT(r.rows, 2);
    CHECK_INT(r.columns, 5);
    CHECK_INT(r.icon_cap, 256);

    r = layout_resolve(none, none, builtin);
    CHECK_INT(r.rows, 1);
    CHECK_INT(r.columns, 4);
    CHECK_INT(r.icon_cap, 0);

    // Each field resolves on its own
    LayoutOverrides menu_rows = { 3, 0, 0 };
    LayoutOverrides global_columns = { 0, 5, 0 };
    r = layout_resolve(menu_rows, global_columns, builtin);
    CHECK_INT(r.rows, 3);
    CHECK_INT(r.columns, 5);
    CHECK_INT(r.icon_cap, 0);
}

// A function to test that only a positive whole number counts as a Rows/Columns/IconSize value
static void test_parse_count(void)
{
    int n = -7;
    CHECK(layout_parse_count("3", &n));
    CHECK_INT(n, 3);
    CHECK(layout_parse_count("12", &n));
    CHECK_INT(n, 12);
    CHECK(!layout_parse_count("0", &n));
    CHECK(!layout_parse_count("-1", &n));
    CHECK(!layout_parse_count("3x", &n));
    CHECK(!layout_parse_count("", &n));
    CHECK(!layout_parse_count(" 3", &n));
    CHECK(!layout_parse_count("1234567", &n));
    CHECK(!layout_parse_count("Plex;plex.png;cmd", &n));
    CHECK(!layout_parse_count(NULL, &n));
    CHECK_INT(n, 12); // A rejected value leaves the output alone
}

// A function to test that IconSize takes only a whole number from LAYOUT_MIN_BUTTON to
// LAYOUT_MAX_BUTTON, so "200px" or "2000" is refused instead of half-read
static void test_parse_icon_size(void)
{
    int n = -7;
    CHECK(layout_parse_icon_size("256", &n));
    CHECK_INT(n, 256);
    CHECK(layout_parse_icon_size("32", &n));
    CHECK_INT(n, 32);
    CHECK(layout_parse_icon_size("1024", &n));
    CHECK_INT(n, 1024);
    CHECK(!layout_parse_icon_size("31", &n));
    CHECK(!layout_parse_icon_size("1025", &n));
    CHECK(!layout_parse_icon_size("200px", &n));
    CHECK(!layout_parse_icon_size("abc", &n));
    CHECK(!layout_parse_icon_size("", &n));
    CHECK(!layout_parse_icon_size(NULL, &n));
    CHECK_INT(n, 1024); // A rejected value leaves the output alone
}

static const LayoutArea SCREEN_1080 = { 0, 54, 1920, 972, 540 };

static LayoutParams params(int rows, int columns, int icon_cap)
{
    LayoutParams p = { rows, columns, icon_cap, 96, 60, 30, 30 };
    return p;
}

// A function to test that a one-row strip with IconSize set lands exactly where today's layout puts it
static void test_compute_strip_matches_today(void)
{
    LayoutParams p = params(1, 4, 256);
    LayoutGeometry g;
    char why[128];
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, why, sizeof(why)), 0);
    CHECK_INT(g.button, 256);
    CHECK_INT(g.x_origin, 304);  // today: (1920 - 4*256 - 4*96 + 96) / 2
    CHECK_INT(g.y_origin, 382);  // today: 540 - (256 + 60) / 2
    CHECK_INT(g.x_advance, 352);
    CHECK_INT(g.y_advance, 412);
    CHECK_INT(g.rows, 1);
    CHECK_INT(g.columns, 4);
    CHECK(why[0] == '\0');
}

// A function to test that with no IconSize, buttons grow until the width runs out
static void test_compute_no_cap_fills(void)
{
    LayoutParams p = params(1, 4, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.button, 393);    // (1920 - 3*96 - 2*30) / 4
}

// A function to test that a tall grid is limited by the height, titles included
static void test_compute_grid_height_limited(void)
{
    LayoutParams p = params(3, 6, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 18, &g, NULL, 0), 0);
    CHECK_INT(g.button, 180);    // (972 - 2*96 - 2*30) / 3 - 60
    CHECK_INT(g.x_origin, 180);  // (1920 - (6*180 + 5*96)) / 2
    CHECK_INT(g.y_origin, 84);   // centred at 540, then kept inside: 54 + 30
    CHECK_INT(g.y_advance, 336); // 180 + 60 + 96
}

// A function to test that highlight padding is capped at half the gap, vertically only in grids
static void test_compute_padding_caps(void)
{
    LayoutParams p = params(1, 4, 256);
    LayoutGeometry g;
    p.spacing = 40;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.hpad, 20);
    CHECK_INT(g.vpad, 30);       // A strip has no row gap to protect
    p.rows = 2;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.hpad, 20);
    CHECK_INT(g.vpad, 20);
    p.hpad = -1;
    p.vpad = -1;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 6, &g, NULL, 0), 0);
    CHECK_INT(g.hpad, 0);        // An unset (negative) padding counts as 0
    CHECK_INT(g.vpad, 0);
}

// A function to test that only the overflowing axis is reduced, and that the log gets a reason
static void test_compute_reduces_overflowing_axis(void)
{
    LayoutArea narrow = { 0, 0, 300, 972, 486 };
    LayoutParams p = params(1, 6, 0);
    LayoutGeometry g;
    char why[128];
    CHECK_INT(layout_compute(&p, &narrow, 6, &g, why, sizeof(why)), 0);
    CHECK_INT(g.columns, 2);     // 3 columns give 16 px, 2 give 72 px
    CHECK_INT(g.rows, 1);
    CHECK_INT(g.button, 72);
    CHECK(strstr(why, "reducing to 2 x 1") != NULL);

    LayoutArea short_area = { 0, 54, 1920, 300, 200 };
    p = params(3, 4, 0);
    CHECK_INT(layout_compute(&p, &short_area, 12, &g, why, sizeof(why)), 0);
    CHECK_INT(g.rows, 1);
    CHECK_INT(g.columns, 4);     // The width was never the problem
}

// A function to test that the call fails, and says why, when not even one button fits
static void test_compute_fails_when_nothing_fits(void)
{
    LayoutArea tiny = { 0, 0, 40, 972, 486 };
    LayoutParams p = params(1, 1, 0);
    LayoutGeometry g = { 0 };
    char why[128];
    CHECK(layout_compute(&p, &tiny, 1, &g, why, sizeof(why)) != 0);
    CHECK(why[0] != '\0');
    CHECK_INT(g.button, 0);      // Untouched on failure
}

// A function to test that huge Rows, Columns, IconSpacing and VPadding keep the arithmetic inside
// int: the gap is capped at the area, and no more slots are tried than 32 px buttons could fill
static void test_compute_limits(void)
{
    LayoutParams p = params(999999, 999999, 0);
    LayoutGeometry g = { 0 };
    char why[128];
    p.spacing = 2000000000;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 3, &g, why, sizeof(why)), 0);
    CHECK_INT(g.columns, 1);     // A 1920 px gap leaves room for one column
    CHECK_INT(g.rows, 1);
    CHECK_INT(g.button, 852);    // 972 - 2*30 - 60, from the height
    CHECK(strstr(why, "for 999999 x 999999 buttons, reducing to 1 x 1") != NULL);

    // A strip's vertical padding is not capped by the gap, so it is capped by the area: a
    // padding taller than the screen leaves no room, as VPadding=2000 already does
    p = params(1, 4, 0);
    p.vpad = 2000000000;
    CHECK(layout_compute(&p, &SCREEN_1080, 3, &g, why, sizeof(why)) != 0);
    CHECK(strstr(why, "not even one") != NULL);
}

// A function to test centring: a partial single row on its own buttons, a partial last row on the columns
static void test_compute_centring(void)
{
    LayoutParams p = params(3, 6, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 4, &g, NULL, 0), 0);
    CHECK_INT(g.x_origin, 456);  // (1920 - (4*180 + 3*96)) / 2
    CHECK_INT(g.y_origin, 420);  // one occupied row: 540 - 240 / 2
    CHECK_INT(layout_compute(&p, &SCREEN_1080, 8, &g, NULL, 0), 0);
    CHECK_INT(g.x_origin, 180);  // columns stay put
    CHECK_INT(g.y_origin, 252);  // two occupied rows: 540 - 576 / 2
}

// A function to test that a grid stays inside the area whatever VCenter says,
// and that a block which fits is not moved
static void test_compute_vcenter_clamp(void)
{
    LayoutParams p = params(2, 4, 0);
    LayoutGeometry g;
    LayoutArea high = SCREEN_1080;
    LayoutArea low = SCREEN_1080;
    high.vcenter = 270;
    low.vcenter = 810;
    CHECK_INT(layout_compute(&p, &high, 8, &g, NULL, 0), 0);
    CHECK_INT(g.y_origin, 84);
    CHECK_INT(layout_compute(&p, &low, 8, &g, NULL, 0), 0);
    CHECK_INT(g.y_origin, 84);   // 84 + 912 + 30 is exactly the area's bottom

    p = params(1, 4, 256);
    CHECK_INT(layout_compute(&p, &high, 6, &g, NULL, 0), 0);
    CHECK_INT(g.y_origin, 112);  // 270 - 316 / 2, unchanged
}

// A function to test that the clock band shrinks a tall grid and keeps it below the clock
static void test_compute_clock_band(void)
{
    LayoutArea below_clock = { 0, 150, 1920, 876, 540 };
    LayoutParams p = params(3, 6, 0);
    LayoutGeometry g;
    CHECK_INT(layout_compute(&p, &below_clock, 18, &g, NULL, 0), 0);
    CHECK_INT(g.button, 148);    // (876 - 192 - 60) / 3 - 60
    CHECK_INT(g.y_origin, 180);  // 150 + 30
}

static LayoutGeometry shape(int rows, int columns)
{
    LayoutGeometry g = { 0 };
    g.rows = rows;
    g.columns = columns;
    g.button = 100;
    g.x_advance = 120;
    g.y_advance = 150;
    g.x_origin = 10;
    g.y_origin = 20;
    return g;
}

static LayoutPosition at(int selected, int first)
{
    LayoutPosition p = { selected, first };
    return p;
}

#define CHECK_POS(position, want_selected, want_first) do { \
    LayoutPosition p_ = (position); \
    CHECK_INT(p_.selected, (want_selected)); \
    CHECK_INT(p_.first, (want_first)); \
} while (0)

// A function to test a one-row strip of 4 visible buttons holding 6 entries
static void test_strip_moves(void)
{
    LayoutGeometry g = shape(1, 4);
    CHECK_POS(layout_move(&g, 6, at(0, 0), LAYOUT_RIGHT, false), 1, 0);
    CHECK_POS(layout_move(&g, 6, at(3, 0), LAYOUT_RIGHT, false), 4, 1);  // slides one button
    CHECK_POS(layout_move(&g, 6, at(4, 1), LAYOUT_RIGHT, false), 5, 2);
    CHECK_POS(layout_move(&g, 6, at(5, 2), LAYOUT_RIGHT, false), 5, 2);  // end, no wrap
    CHECK_POS(layout_move(&g, 6, at(5, 2), LAYOUT_RIGHT, true), 0, 0);   // wraps to the first
    CHECK_POS(layout_move(&g, 6, at(0, 0), LAYOUT_LEFT, false), 0, 0);
    CHECK_POS(layout_move(&g, 6, at(0, 0), LAYOUT_LEFT, true), 5, 2);    // wraps to the last
    CHECK_POS(layout_move(&g, 6, at(2, 2), LAYOUT_LEFT, false), 1, 1);
    CHECK_POS(layout_move(&g, 6, at(2, 0), LAYOUT_UP, true), 2, 0);      // no rows to move between
    CHECK_POS(layout_move(&g, 6, at(2, 0), LAYOUT_DOWN, true), 2, 0);
    CHECK_POS(layout_move(&g, 3, at(2, 0), LAYOUT_RIGHT, false), 2, 0);  // fewer entries than columns
}

// A function to test where strip buttons are drawn and when the strip can scroll
static void test_strip_slots_and_scroll(void)
{
    LayoutGeometry g = shape(1, 4);
    int x = -1, y = -1;
    CHECK(layout_slot(&g, at(4, 1), 1, &x, &y));
    CHECK_INT(x, 10);
    CHECK_INT(y, 20);
    CHECK(layout_slot(&g, at(4, 1), 4, &x, &y));
    CHECK_INT(x, 370);
    CHECK(!layout_slot(&g, at(4, 1), 0, &x, &y));
    CHECK(!layout_slot(&g, at(4, 1), 5, &x, &y));
    CHECK(!layout_can_scroll(&g, 6, at(0, 0), LAYOUT_LEFT));
    CHECK(layout_can_scroll(&g, 6, at(0, 0), LAYOUT_RIGHT));
    CHECK(layout_can_scroll(&g, 6, at(5, 2), LAYOUT_LEFT));
    CHECK(!layout_can_scroll(&g, 6, at(5, 2), LAYOUT_RIGHT));
    CHECK(!layout_can_scroll(&g, 6, at(0, 0), LAYOUT_UP));
    CHECK(!layout_can_scroll(&g, 3, at(0, 0), LAYOUT_RIGHT));
}

// A function to test a 2-row, 4-column grid holding 10 entries: rows [0-3], [4-7], [8,9]
static void test_grid_moves(void)
{
    LayoutGeometry g = shape(2, 4);
    CHECK_POS(layout_move(&g, 10, at(3, 0), LAYOUT_RIGHT, false), 3, 0);  // stops at the row edge
    CHECK_POS(layout_move(&g, 10, at(3, 0), LAYOUT_RIGHT, true), 0, 0);   // wraps within the row
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_RIGHT, true), 8, 1);   // within the short last row
    CHECK_POS(layout_move(&g, 10, at(4, 0), LAYOUT_LEFT, false), 4, 0);
    CHECK_POS(layout_move(&g, 10, at(4, 0), LAYOUT_LEFT, true), 7, 0);
    CHECK_POS(layout_move(&g, 10, at(1, 0), LAYOUT_DOWN, false), 5, 0);
    CHECK_POS(layout_move(&g, 10, at(5, 0), LAYOUT_DOWN, false), 9, 1);   // scrolls one row
    CHECK_POS(layout_move(&g, 10, at(7, 0), LAYOUT_DOWN, false), 9, 1);   // lands on the short row's last
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_DOWN, false), 9, 1);
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_DOWN, true), 1, 0);    // wraps to the first row
    CHECK_POS(layout_move(&g, 10, at(1, 0), LAYOUT_UP, false), 1, 0);
    CHECK_POS(layout_move(&g, 10, at(3, 0), LAYOUT_UP, true), 9, 1);      // wraps to the last row
    CHECK_POS(layout_move(&g, 10, at(9, 1), LAYOUT_UP, false), 5, 1);     // row 1 is still in view
    CHECK_POS(layout_move(&g, 10, at(5, 1), LAYOUT_UP, false), 1, 0);     // scrolls back
    CHECK_POS(layout_move(&g, 3, at(1, 0), LAYOUT_DOWN, false), 1, 0);    // one partial row only
}

// A function to test where grid buttons are drawn and when the grid can scroll
static void test_grid_slots_and_scroll(void)
{
    LayoutGeometry g = shape(2, 4);
    int x = -1, y = -1;
    CHECK(layout_slot(&g, at(9, 1), 4, &x, &y));
    CHECK_INT(x, 10);
    CHECK_INT(y, 20);
    CHECK(layout_slot(&g, at(9, 1), 9, &x, &y));
    CHECK_INT(x, 130);
    CHECK_INT(y, 170);
    CHECK(!layout_slot(&g, at(9, 1), 3, &x, &y));
    CHECK(!layout_slot(&g, at(9, 1), -1, &x, &y));
    CHECK(layout_can_scroll(&g, 10, at(0, 0), LAYOUT_DOWN));
    CHECK(!layout_can_scroll(&g, 10, at(0, 0), LAYOUT_UP));
    CHECK(!layout_can_scroll(&g, 10, at(0, 0), LAYOUT_LEFT));
    CHECK(!layout_can_scroll(&g, 10, at(9, 1), LAYOUT_DOWN));
    CHECK(layout_can_scroll(&g, 10, at(9, 1), LAYOUT_UP));
    CHECK(!layout_can_scroll(&g, 3, at(0, 0), LAYOUT_DOWN));
}

// A function to test that clamping keeps the selection real and visible after a re-layout
static void test_clamp(void)
{
    LayoutGeometry strip = shape(1, 4);
    LayoutGeometry taller = shape(3, 4);
    CHECK_POS(layout_clamp(&strip, 6, at(9, 0)), 5, 2);
    CHECK_POS(layout_clamp(&strip, 6, at(-3, 5)), 0, 0);
    CHECK_POS(layout_clamp(&taller, 10, at(9, 1)), 9, 0);  // 3 rows show everything now
    CHECK_POS(layout_clamp(&strip, 0, at(4, 2)), 0, 0);
}

int main(void)
{
    test_resolve();
    test_parse_count();
    test_parse_icon_size();
    test_compute_strip_matches_today();
    test_compute_no_cap_fills();
    test_compute_grid_height_limited();
    test_compute_padding_caps();
    test_compute_reduces_overflowing_axis();
    test_compute_fails_when_nothing_fits();
    test_compute_limits();
    test_compute_centring();
    test_compute_vcenter_clamp();
    test_compute_clock_band();
    test_strip_moves();
    test_strip_slots_and_scroll();
    test_grid_moves();
    test_grid_slots_and_scroll();
    test_clamp();
    return check_report();
}
