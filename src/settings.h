// The settings screen's model: the table of settings it can change, how each one's value is read
// from and written to config.ini, stepped with Left and Right and described on screen, and the
// pages the remote moves through. Pure: no SDL, no globals, so tests/test_settings.c builds it on
// its own; memory comes from alloc.h. settings_screen.c draws it and applies what it changes.
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#define SETTING_TEXT_MAX 1024  // Longest path a setting holds
#define SETTINGS_MAX_ROWS 64   // Most rows one page shows; Menus counts the rest in a note
#define SETTINGS_MAX_DEPTH 8   // Deepest the pages go

typedef enum {
    SET_TYPE_COUNT,       // A whole number: Rows, Columns
    SET_TYPE_CHOICE,      // One of the background modes
    SET_TYPE_ICON_SIZE,   // IconSize: Fill (no key), or px
    SET_TYPE_COLOR,       // #RRGGBB, stepped through the presets
    SET_TYPE_PATH,        // A file or folder, chosen in the browser, never stepped
    SET_TYPE_SECONDS,     // Whole seconds: SlideshowImageDuration
    SET_TYPE_MILLIS,      // Seconds with decimals in the file, ms here: SlideshowTransitionTime
    SET_TYPE_TITLE_SIZE   // FontSize: a percentage of the button, or a fixed size
} SettingType;

typedef enum {
    SET_REFRESH_NONE,        // The launcher reads the value when it next needs it
    SET_REFRESH_LAYOUT,      // Lay the menu on show out again
    SET_REFRESH_TITLES,      // Render every menu's titles again
    SET_REFRESH_BACKGROUND   // Set the background up again
} SettingRefresh;

typedef enum {
    SET_ID_BACKGROUND_MODE,
    SET_ID_BACKGROUND_COLOR,
    SET_ID_BACKGROUND_IMAGE,
    SET_ID_SLIDESHOW_DIRECTORY,
    SET_ID_SLIDESHOW_DURATION,
    SET_ID_SLIDESHOW_FADE,
    SET_ID_LAYOUT_ROWS,
    SET_ID_LAYOUT_COLUMNS,
    SET_ID_LAYOUT_ICON_SIZE,
    SET_ID_TITLE_SIZE,
    SET_ID_MENU_ROWS,        // The per-menu settings come last: one of each for every menu
    SET_ID_MENU_COLUMNS,
    SET_ID_MENU_ICON_SIZE,
    SET_ID_COUNT
} SettingId;

#define SET_ID_GLOBAL_COUNT SET_ID_MENU_ROWS
#define SET_ID_PER_MENU_COUNT (SET_ID_COUNT - SET_ID_MENU_ROWS)

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} SettingColor;

typedef struct {
    bool inherit;                // The key is absent: a menu follows [Layout], or IconSize is Fill
    int number;                  // COUNT, CHOICE (index), ICON_SIZE (px), SECONDS (s), MILLIS (ms), TITLE_SIZE
    bool percent;                // TITLE_SIZE: `number` is a percentage of the button
    SettingColor color;          // COLOR
    char text[SETTING_TEXT_MAX]; // PATH; "" when none is chosen
} SettingValue;

typedef struct {
    SettingId id;
    const char *label;           // The row's label on screen
    const char *section;         // NULL: the section of the menu being edited
    const char *key;
    const char *alias;           // An older key the parser reads as this one (MaxButtons); NULL if none
    SettingType type;
    int min;                     // COUNT, SECONDS, MILLIS: the limits; CHOICE: the first and last index
    int max;
    bool can_inherit;            // The lowest step removes the key
    SettingRefresh refresh;
} SettingDef;

typedef struct {
    const SettingDef *def;
    int menu;                    // The menu's index for a per-menu setting; -1 otherwise
    SettingValue value;          // What the launcher shows now
    SettingValue entry;          // What it was when settings opened: Discard's target, and the changed test
} SettingSlot;

typedef enum {
    SETTINGS_PAGE_TOP,
    SETTINGS_PAGE_BACKGROUND,
    SETTINGS_PAGE_MENUS,
    SETTINGS_PAGE_MENU,          // One menu's grid, or All menus ([Layout])
    SETTINGS_PAGE_TITLES,
    SETTINGS_PAGE_SAVE_FAILED
} SettingsPage;

typedef enum {
    SETTINGS_ROW_SETTING,        // Left and Right step its value
    SETTINGS_ROW_LINK,           // OK opens another page
    SETTINGS_ROW_BROWSE,         // OK opens the folder browser for its path
    SETTINGS_ROW_ACTION,         // OK does something
    SETTINGS_ROW_DIVIDER,
    SETTINGS_ROW_NOTE            // Text only
} SettingsRowKind;

typedef enum {
    SETTINGS_ACTION_NONE,
    SETTINGS_ACTION_DISCARD,
    SETTINGS_ACTION_RETRY,
    SETTINGS_ACTION_LEAVE
} SettingsAction;

typedef struct {
    SettingsRowKind kind;
    char label[128];
    char value[256];             // What the row shows on the right
    const char *note;            // NOTE rows: the text, owned by the model
    SettingSlot *slot;           // SETTING and BROWSE rows
    SettingsPage target;         // LINK rows
    int menu;                    // LINK rows to a menu's page: its index; -1 for All menus
    SettingsAction action;       // ACTION rows
    bool enabled;                // False: shown greyed, and the cursor skips it
} SettingsRow;

typedef enum {
    SETTINGS_UP,
    SETTINGS_DOWN,
    SETTINGS_LEFT,
    SETTINGS_RIGHT,
    SETTINGS_OK,
    SETTINGS_BACK,
    SETTINGS_HOME,               // Save and close, then go to the default menu
    SETTINGS_CLOSE               // Save and close (the key that opened settings)
} SettingsCommand;

typedef enum {
    SETTINGS_EVENT_NONE,
    SETTINGS_EVENT_MOVED,        // The cursor or the page changed: redraw
    SETTINGS_EVENT_CHANGED,      // `slot`'s value changed from `before`: apply it
    SETTINGS_EVENT_BROWSE,       // Open the folder browser for `slot`
    SETTINGS_EVENT_DISCARD,      // Every value went back to its entry: apply them all
    SETTINGS_EVENT_CLOSE,        // Save and close; apply `slot` first when it is set
    SETTINGS_EVENT_CLOSE_HOME,   // The same, then go to the default menu
    SETTINGS_EVENT_RETRY,        // Try the failed save again
    SETTINGS_EVENT_LEAVE         // Close without saving
} SettingsEventKind;

typedef struct {
    SettingsEventKind kind;
    SettingSlot *slot;
    SettingValue before;
} SettingsEvent;

typedef struct SettingsState SettingsState;

const SettingDef *setting_def(SettingId id);
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value);
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size);
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b);
SettingValue setting_step(const SettingDef *def, const SettingValue *current, const SettingValue *entry, int direction);
void setting_describe(const SettingDef *def, const SettingValue *value, const SettingValue *inherited, char *out, size_t size);

SettingsState *settings_create(const char *const *menu_names, int menu_count);
void settings_free(SettingsState *state);
SettingSlot *settings_slot(SettingsState *state, SettingId id, int menu);
int settings_slot_count(const SettingsState *state);
SettingSlot *settings_slot_at(SettingsState *state, int index);
void settings_set_entry(SettingsState *state, SettingId id, int menu, const SettingValue *value);
bool settings_changed(const SettingSlot *slot);
bool settings_any_changed(const SettingsState *state);
int settings_rows(SettingsState *state, SettingsRow *rows, int max);
int settings_cursor(const SettingsState *state);
SettingsPage settings_page(const SettingsState *state);
void settings_path(const SettingsState *state, char *out, size_t size);
int settings_preview_menu(SettingsState *state);
const char *settings_notice(const SettingsState *state);
SettingsEvent settings_command(SettingsState *state, SettingsCommand command);
SettingsEvent settings_choose(SettingsState *state, SettingSlot *slot, const char *path);
void settings_show_save_failed(SettingsState *state, const char *message);

#endif
