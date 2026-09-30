#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "listpick.h"

// A function to make a picker of the rows a small command picker holds
static ListPick *sample(void)
{
    ListPick *pick = listpick_create();
    CHECK(listpick_add(pick, "None", "", true, NULL));
    CHECK(listpick_add(pick, "Left", ":left", true, NULL));
    CHECK(listpick_add(pick, "Right", ":right", true, NULL));
    CHECK(listpick_add(pick, "Quit StreamFlex", ":quit", true, NULL));
    CHECK(listpick_add(pick, "Close the app on show", ":exit", false, "Only on Windows"));
    CHECK(listpick_add(pick, "Kodi", "kodi --standalone", true, NULL));
    return pick;
}

// A function to test moving, paging and the ends
static void test_moves(void)
{
    ListPick *pick = sample();
    CHECK_INT(listpick_count(pick), 6);
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_UP, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 1);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 4);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 5);                     // The last row, not past it
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_UP, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 2);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_UP, 0), LISTPICK_MOVED);   // A page is at least one row
    CHECK_INT(listpick_cursor(pick), 1);
    listpick_free(pick);
}

// A function to test choosing, a row that cannot be chosen, and cancelling
static void test_choose(void)
{
    ListPick *pick = sample();
    CHECK(listpick_select(pick, ":quit", "Custom: :quit"));
    CHECK_INT(listpick_cursor(pick), 3);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), ":quit");

    // A row that cannot be chosen takes the cursor, and OK says why
    listpick_command(pick, LISTPICK_DOWN, 3);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_NONE);
    CHECK_STR(listpick_why(pick), "Only on Windows");
    listpick_command(pick, LISTPICK_DOWN, 3);
    CHECK(listpick_why(pick) == NULL);                        // Moving clears the reason
    CHECK_INT(listpick_command(pick, LISTPICK_BACK, 3), LISTPICK_CANCELLED);

    // None chooses the empty value
    CHECK(listpick_select(pick, "", "Custom: "));
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "");
    listpick_free(pick);
}

// A function to test a value that matches no row: pinned first as Custom, chosen as itself
static void test_custom(void)
{
    ListPick *pick = sample();
    CHECK(!listpick_has(pick, "retroarch -f"));
    CHECK(listpick_select(pick, "retroarch -f", "Custom: retroarch -f"));
    CHECK_INT(listpick_count(pick), 7);
    const ListPickRow *row = listpick_row(pick, 0);
    CHECK(row->custom);
    CHECK_STR(row->label, "Custom: retroarch -f");
    CHECK_STR(row->value, "retroarch -f");
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_STR(listpick_row(pick, 1)->label, "None");        // The rest keep their order
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "retroarch -f");

    // Selecting again does not pin a second one, and a value that matches unpins it
    CHECK(listpick_select(pick, "retroarch -f", "Custom: retroarch -f"));
    CHECK_INT(listpick_count(pick), 7);
    CHECK(listpick_select(pick, ":left", "Custom: :left"));
    CHECK_INT(listpick_count(pick), 6);
    CHECK_STR(listpick_row(pick, listpick_cursor(pick))->value, ":left");
    CHECK(listpick_row(pick, 6) == NULL);
    listpick_free(pick);
}

// A function to test an empty picker, as the font picker is while it loads
static void test_empty(void)
{
    ListPick *pick = listpick_create();
    CHECK_INT(listpick_count(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_DOWN, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_BACK, 3), LISTPICK_CANCELLED);
    CHECK(listpick_row(pick, 0) == NULL);
    listpick_free(pick);
    listpick_free(NULL);
}

int main(void)
{
    test_moves();
    test_choose();
    test_custom();
    test_empty();
    return check_report();
}
