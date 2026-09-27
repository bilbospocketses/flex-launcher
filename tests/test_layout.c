#include <stdbool.h>
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

int main(void)
{
    test_resolve();
    test_parse_count();
    return check_report();
}
