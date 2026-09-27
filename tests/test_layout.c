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

int main(void)
{
    test_resolve();
    test_parse_count();
    test_compute_strip_matches_today();
    test_compute_no_cap_fills();
    test_compute_grid_height_limited();
    test_compute_padding_caps();
    test_compute_reduces_overflowing_axis();
    test_compute_fails_when_nothing_fits();
    test_compute_centring();
    test_compute_vcenter_clamp();
    test_compute_clock_band();
    return check_report();
}
