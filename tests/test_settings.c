#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "settings.h"

#define ARROW " \xE2\x80\xBA "   // U+203A with a space either side, between pages in the page path
#define TIMES "\xC3\x97"         // U+00D7, the multiplication sign
#define ELLIPSIS "\xE2\x80\xA6"  // U+2026, the ellipsis

// A function to parse a value, failing the check when the parser refuses it
static SettingValue parsed(SettingId id, const char *text)
{
    SettingValue value;
    memset(&value, 0, sizeof(value));
    CHECK(setting_parse(setting_def(id), text, &value));
    return value;
}

// A function to write a value back out as config.ini would hold it
static const char *formatted(SettingId id, const SettingValue *value)
{
    static char text[SETTING_TEXT_MAX];
    setting_format(setting_def(id), value, text, sizeof(text));
    return text;
}

// A function to describe a value as the screen shows it
static const char *described(SettingId id, const SettingValue *value, const SettingValue *inherited)
{
    static char text[256];
    setting_describe(setting_def(id), value, inherited, text, sizeof(text));
    return text;
}

// A function to step a value several times one way
static SettingValue stepped(SettingId id, SettingValue value, const SettingValue *entry, int direction, int times)
{
    for (int i = 0; i < times; i++)
        value = setting_step(setting_def(id), &value, entry, direction);
    return value;
}

// A function to test that every type reads what the parser reads and writes it back unchanged
static void test_round_trips(void)
{
    static const struct { SettingId id; const char *text; } cases[] = {
        { SET_ID_LAYOUT_ROWS, "3" },
        { SET_ID_LAYOUT_COLUMNS, "12" },
        { SET_ID_LAYOUT_ROWS, "999999" },
        { SET_ID_LAYOUT_ICON_SIZE, "256" },
        { SET_ID_MENU_ICON_SIZE, "1024" },
        { SET_ID_BACKGROUND_MODE, "Slideshow" },
        { SET_ID_BACKGROUND_COLOR, "#1A2B3C" },
        { SET_ID_BACKGROUND_IMAGE, "C:\\My Pictures\\sunset.jpg" },
        { SET_ID_SLIDESHOW_DIRECTORY, "/home/me/Pictures" },
        { SET_ID_SLIDESHOW_DURATION, "30" },
        { SET_ID_SLIDESHOW_DURATION, "3600" },
        { SET_ID_SLIDESHOW_FADE, "0" },
        { SET_ID_SLIDESHOW_FADE, "0.5" },
        { SET_ID_SLIDESHOW_FADE, "1" },
        { SET_ID_SLIDESHOW_FADE, "1.5" },
        { SET_ID_SLIDESHOW_FADE, "2.5" },
        { SET_ID_SLIDESHOW_FADE, "3" },
        { SET_ID_SLIDESHOW_FADE, "1.25" },
        { SET_ID_SLIDESHOW_FADE, "0.123" },
        { SET_ID_TITLE_SIZE, "14%" },
        { SET_ID_TITLE_SIZE, "36" }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        SettingValue value = parsed(cases[i].id, cases[i].text);
        CHECK_STR(formatted(cases[i].id, &value), cases[i].text);
    }

    // What the parser reads from files written other ways
    SettingValue value = parsed(SET_ID_BACKGROUND_COLOR, "#1a2b3c");
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#1A2B3C");
    value = parsed(SET_ID_BACKGROUND_IMAGE, "\"C:\\My Pictures\\a.png\"");     // Quotes dropped, as clean_path does
    CHECK_STR(value.text, "C:\\My Pictures\\a.png");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30s");                          // atoi, as before
    CHECK_INT(value.number, 30);
    value = parsed(SET_ID_SLIDESHOW_FADE, "0.7");
    CHECK_INT(value.number, 700);
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.2346");
    CHECK_INT(value.number, 1235);                                             // Rounded to the ms
    value = parsed(SET_ID_SLIDESHOW_FADE, "2.9995");
    CHECK_INT(value.number, 3000);                                             // Rounds up to the maximum

    // Following the default writes nothing: the key is removed
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    CHECK_STR(formatted(SET_ID_MENU_ROWS, &inherit), "");
}

// A function to test the values the parser refuses
static void test_rejects(void)
{
    SettingValue value;
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ROWS), "0", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ROWS), "3x", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), "31", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), "200px", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_MODE), "color", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "#12345G", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "123456", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "#1234567", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_IMAGE), "", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_DURATION), "4", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_DURATION), "3601", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "-1", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "3.5", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "3.0005", &value));   // Would round past the maximum
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "-0.0004", &value));  // Negative, though it rounds to 0
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "nan", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "inf", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "-inf", &value));
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_SIZE), "0%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_SIZE), "abc", &value));
}

// A function to test how Left and Right step each type
static void test_steps(void)
{
    // Rows stop at their ends, and a value from the file past the last step stays reachable
    SettingValue one = parsed(SET_ID_LAYOUT_ROWS, "1");
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, -1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, 1, 1).number, 2);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, 1, 20).number, 10);
    SettingValue big = parsed(SET_ID_LAYOUT_ROWS, "25");
    SettingValue ten = stepped(SET_ID_LAYOUT_ROWS, big, &big, -1, 1);
    CHECK_INT(ten.number, 10);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, ten, &big, 1, 1).number, 25);

    // A menu's lowest step follows All menus
    SettingValue three = parsed(SET_ID_MENU_ROWS, "3");
    SettingValue value = stepped(SET_ID_MENU_ROWS, three, &three, -1, 3);
    CHECK(value.inherit);
    value = stepped(SET_ID_MENU_ROWS, value, &three, 1, 1);
    CHECK(!value.inherit);
    CHECK_INT(value.number, 1);

    // IconSize: Fill, then the steps, with a value from the file in its sorted place
    SettingValue fill;
    memset(&fill, 0, sizeof(fill));
    fill.inherit = true;
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, fill, &fill, 1, 1).number, 64);
    SettingValue odd = parsed(SET_ID_LAYOUT_ICON_SIZE, "200");
    SettingValue below = parsed(SET_ID_LAYOUT_ICON_SIZE, "192");
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, below, &odd, 1, 1).number, 200);
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, odd, &odd, 1, 1).number, 256);

    // Colours: a custom colour from the file first, then the presets in order
    SettingValue custom = parsed(SET_ID_BACKGROUND_COLOR, "#123456");
    value = stepped(SET_ID_BACKGROUND_COLOR, custom, &custom, 1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, value, &custom, 1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#1E1E1E");
    value = stepped(SET_ID_BACKGROUND_COLOR, value, &custom, -1, 2);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#123456");
    SettingValue black = parsed(SET_ID_BACKGROUND_COLOR, "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, black, &black, -1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, black, &black, 1, 20);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#4A1520");

    // Title size: a fixed size from the file, then Small, Medium and Large
    SettingValue fixed = parsed(SET_ID_TITLE_SIZE, "36");
    value = stepped(SET_ID_TITLE_SIZE, fixed, &fixed, 1, 1);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "11%");
    value = stepped(SET_ID_TITLE_SIZE, value, &fixed, 1, 5);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "17%");
    value = stepped(SET_ID_TITLE_SIZE, value, &fixed, -1, 5);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "36");

    // Durations and fades follow their step lists
    SettingValue thirty = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    CHECK_INT(stepped(SET_ID_SLIDESHOW_DURATION, thirty, &thirty, 1, 1).number, 60);
    SettingValue fade = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    CHECK_INT(stepped(SET_ID_SLIDESHOW_FADE, fade, &fade, 1, 1).number, 2000);

    // The mode stops at both ends; a path never steps
    SettingValue mode = parsed(SET_ID_BACKGROUND_MODE, "Color");
    CHECK_INT(stepped(SET_ID_BACKGROUND_MODE, mode, &mode, -1, 1).number, 0);
    CHECK_INT(stepped(SET_ID_BACKGROUND_MODE, mode, &mode, 1, 9).number, 3);
    SettingValue path = parsed(SET_ID_BACKGROUND_IMAGE, "/a.png");
    SettingValue same = stepped(SET_ID_BACKGROUND_IMAGE, path, &path, 1, 1);
    CHECK_STR(same.text, "/a.png");
}

// A function to test how values are described on screen
static void test_descriptions(void)
{
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    SettingValue four = parsed(SET_ID_LAYOUT_ROWS, "4");
    CHECK_STR(described(SET_ID_MENU_ROWS, &inherit, &four), "All menus (4)");
    SettingValue value = parsed(SET_ID_MENU_ROWS, "3");
    CHECK_STR(described(SET_ID_MENU_ROWS, &value, &four), "3");
    CHECK_STR(described(SET_ID_LAYOUT_ICON_SIZE, &inherit, NULL), "Fill");
    CHECK_STR(described(SET_ID_MENU_ICON_SIZE, &inherit, &inherit), "All menus (Fill)");
    SettingValue cap = parsed(SET_ID_LAYOUT_ICON_SIZE, "256");
    CHECK_STR(described(SET_ID_MENU_ICON_SIZE, &inherit, &cap), "All menus (256 px)");
    CHECK_STR(described(SET_ID_LAYOUT_ICON_SIZE, &cap, NULL), "256 px");
    value = parsed(SET_ID_BACKGROUND_MODE, "Color");
    CHECK_STR(described(SET_ID_BACKGROUND_MODE, &value, NULL), "Colour");
    value = parsed(SET_ID_BACKGROUND_COLOR, "#1E1E1E");
    CHECK_STR(described(SET_ID_BACKGROUND_COLOR, &value, NULL), "Charcoal");
    value = parsed(SET_ID_BACKGROUND_COLOR, "#123456");
    CHECK_STR(described(SET_ID_BACKGROUND_COLOR, &value, NULL), "Custom #123456");
    value = parsed(SET_ID_BACKGROUND_IMAGE, "C:\\Pics\\sunset.jpg");
    CHECK_STR(described(SET_ID_BACKGROUND_IMAGE, &value, NULL), "sunset.jpg");
    value = parsed(SET_ID_SLIDESHOW_DIRECTORY, "/home/me/Pictures/");
    CHECK_STR(described(SET_ID_SLIDESHOW_DIRECTORY, &value, NULL), "Pictures");
    memset(&value, 0, sizeof(value));
    CHECK_STR(described(SET_ID_BACKGROUND_IMAGE, &value, NULL), "Choose" ELLIPSIS);
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "30 s");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "120");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "2 min");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "90");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "90 s");
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    CHECK_STR(described(SET_ID_SLIDESHOW_FADE, &value, NULL), "1.5 s");
    value = parsed(SET_ID_TITLE_SIZE, "11%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Small");
    value = parsed(SET_ID_TITLE_SIZE, "14%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Medium");
    value = parsed(SET_ID_TITLE_SIZE, "17%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Large");
    value = parsed(SET_ID_TITLE_SIZE, "12%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "12%");
    value = parsed(SET_ID_TITLE_SIZE, "36");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Fixed 36");
}

// A function to open a model over two menus, with the values a typical config gives
static SettingsState *open_model(void)
{
    static const char *const names[] = { "Main", "Games" };
    SettingsState *state = settings_create(names, 2);
    SettingValue value;
    value = parsed(SET_ID_BACKGROUND_MODE, "Color");
    settings_set_entry(state, SET_ID_BACKGROUND_MODE, -1, &value);
    value = parsed(SET_ID_BACKGROUND_COLOR, "#000000");
    settings_set_entry(state, SET_ID_BACKGROUND_COLOR, -1, &value);
    memset(&value, 0, sizeof(value));
    settings_set_entry(state, SET_ID_BACKGROUND_IMAGE, -1, &value);
    settings_set_entry(state, SET_ID_SLIDESHOW_DIRECTORY, -1, &value);
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    settings_set_entry(state, SET_ID_SLIDESHOW_DURATION, -1, &value);
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    settings_set_entry(state, SET_ID_SLIDESHOW_FADE, -1, &value);
    value = parsed(SET_ID_LAYOUT_ROWS, "1");
    settings_set_entry(state, SET_ID_LAYOUT_ROWS, -1, &value);
    value = parsed(SET_ID_LAYOUT_COLUMNS, "4");
    settings_set_entry(state, SET_ID_LAYOUT_COLUMNS, -1, &value);
    memset(&value, 0, sizeof(value));
    value.inherit = true;
    settings_set_entry(state, SET_ID_LAYOUT_ICON_SIZE, -1, &value);
    value = parsed(SET_ID_TITLE_SIZE, "14%");
    settings_set_entry(state, SET_ID_TITLE_SIZE, -1, &value);
    value = parsed(SET_ID_MENU_ROWS, "3");
    settings_set_entry(state, SET_ID_MENU_ROWS, 1, &value);
    value = parsed(SET_ID_MENU_COLUMNS, "6");
    settings_set_entry(state, SET_ID_MENU_COLUMNS, 1, &value);
    return state;
}

// A function to test the top level, the Menus page, one menu's page and Discard
static void test_top_and_menus(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 5);
    CHECK_STR(rows[0].label, "Background");
    CHECK_STR(rows[0].value, "Colour");
    CHECK_STR(rows[1].label, "Menus");
    CHECK_STR(rows[1].value, "2 menus");
    CHECK_STR(rows[2].label, "Titles");
    CHECK_STR(rows[2].value, "Medium");
    CHECK_INT(rows[3].kind, SETTINGS_ROW_DIVIDER);
    CHECK_STR(rows[4].label, "Discard changes");
    CHECK(!rows[4].enabled);
    CHECK_INT(settings_cursor(state), 0);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_NONE);   // Discard is greyed
    CHECK_INT(settings_cursor(state), 2);
    settings_command(state, SETTINGS_UP);

    // Menus: All menus, a divider, then each menu with its grid as columns x rows
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[0].label, "All menus");
    CHECK_STR(rows[0].value, "4 " TIMES " 1");
    CHECK_STR(rows[2].label, "Main");
    CHECK_STR(rows[2].value, "All menus");
    CHECK_STR(rows[3].label, "Games");
    CHECK_STR(rows[3].value, "6 " TIMES " 3");
    CHECK_INT(settings_preview_menu(state), -1);
    settings_command(state, SETTINGS_DOWN);                  // Over the divider, onto Main
    CHECK_INT(settings_cursor(state), 2);
    CHECK_INT(settings_preview_menu(state), 0);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_preview_menu(state), 1);

    // Games' page: its own rows and columns, and IconSize following All menus
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), 1);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Games");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "3");
    CHECK_STR(rows[1].value, "6");
    CHECK_STR(rows[2].value, "All menus (Fill)");

    // Rows 3 -> 2 -> 1 -> All menus (1)
    SettingsEvent event = settings_command(state, SETTINGS_LEFT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_MENU_ROWS, 1));
    CHECK_INT(event.before.number, 3);
    settings_command(state, SETTINGS_LEFT);
    settings_command(state, SETTINGS_LEFT);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "All menus (1)");
    CHECK(settings_changed(settings_slot(state, SET_ID_MENU_ROWS, 1)));
    CHECK_INT(settings_command(state, SETTINGS_LEFT).kind, SETTINGS_EVENT_NONE);

    // Back at the top, Discard is offered; it puts every value back and greys out again
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK_INT(settings_cursor(state), 1);                    // Where it was
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(rows[4].enabled);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), 4);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_DISCARD);
    CHECK_INT(settings_slot(state, SET_ID_MENU_ROWS, 1)->value.number, 3);
    CHECK(!settings_any_changed(state));
    CHECK_INT(settings_cursor(state), 2);

    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_CLOSE);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_CLOSE_HOME);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_CLOSE);
    settings_free(state);
}

// A function to test the Background page: its rows per mode, and the incomplete-mode rule
static void test_background_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BACKGROUND);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 2);
    CHECK_STR(rows[0].value, "Colour");
    CHECK_STR(rows[1].label, "Colour");
    CHECK_STR(rows[1].value, "Black");

    // Image with none chosen: Back puts the mode back, and says why
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 2);
    CHECK_INT(rows[1].kind, SETTINGS_ROW_BROWSE);
    CHECK_STR(rows[1].value, "Choose" ELLIPSIS);
    SettingsEvent event = settings_command(state, SETTINGS_BACK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_MODE, -1));
    CHECK_INT(event.slot->value.number, 0);
    CHECK(strstr(settings_notice(state), "No image was chosen") != NULL);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK(!settings_any_changed(state));

    // Image chosen in the browser: Back keeps it
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_RIGHT);
    settings_command(state, SETTINGS_DOWN);
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_BROWSE);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1));
    SettingSlot *image = event.slot;
    CHECK_INT(settings_choose(state, image, "/pics/a.png").kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(settings_choose(state, image, "/pics/a.png").kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number, 1);

    // Slideshow: a folder, how long each image shows, and the fade
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[1].label, "Folder");
    CHECK_STR(rows[2].value, "30 s");
    CHECK_STR(rows[3].value, "1.5 s");

    // Transparent: a note in place of rows
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 2);
    CHECK_INT(rows[1].kind, SETTINGS_ROW_NOTE);
    CHECK(rows[1].note != NULL && strstr(rows[1].note, "compositor") != NULL);

    // Closing from the page with Slideshow and no folder puts back the mode the page opened with
    settings_command(state, SETTINGS_LEFT);
    event = settings_command(state, SETTINGS_CLOSE);
    CHECK_INT(event.kind, SETTINGS_EVENT_CLOSE);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_MODE, -1));
    CHECK_INT(event.slot->value.number, 1);
    settings_free(state);
}

// A function to test the page a failed save shows
static void test_save_failed_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_show_save_failed(state, "Couldn't save to /x/config.ini: permission denied");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SAVE_FAILED);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 3);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_NOTE);
    CHECK(strstr(rows[0].note, "permission denied") != NULL);
    CHECK_STR(rows[1].label, "Try again");
    CHECK_STR(rows[2].label, "Leave without saving");
    CHECK_INT(settings_cursor(state), 1);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_RETRY);

    // A retry that fails again stays on the same page, with the new reason
    settings_show_save_failed(state, "Couldn't save to /x/config.ini: the disk is full");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(strstr(rows[0].note, "disk is full") != NULL);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_LEAVE);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    settings_free(state);
}

// A function to test the All menus page, the Titles page, and UP on the first row
static void test_all_menus_and_titles_pages(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    CHECK_INT(settings_command(state, SETTINGS_UP).kind, SETTINGS_EVENT_NONE);   // Nothing above the first row
    CHECK_INT(settings_cursor(state), 0);

    // All menus: [Layout]'s three settings, with no note about following All menus
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    CHECK_INT(settings_cursor(state), 0);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), -1);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "All menus");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 3);
    CHECK(rows[0].slot == settings_slot(state, SET_ID_LAYOUT_ROWS, -1));
    CHECK_STR(rows[0].value, "1");
    CHECK(rows[1].slot == settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1));
    CHECK_STR(rows[1].value, "4");
    CHECK(rows[2].slot == settings_slot(state, SET_ID_LAYOUT_ICON_SIZE, -1));
    CHECK_STR(rows[2].value, "Fill");
    for (int i = 0; i < count; i++)
        CHECK(rows[i].kind != SETTINGS_ROW_NOTE);
    SettingsEvent event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_LAYOUT_ROWS, -1));
    CHECK_INT(event.slot->value.number, 2);

    // Titles: one row, its size
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), 2);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TITLES);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Titles");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 1);
    CHECK_STR(rows[0].label, "Size");
    CHECK_STR(rows[0].value, "Medium");
    event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_TITLE_SIZE, -1));
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "Large");
    CHECK_INT(settings_command(state, SETTINGS_UP).kind, SETTINGS_EVENT_NONE);
    settings_free(state);
}

// A function to test walking every slot by its place: the global settings, then each menu's
static void test_slots_by_place(void)
{
    SettingsState *state = open_model();
    CHECK_INT(settings_slot_count(state), SET_ID_GLOBAL_COUNT + 2 * SET_ID_PER_MENU_COUNT);
    SettingSlot *slot = settings_slot_at(state, 0);
    CHECK(slot != NULL && slot->def->id == SET_ID_BACKGROUND_MODE && slot->menu == -1);
    slot = settings_slot_at(state, SET_ID_GLOBAL_COUNT);
    CHECK(slot != NULL && slot->def->id == SET_ID_MENU_ROWS && slot->menu == 0);
    CHECK(slot == settings_slot(state, SET_ID_MENU_ROWS, 0));
    slot = settings_slot_at(state, settings_slot_count(state) - 1);
    CHECK(slot != NULL && slot->def->id == SET_ID_MENU_ICON_SIZE && slot->menu == 1);
    CHECK(settings_slot_at(state, settings_slot_count(state)) == NULL);
    CHECK(settings_slot_at(state, -1) == NULL);
    CHECK(settings_slot(state, SET_ID_MENU_ROWS, 2) == NULL);
    settings_free(state);
}

// A function to test that one menu is "1 menu"
static void test_one_menu(void)
{
    static const char *const names[] = { "Main" };
    SettingsState *state = settings_create(names, 1);
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[1].value, "1 menu");
    settings_free(state);
}

// A function to test that the Background page opened in an incomplete mode (Mode=Image with no
// Image= line) has nothing to go back to: Back and Close leave the mode, with no event and no notice
static void test_background_entered_incomplete(void)
{
    SettingsState *state = open_model();
    SettingValue image = parsed(SET_ID_BACKGROUND_MODE, "Image");
    settings_set_entry(state, SET_ID_BACKGROUND_MODE, -1, &image);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BACKGROUND);
    SettingsEvent event = settings_command(state, SETTINGS_BACK);
    CHECK_INT(event.kind, SETTINGS_EVENT_MOVED);
    CHECK(event.slot == NULL);
    CHECK_STR(settings_notice(state), "");
    CHECK_INT(settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number, 1);

    settings_command(state, SETTINGS_OK);
    event = settings_command(state, SETTINGS_CLOSE);
    CHECK_INT(event.kind, SETTINGS_EVENT_CLOSE);
    CHECK(event.slot == NULL);
    CHECK_STR(settings_notice(state), "");
    CHECK(!settings_any_changed(state));
    settings_free(state);
}

// A function to test a config with more menus than a page has rows: the Menus page lists what
// fits and says how many more there are in its last row, rather than cutting them off unseen
static void test_more_menus_than_rows(void)
{
    enum { MENUS = 70 };
    static char names[MENUS][16];
    const char *pointers[MENUS];
    for (int i = 0; i < MENUS; i++) {
        snprintf(names[i], sizeof(names[i]), "Menu %d", i + 1);
        pointers[i] = names[i];
    }
    SettingsState *state = settings_create(pointers, MENUS);
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[count - 2].label, "Menu 61");
    CHECK_INT(rows[count - 1].kind, SETTINGS_ROW_NOTE);
    CHECK(rows[count - 1].note != NULL && strstr(rows[count - 1].note, "9 more menus") != NULL);
    for (int i = 0; i < 100; i++)
        settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), count - 2);             // The cursor stops on the last menu listed
    CHECK_INT(settings_preview_menu(state), 60);
    settings_free(state);

    // Exactly as many as fit need no note
    state = settings_create(pointers, SETTINGS_MAX_ROWS - 2);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, SETTINGS_MAX_ROWS);
    CHECK_INT(rows[count - 1].kind, SETTINGS_ROW_LINK);
    CHECK_STR(rows[count - 1].label, "Menu 62");
    settings_free(state);
}

// A function to test the new types' round trips: what the parser reads is written back unchanged
static void test_new_round_trips(void)
{
    static const struct { SettingId id; const char *text; } cases[] = {
        { SET_ID_WRAP_ENTRIES, "true" },
        { SET_ID_VSYNC, "false" },
        { SET_ID_FPS_LIMIT, "10" },                  // The documented minimum, which the old parser refused
        { SET_ID_FPS_LIMIT, "144" },
        { SET_ID_APPLICATION_TIMEOUT, "15" },
        { SET_ID_ON_LAUNCH, "Quit" },
        { SET_ID_STARTUP_CMD, ":submenu Games" },
        { SET_ID_QUIT_CMD, "\"C:\\Program Files\\Kodi\\kodi.exe\" --standalone" },
        { SET_ID_DEFAULT_MENU, "Main" },
        { SET_ID_CHROMA_KEY_COLOR, "#010101" },
        { SET_ID_OVERLAY_OPACITY, "50%" },
        { SET_ID_OVERLAY_OPACITY, "12.5%" },
        { SET_ID_OVERLAY_OPACITY, "33.33%" },
        { SET_ID_ICON_SPACING, "5%" },
        { SET_ID_ICON_SPACING, "40" },               // px, as the old parser read it
        { SET_ID_ICON_SPACING, "2000000000" },       // f15-limits: huge, and still read
        { SET_ID_VCENTER, "50%" },
        { SET_ID_TITLE_FONT, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" },
        { SET_ID_TITLE_FONT_FACE, "2" },
        { SET_ID_TITLE_OVERSIZE, "Shrink" },
        { SET_ID_TITLE_OVERSIZE, "None" },
        { SET_ID_TITLE_PADDING, "8%" },
        { SET_ID_TITLE_PADDING, "20" },
        { SET_ID_HIGHLIGHT_OUTLINE_SIZE, "3" },
        { SET_ID_HIGHLIGHT_CORNER_RADIUS, "25" },
        { SET_ID_HIGHLIGHT_HPADDING, "300" },        // Past the last step; the clamp is Effective's
        { SET_ID_CLOCK_FONT_SIZE, "50" },
        { SET_ID_CLOCK_MARGIN, "5%" },
        { SET_ID_CLOCK_TIME_FORMAT, "12hr" },
        { SET_ID_CLOCK_DATE_FORMAT, "Little" },
        { SET_ID_SCREENSAVER_IDLE_TIME, "300" },
        { SET_ID_SCREENSAVER_INTENSITY, "70%" },
        { SET_ID_GAMEPAD_DEVICE, "-1" },
        { SET_ID_GAMEPAD_DEVICE, "2" },
        { SET_ID_GAMEPAD_MAPPINGS, "/home/me/gamecontrollerdb.txt" }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        SettingValue value = parsed(cases[i].id, cases[i].text);
        CHECK_STR(formatted(cases[i].id, &value), cases[i].text);
    }

    // Spellings the parser reads, and how they are written back
    SettingValue value = parsed(SET_ID_WRAP_ENTRIES, "True");
    CHECK_INT(value.number, 1);
    CHECK_STR(formatted(SET_ID_WRAP_ENTRIES, &value), "true");
    value = parsed(SET_ID_TITLE_OVERSIZE, "Truncated");                     // The old parser's only spelling
    CHECK_STR(formatted(SET_ID_TITLE_OVERSIZE, &value), "Truncate");
    value = parsed(SET_ID_OVERLAY_OPACITY, "12.50%");
    CHECK_INT(value.number, 1250);
    CHECK(value.percent);
    CHECK_STR(formatted(SET_ID_OVERLAY_OPACITY, &value), "12.5%");
    value = parsed(SET_ID_TITLE_FONT, "\"C:\\Fonts\\My Font.ttf\"");         // Quotes dropped, as clean_path does
    CHECK_STR(value.text, "C:\\Fonts\\My Font.ttf");
    value = parsed(SET_ID_APPLICATION_TIMEOUT, "15s");                     // atoi, as before
    CHECK_INT(value.number, 15);

    // An absent FPSLimit, FontFace or command writes nothing: the key is removed
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    CHECK_STR(formatted(SET_ID_FPS_LIMIT, &inherit), "");
    CHECK_STR(formatted(SET_ID_TITLE_FONT_FACE, &inherit), "");
    CHECK_STR(formatted(SET_ID_STARTUP_CMD, &inherit), "");
}

// A function to test what the new types refuse, including the spec's two parser bugs
static void test_new_rejects(void)
{
    SettingValue value;
    CHECK(!setting_parse(setting_def(SET_ID_FPS_LIMIT), "9", &value));
    CHECK(!setting_parse(setting_def(SET_ID_CLOCK_FONT_SIZE), "-5", &value));   // Wrapped to 4294967291 before
    CHECK(!setting_parse(setting_def(SET_ID_CLOCK_FONT_SIZE), "0", &value));
    CHECK(!setting_parse(setting_def(SET_ID_WRAP_ENTRIES), "yes", &value));
    CHECK(!setting_parse(setting_def(SET_ID_WRAP_ENTRIES), "TRUE", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ON_LAUNCH), "blank", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "101%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "abc%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "50", &value));   // No px for an opacity
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "5.555%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "5.%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "40px", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "-4", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "2147483648", &value));  // Past an int
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_PADDING), "8.5%", &value));  // Padding is whole
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_PADDING), "51%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_HIGHLIGHT_CORNER_RADIUS), "101", &value));
    CHECK(!setting_parse(setting_def(SET_ID_HIGHLIGHT_OUTLINE_SIZE), "-1", &value));
    CHECK(!setting_parse(setting_def(SET_ID_APPLICATION_TIMEOUT), "2", &value));
    CHECK(!setting_parse(setting_def(SET_ID_APPLICATION_TIMEOUT), "31", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SCREENSAVER_IDLE_TIME), "901", &value));
    CHECK(!setting_parse(setting_def(SET_ID_GAMEPAD_DEVICE), "-2", &value));
    CHECK(!setting_parse(setting_def(SET_ID_GAMEPAD_DEVICE), "16", &value));
    CHECK(!setting_parse(setting_def(SET_ID_STARTUP_CMD), "", &value));
    CHECK(!setting_parse(setting_def(SET_ID_DEFAULT_MENU), "", &value));
}

// A function to test the new types' steps: limits, the Off step, and a file's value in its place
static void test_new_steps(void)
{
    SettingValue off = parsed(SET_ID_WRAP_ENTRIES, "false");
    CHECK_INT(stepped(SET_ID_WRAP_ENTRIES, off, &off, 1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_WRAP_ENTRIES, off, &off, 1, 5).number, 1);

    // FPS limit: Off, 30, 60, 75, 120, 144, 165, 240; a file's 10 sits between Off and 30
    SettingValue ten = parsed(SET_ID_FPS_LIMIT, "10");
    CHECK(stepped(SET_ID_FPS_LIMIT, ten, &ten, -1, 1).inherit);
    CHECK_INT(stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 1).number, 30);
    CHECK_INT(stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 20).number, 240);
    SettingValue back = stepped(SET_ID_FPS_LIMIT, stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 3), &ten, -1, 3);
    CHECK_INT(back.number, 10);

    // Opacity: 0-100% in fives; a file's 12.5% sits between 10% and 15%
    SettingValue odd = parsed(SET_ID_OVERLAY_OPACITY, "12.5%");
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, -1, 1).number, 1000);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, 1, 1).number, 1500);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, 1, 30).number, 10000);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, -1, 30).number, 0);

    // Icon spacing: a file's px value comes before every percentage
    SettingValue px = parsed(SET_ID_ICON_SPACING, "40");
    SettingValue first_pct = stepped(SET_ID_ICON_SPACING, px, &px, 1, 1);
    CHECK(first_pct.percent);
    CHECK_INT(first_pct.number, 0);
    CHECK(!stepped(SET_ID_ICON_SPACING, first_pct, &px, -1, 1).percent);
    CHECK_INT(stepped(SET_ID_ICON_SPACING, px, &px, 1, 20).number, 1000);   // 10% at most

    // Vertical centre: 25-75% in fives
    SettingValue centre = parsed(SET_ID_VCENTER, "50%");
    CHECK_INT(stepped(SET_ID_VCENTER, centre, &centre, 1, 20).number, 7500);
    CHECK_INT(stepped(SET_ID_VCENTER, centre, &centre, -1, 20).number, 2500);

    // Padding: 0-20% in twos; a file's px sits first
    SettingValue padding = parsed(SET_ID_TITLE_PADDING, "8%");
    CHECK_INT(stepped(SET_ID_TITLE_PADDING, padding, &padding, 1, 1).number, 1000);
    CHECK_INT(stepped(SET_ID_TITLE_PADDING, padding, &padding, 1, 20).number, 2000);

    // A padding past the last step (300 px) stays reachable at the end
    SettingValue wide = parsed(SET_ID_HIGHLIGHT_HPADDING, "300");
    CHECK_INT(stepped(SET_ID_HIGHLIGHT_HPADDING, wide, &wide, -1, 1).number, 100);
    CHECK_INT(stepped(SET_ID_HIGHLIGHT_HPADDING, wide, &wide, 1, 1).number, 300);

    // Choices step through their own names only: Too long is Truncate or Shrink, and a file's None stays
    SettingValue none = parsed(SET_ID_TITLE_OVERSIZE, "None");
    CHECK_INT(stepped(SET_ID_TITLE_OVERSIZE, none, &none, -1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_TITLE_OVERSIZE, none, &none, -1, 5).number, 0);

    // The spec's lists: app timeout, idle time, dim level, clock size
    SettingValue timeout = parsed(SET_ID_APPLICATION_TIMEOUT, "3");
    CHECK_INT(stepped(SET_ID_APPLICATION_TIMEOUT, timeout, &timeout, 1, 4).number, 20);
    SettingValue idle = parsed(SET_ID_SCREENSAVER_IDLE_TIME, "3");
    CHECK_INT(stepped(SET_ID_SCREENSAVER_IDLE_TIME, idle, &idle, 1, 5).number, 60);
    CHECK_INT(stepped(SET_ID_SCREENSAVER_IDLE_TIME, idle, &idle, 1, 20).number, 900);
    SettingValue dim = parsed(SET_ID_SCREENSAVER_INTENSITY, "70%");
    CHECK_INT(stepped(SET_ID_SCREENSAVER_INTENSITY, dim, &dim, -1, 20).number, 1000);
    SettingValue size = parsed(SET_ID_CLOCK_FONT_SIZE, "50");
    CHECK_INT(stepped(SET_ID_CLOCK_FONT_SIZE, size, &size, 1, 50).number, 120);
    CHECK_INT(stepped(SET_ID_CLOCK_FONT_SIZE, size, &size, -1, 50).number, 20);

    // Paths, fonts, commands, the default menu and the device are chosen elsewhere, never stepped here
    SettingValue cmd = parsed(SET_ID_STARTUP_CMD, ":quit");
    SettingValue same_cmd = stepped(SET_ID_STARTUP_CMD, cmd, &cmd, 1, 1);   // A local: MSVC's C4223 refuses .text on a returned struct
    CHECK_STR(same_cmd.text, ":quit");
    SettingValue device = parsed(SET_ID_GAMEPAD_DEVICE, "1");
    CHECK_INT(stepped(SET_ID_GAMEPAD_DEVICE, device, &device, 1, 1).number, 1);
}

// A function to test how the new types read on screen
static void test_new_descriptions(void)
{
    SettingValue value = parsed(SET_ID_WRAP_ENTRIES, "true");
    CHECK_STR(described(SET_ID_WRAP_ENTRIES, &value, NULL), "On");
    value = parsed(SET_ID_FPS_LIMIT, "60");
    CHECK_STR(described(SET_ID_FPS_LIMIT, &value, NULL), "60 fps");
    value.inherit = true;
    CHECK_STR(described(SET_ID_FPS_LIMIT, &value, NULL), "Off");
    value = parsed(SET_ID_OVERLAY_OPACITY, "12.5%");
    CHECK_STR(described(SET_ID_OVERLAY_OPACITY, &value, NULL), "12.5%");
    value = parsed(SET_ID_ICON_SPACING, "40");
    CHECK_STR(described(SET_ID_ICON_SPACING, &value, NULL), "40 px");
    value = parsed(SET_ID_HIGHLIGHT_HPADDING, "30");
    CHECK_STR(described(SET_ID_HIGHLIGHT_HPADDING, &value, NULL), "30 px");
    value = parsed(SET_ID_HIGHLIGHT_CORNER_RADIUS, "25");
    CHECK_STR(described(SET_ID_HIGHLIGHT_CORNER_RADIUS, &value, NULL), "25");
    value = parsed(SET_ID_APPLICATION_TIMEOUT, "15");
    CHECK_STR(described(SET_ID_APPLICATION_TIMEOUT, &value, NULL), "15 s");
    value = parsed(SET_ID_SCREENSAVER_IDLE_TIME, "300");
    CHECK_STR(described(SET_ID_SCREENSAVER_IDLE_TIME, &value, NULL), "5 min");
    value = parsed(SET_ID_ON_LAUNCH, "Blank");
    CHECK_STR(described(SET_ID_ON_LAUNCH, &value, NULL), "Blank screen");
    value = parsed(SET_ID_CLOCK_TIME_FORMAT, "12hr");
    CHECK_STR(described(SET_ID_CLOCK_TIME_FORMAT, &value, NULL), "2:05 PM");
    value = parsed(SET_ID_CLOCK_DATE_FORMAT, "Big");
    CHECK_STR(described(SET_ID_CLOCK_DATE_FORMAT, &value, NULL), "Sep 28");
    value = parsed(SET_ID_TITLE_FONT, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    CHECK_STR(described(SET_ID_TITLE_FONT, &value, NULL), "DejaVuSans.ttf");
    value = parsed(SET_ID_STARTUP_CMD, ":submenu Games");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "Open submenu: Games");
    value = parsed(SET_ID_STARTUP_CMD, ":quit");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "Quit StreamFlex");
    value = parsed(SET_ID_STARTUP_CMD, "kodi --standalone");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "kodi --standalone");
    value.inherit = true;
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "None");
    value = parsed(SET_ID_GAMEPAD_DEVICE, "-1");
    CHECK_STR(described(SET_ID_GAMEPAD_DEVICE, &value, NULL), "Any");
    value = parsed(SET_ID_GAMEPAD_DEVICE, "1");
    CHECK_STR(described(SET_ID_GAMEPAD_DEVICE, &value, NULL), "Pad 1");

    char label[64];
    setting_command_label(":select", label, sizeof(label));
    CHECK_STR(label, "OK");
    setting_command_label(":exit", label, sizeof(label));
    CHECK_STR(label, "Close the app on show");
}

// A function to test finding a setting by section and key, as the parser does
static void test_find(void)
{
    CHECK(setting_find("Titles", "Color") == setting_def(SET_ID_TITLE_COLOR));
    CHECK(setting_find("Background", "Color") == setting_def(SET_ID_BACKGROUND_COLOR));
    CHECK(setting_find("Highlight", "Enabled") == setting_def(SET_ID_HIGHLIGHT_ENABLED));
    CHECK(setting_find("Scroll Indicators", "Enabled") == setting_def(SET_ID_SCROLL_ENABLED));
    CHECK(setting_find("Layout", "MaxButtons") == setting_def(SET_ID_LAYOUT_COLUMNS));   // Its alias
    CHECK(setting_find("Titles", "FontFace") == setting_def(SET_ID_TITLE_FONT_FACE));
    CHECK(setting_find("Clock", "FontFace") == setting_def(SET_ID_CLOCK_FONT_FACE));
    CHECK(setting_find("General", "Nope") == NULL);
    CHECK(setting_find("Main", "Rows") == NULL);                   // Per-menu keys belong to menu sections
    CHECK(setting_find("Hotkeys", "Hotkey1") == NULL);

    // Every global setting is found by its own section and key, and has a label and a section
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        const SettingDef *def = setting_def((SettingId) id);
        CHECK_INT((int) def->id, id);
        CHECK(def->section != NULL && def->label != NULL && def->key != NULL);
        CHECK(setting_find(def->section, def->key) == def);
    }
}

int main(void)
{
    test_round_trips();
    test_rejects();
    test_steps();
    test_descriptions();
    test_top_and_menus();
    test_background_page();
    test_save_failed_page();
    test_all_menus_and_titles_pages();
    test_slots_by_place();
    test_one_menu();
    test_background_entered_incomplete();
    test_more_menus_than_rows();
    test_new_round_trips();
    test_new_rejects();
    test_new_steps();
    test_new_descriptions();
    test_find();
    return check_report();
}

