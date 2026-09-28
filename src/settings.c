#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "settings.h"
#include "layout.h"
#include <launcher_config.h>

#define ARROW " \xE2\x80\xBA "   // U+203A with a space either side, between the pages in the page path
#define TIMES "\xC3\x97"         // U+00D7, the multiplication sign
#define ELLIPSIS "\xE2\x80\xA6"  // U+2026, the ellipsis
#define LENGTH(array) ((int) (sizeof(array) / sizeof((array)[0])))

// The background modes, in ModeBackground's order (launcher.h): the file's names and the screen's
static const char *const MODE_NAMES[] = { "Color", "Image", "Slideshow", "Transparent" };
static const char *const MODE_LABELS[] = { "Colour", "Image", "Slideshow", "Transparent" };
#define MODE_IMAGE 1
#define MODE_SLIDESHOW 2

static const struct {
    const char *name;
    SettingColor color;
} PRESETS[] = {
    { "Black",    { 0x00, 0x00, 0x00 } },
    { "Charcoal", { 0x1E, 0x1E, 0x1E } },
    { "Graphite", { 0x33, 0x38, 0x3D } },
    { "Slate",    { 0x2E, 0x34, 0x40 } },
    { "Midnight", { 0x12, 0x1A, 0x2E } },
    { "Navy",     { 0x0B, 0x1F, 0x3A } },
    { "Teal",     { 0x07, 0x60, 0x6C } },
    { "Forest",   { 0x1E, 0x3B, 0x2F } },
    { "Plum",     { 0x3B, 0x1F, 0x3A } },
    { "Burgundy", { 0x4A, 0x15, 0x20 } }
};

static const int ICON_STEPS[] = { 64, 96, 128, 160, 192, 256, 320, 384, 512, 768, 1024 };
static const int SECOND_STEPS[] = { 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600 };
static const int MILLI_STEPS[] = { 0, 500, 1000, 1500, 2000, 2500, 3000 };
static const int TITLE_STEPS[] = { 11, 14, 17 };  // Small, Medium, Large

static const char *const TRANSPARENT_NOTE =
    "The desktop shows through. On Linux this needs a compositor: see Transparent Backgrounds in the configuration docs.";
static const char *const MENU_NOTE = "The lowest step, All menus, follows the shared grid.";

static const SettingDef DEFS[SET_ID_COUNT] = {
    { SET_ID_BACKGROUND_MODE, "Mode", "Background", SETTING_BACKGROUND_MODE, NULL, SET_TYPE_CHOICE, 0, 3, false, SET_REFRESH_BACKGROUND },
    { SET_ID_BACKGROUND_COLOR, "Colour", "Background", SETTING_BACKGROUND_COLOR, NULL, SET_TYPE_COLOR, 0, 0, false, SET_REFRESH_BACKGROUND },
    { SET_ID_BACKGROUND_IMAGE, "Image", "Background", SETTING_BACKGROUND_IMAGE, NULL, SET_TYPE_PATH, 0, 0, false, SET_REFRESH_BACKGROUND },
    { SET_ID_SLIDESHOW_DIRECTORY, "Folder", "Background", SETTING_SLIDESHOW_DIRECTORY, NULL, SET_TYPE_PATH, 0, 0, false, SET_REFRESH_BACKGROUND },
    { SET_ID_SLIDESHOW_DURATION, "Change every", "Background", SETTING_SLIDESHOW_IMAGE_DURATION, NULL, SET_TYPE_SECONDS, 5, 3600, false, SET_REFRESH_NONE },
    { SET_ID_SLIDESHOW_FADE, "Fade", "Background", SETTING_SLIDESHOW_TRANSITION_TIME, NULL, SET_TYPE_MILLIS, 0, 3000, false, SET_REFRESH_NONE },
    { SET_ID_LAYOUT_ROWS, "Rows", "Layout", SETTING_ROWS, NULL, SET_TYPE_COUNT, 1, 10, false, SET_REFRESH_LAYOUT },
    { SET_ID_LAYOUT_COLUMNS, "Columns", "Layout", SETTING_COLUMNS, SETTING_MAX_BUTTONS, SET_TYPE_COUNT, 1, 12, false, SET_REFRESH_LAYOUT },
    { SET_ID_LAYOUT_ICON_SIZE, "Largest button", "Layout", SETTING_ICON_SIZE, NULL, SET_TYPE_ICON_SIZE, 0, 0, true, SET_REFRESH_LAYOUT },
    { SET_ID_TITLE_SIZE, "Size", "Titles", SETTING_TITLE_FONT_SIZE, NULL, SET_TYPE_TITLE_SIZE, 0, 0, false, SET_REFRESH_TITLES },
    { SET_ID_MENU_ROWS, "Rows", NULL, SETTING_ROWS, NULL, SET_TYPE_COUNT, 1, 10, true, SET_REFRESH_LAYOUT },
    { SET_ID_MENU_COLUMNS, "Columns", NULL, SETTING_COLUMNS, NULL, SET_TYPE_COUNT, 1, 12, true, SET_REFRESH_LAYOUT },
    { SET_ID_MENU_ICON_SIZE, "Largest button", NULL, SETTING_ICON_SIZE, NULL, SET_TYPE_ICON_SIZE, 0, 0, true, SET_REFRESH_LAYOUT }
};

// A function to get a setting's row in the table
const SettingDef *setting_def(SettingId id)
{
    return &DEFS[id];
}

// A function to read "#RRGGBB", strictly: six hex digits
static bool parse_hex_color(const char *text, SettingColor *color)
{
    if (text[0] != '#' || strlen(text) != 7)
        return false;
    unsigned int rgb = 0;
    for (int i = 1; i < 7; i++) {
        char c = text[i];
        unsigned int digit;
        if (c >= '0' && c <= '9')
            digit = (unsigned int) (c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int) (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = (unsigned int) (c - 'A' + 10);
        else
            return false;
        rgb = rgb * 16 + digit;
    }
    color->r = (unsigned char) (rgb >> 16);
    color->g = (unsigned char) ((rgb >> 8) & 0xFF);
    color->b = (unsigned char) (rgb & 0xFF);
    return true;
}

// A function to read a setting's value from config.ini's text, as the launcher has always read it
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value)
{
    SettingValue v;
    memset(&v, 0, sizeof(v));
    switch (def->type) {
        case SET_TYPE_COUNT:
            if (!layout_parse_count(text, &v.number))
                return false;
            break;
        case SET_TYPE_ICON_SIZE:
            if (!layout_parse_icon_size(text, &v.number))
                return false;
            break;
        case SET_TYPE_CHOICE:
            for (v.number = 0; v.number < LENGTH(MODE_NAMES) && strcmp(MODE_NAMES[v.number], text) != 0; v.number++);
            if (v.number == LENGTH(MODE_NAMES))
                return false;
            break;
        case SET_TYPE_COLOR:
            if (!parse_hex_color(text, &v.color))
                return false;
            break;
        case SET_TYPE_PATH: {
            // Quotes round a path are dropped, as clean_path() does
            size_t length = strlen(text);
            if (length == 0 || length >= SETTING_TEXT_MAX)
                return false;
            if (length >= 3 && text[0] == '"' && text[length - 1] == '"') {
                memcpy(v.text, text + 1, length - 2);
                v.text[length - 2] = '\0';
            }
            else
                memcpy(v.text, text, length + 1);
            break;
        }
        case SET_TYPE_SECONDS:
            v.number = atoi(text);
            if (v.number < def->min || v.number > def->max)
                return false;
            break;
        case SET_TYPE_MILLIS: {
            double seconds = atof(text);
            if (!(seconds >= 0.0) || seconds * 1000.0 >= (double) def->max + 0.5)
                return false;
            v.number = (int) (seconds * 1000.0 + 0.5);
            break;
        }
        case SET_TYPE_TITLE_SIZE:
            if (!layout_parse_title_size(text, &v.number, &v.percent))
                return false;
            break;
    }
    *value = v;
    return true;
}

// A function to write ms as seconds with only the decimals it needs: 1500 is "1.5", 3000 is "3"
static void format_millis(int ms, char *out, size_t size)
{
    int whole = ms / 1000;
    int part = ms % 1000;
    if (part == 0)
        snprintf(out, size, "%d", whole);
    else if (part % 100 == 0)
        snprintf(out, size, "%d.%d", whole, part / 100);
    else if (part % 10 == 0)
        snprintf(out, size, "%d.%02d", whole, part / 10);
    else
        snprintf(out, size, "%d.%03d", whole, part);
}

// A function to write a value as config.ini holds it; "" for a value that removes the key
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size)
{
    if (size == 0)
        return;
    out[0] = '\0';
    if (value->inherit)
        return;
    switch (def->type) {
        case SET_TYPE_COUNT:
        case SET_TYPE_ICON_SIZE:
        case SET_TYPE_SECONDS:
            snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_CHOICE:
            if (value->number >= 0 && value->number < LENGTH(MODE_NAMES))
                snprintf(out, size, "%s", MODE_NAMES[value->number]);
            break;
        case SET_TYPE_COLOR:
            snprintf(out, size, "#%02X%02X%02X", value->color.r, value->color.g, value->color.b);
            break;
        case SET_TYPE_PATH:
            snprintf(out, size, "%s", value->text);
            break;
        case SET_TYPE_MILLIS:
            format_millis(value->number, out, size);
            break;
        case SET_TYPE_TITLE_SIZE:
            if (value->percent)
                snprintf(out, size, "%d%%", value->number);
            else
                snprintf(out, size, "%d", value->number);
            break;
    }
}

// A function to compare two values of a setting
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    switch (def->type) {
        case SET_TYPE_COLOR:
            return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
        case SET_TYPE_PATH:
            return strcmp(a->text, b->text) == 0;
        case SET_TYPE_TITLE_SIZE:
            return a->percent == b->percent && a->number == b->number;
        default:
            return a->number == b->number;
    }
}

// One step a row can take; a SettingValue without its path buffer
typedef struct {
    bool inherit;
    int number;
    bool percent;
    SettingColor color;
} Candidate;

#define MAX_CANDIDATES 48

// A function to find a colour among the presets; -1 when it is not one
static int preset_index(SettingColor color)
{
    for (int i = 0; i < LENGTH(PRESETS); i++) {
        if (PRESETS[i].color.r == color.r && PRESETS[i].color.g == color.g && PRESETS[i].color.b == color.b)
            return i;
    }
    return -1;
}

// A function to give a step its place: following the default first, then a custom colour or a
// fixed title size, then the rest in order
static long sort_key(const SettingDef *def, const Candidate *c)
{
    if (c->inherit)
        return LONG_MIN;
    if (def->type == SET_TYPE_COLOR)
        return preset_index(c->color);
    if (def->type == SET_TYPE_TITLE_SIZE)
        return c->percent ? c->number : -1;
    return c->number;
}

// A function to compare two steps
static bool same_candidate(const SettingDef *def, const Candidate *a, const Candidate *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    if (def->type == SET_TYPE_COLOR)
        return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
    if (def->type == SET_TYPE_TITLE_SIZE)
        return a->percent == b->percent && a->number == b->number;
    return a->number == b->number;
}

// A function to add a step to a list once
static void add_candidate(const SettingDef *def, Candidate *list, int *count, Candidate c)
{
    for (int i = 0; i < *count; i++) {
        if (same_candidate(def, &list[i], &c))
            return;
    }
    if (*count < MAX_CANDIDATES)
        list[(*count)++] = c;
}

// A function to make a step from a number
static Candidate number_candidate(int number, bool percent)
{
    Candidate c;
    memset(&c, 0, sizeof(c));
    c.number = number;
    c.percent = percent;
    return c;
}

// A function to make a step from a value
static Candidate value_candidate(const SettingValue *value)
{
    Candidate c;
    memset(&c, 0, sizeof(c));
    c.inherit = value->inherit;
    c.number = value->number;
    c.percent = value->percent;
    c.color = value->color;
    return c;
}

// A function to list a row's steps in order. The value on entry and the current value are always
// among them, in their sorted place, so the user can step back to what the file said.
static int build_candidates(const SettingDef *def, const SettingValue *current, const SettingValue *entry, Candidate *list)
{
    int count = 0;
    if (def->can_inherit) {
        Candidate inherit = number_candidate(0, false);
        inherit.inherit = true;
        add_candidate(def, list, &count, inherit);
    }
    switch (def->type) {
        case SET_TYPE_COUNT:
        case SET_TYPE_CHOICE:
            for (int n = def->min; n <= def->max; n++)
                add_candidate(def, list, &count, number_candidate(n, false));
            break;
        case SET_TYPE_ICON_SIZE:
            for (int i = 0; i < LENGTH(ICON_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(ICON_STEPS[i], false));
            break;
        case SET_TYPE_SECONDS:
            for (int i = 0; i < LENGTH(SECOND_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(SECOND_STEPS[i], false));
            break;
        case SET_TYPE_MILLIS:
            for (int i = 0; i < LENGTH(MILLI_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(MILLI_STEPS[i], false));
            break;
        case SET_TYPE_TITLE_SIZE:
            for (int i = 0; i < LENGTH(TITLE_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(TITLE_STEPS[i], true));
            break;
        case SET_TYPE_COLOR:
            for (int i = 0; i < LENGTH(PRESETS); i++) {
                Candidate c = number_candidate(0, false);
                c.color = PRESETS[i].color;
                add_candidate(def, list, &count, c);
            }
            break;
        case SET_TYPE_PATH:
            break;
    }
    add_candidate(def, list, &count, value_candidate(entry));
    add_candidate(def, list, &count, value_candidate(current));

    // Put them in order: an insertion sort, since a row has a few dozen steps at most
    for (int i = 1; i < count; i++) {
        Candidate c = list[i];
        long key = sort_key(def, &c);
        int j = i - 1;
        while (j >= 0 && sort_key(def, &list[j]) > key) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = c;
    }
    return count;
}

// A function to step a value one place left (direction < 0) or right; it stops at the ends
SettingValue setting_step(const SettingDef *def, const SettingValue *current, const SettingValue *entry, int direction)
{
    SettingValue result = *current;
    if (def->type == SET_TYPE_PATH || direction == 0)
        return result;
    Candidate list[MAX_CANDIDATES];
    int count = build_candidates(def, current, entry, list);
    Candidate now = value_candidate(current);
    int index = -1;
    for (int i = 0; i < count && index < 0; i++) {
        if (same_candidate(def, &list[i], &now))
            index = i;
    }
    int next = index + (direction > 0 ? 1 : -1);
    if (index < 0 || next < 0 || next >= count)
        return result;
    result.inherit = list[next].inherit;
    result.number = list[next].number;
    result.percent = list[next].percent;
    result.color = list[next].color;
    return result;
}

// A function to find the last name in a path, ignoring a trailing separator
static void base_name(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\'))
        length--;
    size_t start = length;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\')
        start--;
    snprintf(out, size, "%.*s", (int) (length - start), path + start);
}

// A function to describe a value as the screen shows it. `inherited` is the value a per-menu
// setting follows (from [Layout]); NULL for the others.
void setting_describe(const SettingDef *def, const SettingValue *value, const SettingValue *inherited, char *out, size_t size)
{
    char text[64];
    switch (def->type) {
        case SET_TYPE_COUNT:
            if (value->inherit)
                snprintf(out, size, "All menus (%d)", inherited != NULL ? inherited->number : 0);
            else
                snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_ICON_SIZE:
            if (!value->inherit)
                snprintf(out, size, "%d px", value->number);
            else if (def->section != NULL)
                snprintf(out, size, "Fill");
            else if (inherited == NULL || inherited->inherit)
                snprintf(out, size, "All menus (Fill)");
            else
                snprintf(out, size, "All menus (%d px)", inherited->number);
            break;
        case SET_TYPE_CHOICE:
            snprintf(out, size, "%s", value->number >= 0 && value->number < LENGTH(MODE_LABELS) ? MODE_LABELS[value->number] : "?");
            break;
        case SET_TYPE_COLOR: {
            int preset = preset_index(value->color);
            if (preset >= 0)
                snprintf(out, size, "%s", PRESETS[preset].name);
            else
                snprintf(out, size, "Custom #%02X%02X%02X", value->color.r, value->color.g, value->color.b);
            break;
        }
        case SET_TYPE_PATH:
            if (value->text[0] == '\0')
                snprintf(out, size, "Choose" ELLIPSIS);
            else
                base_name(value->text, out, size);
            break;
        case SET_TYPE_SECONDS:
            if (value->number >= 60 && value->number % 60 == 0)
                snprintf(out, size, "%d min", value->number / 60);
            else
                snprintf(out, size, "%d s", value->number);
            break;
        case SET_TYPE_MILLIS:
            format_millis(value->number, text, sizeof(text));
            snprintf(out, size, "%s s", text);
            break;
        case SET_TYPE_TITLE_SIZE:
            if (!value->percent)
                snprintf(out, size, "Fixed %d", value->number);
            else if (value->number == TITLE_STEPS[0])
                snprintf(out, size, "Small");
            else if (value->number == TITLE_STEPS[1])
                snprintf(out, size, "Medium");
            else if (value->number == TITLE_STEPS[2])
                snprintf(out, size, "Large");
            else
                snprintf(out, size, "%d%%", value->number);
            break;
    }
}

// One page on the stack
typedef struct {
    SettingsPage page;
    int menu;          // MENU pages: the menu's index, -1 for All menus
    int cursor;
    int entry_mode;    // The background mode when the page opened, for the incomplete-mode rule
} PageRef;

struct SettingsState {
    SettingSlot *slots;
    int slot_count;
    char **names;
    int menu_count;
    PageRef stack[SETTINGS_MAX_DEPTH];
    int depth;
    char notice[256];
    char failure[1400];
};

// A function to start the model over the launcher's menus; the caller then sets every entry value
SettingsState *settings_create(const char *const *menu_names, int menu_count)
{
    SettingsState *state = calloc(1, sizeof(SettingsState));
    if (state == NULL)
        return NULL;
    state->menu_count = menu_count;
    state->slot_count = SET_ID_GLOBAL_COUNT + menu_count * SET_ID_PER_MENU_COUNT;
    state->slots = calloc((size_t) state->slot_count, sizeof(SettingSlot));
    state->names = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(char*));
    if (state->slots == NULL || state->names == NULL) {
        settings_free(state);
        return NULL;
    }
    for (int i = 0; i < menu_count; i++) {
        state->names[i] = strdup(menu_names[i]);
        if (state->names[i] == NULL) {
            settings_free(state);
            return NULL;
        }
    }
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        state->slots[id].def = &DEFS[id];
        state->slots[id].menu = -1;
    }
    for (int m = 0; m < menu_count; m++) {
        for (int k = 0; k < SET_ID_PER_MENU_COUNT; k++) {
            SettingSlot *slot = &state->slots[SET_ID_GLOBAL_COUNT + m * SET_ID_PER_MENU_COUNT + k];
            slot->def = &DEFS[SET_ID_MENU_ROWS + k];
            slot->menu = m;
            slot->value.inherit = true;
            slot->entry.inherit = true;
        }
    }
    state->stack[0].page = SETTINGS_PAGE_TOP;
    state->stack[0].menu = -1;
    return state;
}

// A function to free the model
void settings_free(SettingsState *state)
{
    if (state == NULL)
        return;
    for (int i = 0; state->names != NULL && i < state->menu_count; i++)
        free(state->names[i]);
    free(state->names);
    free(state->slots);
    free(state);
}

// A function to find a setting's slot; `menu` matters only for the per-menu settings
SettingSlot *settings_slot(SettingsState *state, SettingId id, int menu)
{
    if (id < SET_ID_GLOBAL_COUNT)
        return &state->slots[id];
    if (id >= SET_ID_COUNT || menu < 0 || menu >= state->menu_count)
        return NULL;
    return &state->slots[SET_ID_GLOBAL_COUNT + menu * SET_ID_PER_MENU_COUNT + ((int) id - SET_ID_MENU_ROWS)];
}

// A function to count the slots, for walking them all
int settings_slot_count(const SettingsState *state)
{
    return state->slot_count;
}

// A function to get a slot by its place
SettingSlot *settings_slot_at(SettingsState *state, int index)
{
    return index >= 0 && index < state->slot_count ? &state->slots[index] : NULL;
}

// A function to set a setting's value as it is when settings open
void settings_set_entry(SettingsState *state, SettingId id, int menu, const SettingValue *value)
{
    SettingSlot *slot = settings_slot(state, id, menu);
    if (slot != NULL) {
        slot->value = *value;
        slot->entry = *value;
    }
}

// A function to tell whether a setting differs from what it was when settings opened
bool settings_changed(const SettingSlot *slot)
{
    return !setting_equal(slot->def, &slot->value, &slot->entry);
}

// A function to tell whether anything has changed
bool settings_any_changed(const SettingsState *state)
{
    for (int i = 0; i < state->slot_count; i++) {
        if (settings_changed(&state->slots[i]))
            return true;
    }
    return false;
}

// A function to get the value a per-menu setting follows when it inherits
static const SettingValue *inherited_value(SettingsState *state, const SettingSlot *slot)
{
    switch (slot->def->id) {
        case SET_ID_MENU_ROWS:
            return &settings_slot(state, SET_ID_LAYOUT_ROWS, -1)->value;
        case SET_ID_MENU_COLUMNS:
            return &settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1)->value;
        case SET_ID_MENU_ICON_SIZE:
            return &settings_slot(state, SET_ID_LAYOUT_ICON_SIZE, -1)->value;
        default:
            return NULL;
    }
}

// A function to start a row of a kind
static SettingsRow new_row(SettingsRowKind kind, const char *label)
{
    SettingsRow row;
    memset(&row, 0, sizeof(row));
    row.kind = kind;
    row.menu = -1;
    row.enabled = true;
    snprintf(row.label, sizeof(row.label), "%s", label);
    return row;
}

// A function to make a setting's row: stepped, or opening the browser for a path
static SettingsRow setting_row(SettingsState *state, SettingSlot *slot)
{
    SettingsRow row = new_row(slot->def->type == SET_TYPE_PATH ? SETTINGS_ROW_BROWSE : SETTINGS_ROW_SETTING, slot->def->label);
    row.slot = slot;
    setting_describe(slot->def, &slot->value, inherited_value(state, slot), row.value, sizeof(row.value));
    return row;
}

// A function to make a row that opens a page
static SettingsRow link_row(const char *label, const char *value, SettingsPage target, int menu)
{
    SettingsRow row = new_row(SETTINGS_ROW_LINK, label);
    snprintf(row.value, sizeof(row.value), "%s", value);
    row.target = target;
    row.menu = menu;
    return row;
}

// A function to make a row that does something
static SettingsRow action_row(const char *label, SettingsAction action, bool enabled)
{
    SettingsRow row = new_row(SETTINGS_ROW_ACTION, label);
    row.action = action;
    row.enabled = enabled;
    return row;
}

// A function to make a row of text
static SettingsRow note_row(const char *text)
{
    SettingsRow row = new_row(SETTINGS_ROW_NOTE, "");
    row.note = text;
    return row;
}

// A function to summarise a menu's grid (columns x rows), or say it follows All menus
static void grid_summary(SettingsState *state, int menu, char *out, size_t size)
{
    const SettingValue *rows = &settings_slot(state, SET_ID_LAYOUT_ROWS, -1)->value;
    const SettingValue *columns = &settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1)->value;
    if (menu >= 0) {
        const SettingValue *menu_rows = &settings_slot(state, SET_ID_MENU_ROWS, menu)->value;
        const SettingValue *menu_columns = &settings_slot(state, SET_ID_MENU_COLUMNS, menu)->value;
        const SettingValue *menu_icon = &settings_slot(state, SET_ID_MENU_ICON_SIZE, menu)->value;
        if (menu_rows->inherit && menu_columns->inherit && menu_icon->inherit) {
            snprintf(out, size, "All menus");
            return;
        }
        if (!menu_rows->inherit)
            rows = menu_rows;
        if (!menu_columns->inherit)
            columns = menu_columns;
    }
    snprintf(out, size, "%d " TIMES " %d", columns->number, rows->number);
}

// A function to add a row when there is room
static int add_row(SettingsRow *rows, int count, int max, SettingsRow row)
{
    if (count < max)
        rows[count] = row;
    return count + 1;
}

// A function to list the rows of the page on show
int settings_rows(SettingsState *state, SettingsRow *rows, int max)
{
    const PageRef *top = &state->stack[state->depth];
    char text[256];
    int n = 0;
    switch (top->page) {
        case SETTINGS_PAGE_TOP: {
            SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
            setting_describe(mode->def, &mode->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Background", text, SETTINGS_PAGE_BACKGROUND, -1));
            snprintf(text, sizeof(text), state->menu_count == 1 ? "%d menu" : "%d menus", state->menu_count);
            n = add_row(rows, n, max, link_row("Menus", text, SETTINGS_PAGE_MENUS, -1));
            SettingSlot *size = settings_slot(state, SET_ID_TITLE_SIZE, -1);
            setting_describe(size->def, &size->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Titles", text, SETTINGS_PAGE_TITLES, -1));
            n = add_row(rows, n, max, new_row(SETTINGS_ROW_DIVIDER, ""));
            n = add_row(rows, n, max, action_row("Discard changes", SETTINGS_ACTION_DISCARD, settings_any_changed(state)));
            break;
        }
        case SETTINGS_PAGE_BACKGROUND: {
            SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
            n = add_row(rows, n, max, setting_row(state, mode));
            if (mode->value.number == 0)
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_BACKGROUND_COLOR, -1)));
            else if (mode->value.number == MODE_IMAGE)
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1)));
            else if (mode->value.number == MODE_SLIDESHOW) {
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_DIRECTORY, -1)));
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_DURATION, -1)));
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_FADE, -1)));
            }
            else
                n = add_row(rows, n, max, note_row(TRANSPARENT_NOTE));
            break;
        }
        case SETTINGS_PAGE_MENUS:
            grid_summary(state, -1, text, sizeof(text));
            n = add_row(rows, n, max, link_row("All menus", text, SETTINGS_PAGE_MENU, -1));
            n = add_row(rows, n, max, new_row(SETTINGS_ROW_DIVIDER, ""));
            for (int m = 0; m < state->menu_count; m++) {
                grid_summary(state, m, text, sizeof(text));
                n = add_row(rows, n, max, link_row(state->names[m], text, SETTINGS_PAGE_MENU, m));
            }
            break;
        case SETTINGS_PAGE_MENU: {
            int m = top->menu;
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_ROWS : SET_ID_MENU_ROWS, m)));
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_COLUMNS : SET_ID_MENU_COLUMNS, m)));
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_ICON_SIZE : SET_ID_MENU_ICON_SIZE, m)));
            if (m >= 0)
                n = add_row(rows, n, max, note_row(MENU_NOTE));
            break;
        }
        case SETTINGS_PAGE_TITLES:
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_TITLE_SIZE, -1)));
            break;
        case SETTINGS_PAGE_SAVE_FAILED:
            n = add_row(rows, n, max, note_row(state->failure));
            n = add_row(rows, n, max, action_row("Try again", SETTINGS_ACTION_RETRY, true));
            n = add_row(rows, n, max, action_row("Leave without saving", SETTINGS_ACTION_LEAVE, true));
            break;
    }
    return n < max ? n : max;
}

// A function to tell whether the cursor may rest on a row
static bool selectable(const SettingsRow *row)
{
    return row->enabled && row->kind != SETTINGS_ROW_DIVIDER && row->kind != SETTINGS_ROW_NOTE;
}

// A function to keep the cursor on a row it may rest on, since the rows under it can change
static void fix_cursor(SettingsState *state)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    PageRef *top = &state->stack[state->depth];
    if (top->cursor >= count)
        top->cursor = count - 1;
    if (top->cursor < 0)
        top->cursor = 0;
    if (count == 0 || selectable(&rows[top->cursor]))
        return;
    for (int i = top->cursor; i < count; i++) {
        if (selectable(&rows[i])) {
            top->cursor = i;
            return;
        }
    }
    for (int i = top->cursor; i >= 0; i--) {
        if (selectable(&rows[i])) {
            top->cursor = i;
            return;
        }
    }
}

// A function to open a page on top of the one on show
static void push_page(SettingsState *state, SettingsPage page, int menu)
{
    if (state->depth + 1 >= SETTINGS_MAX_DEPTH)
        return;
    state->depth++;
    PageRef *top = &state->stack[state->depth];
    memset(top, 0, sizeof(*top));
    top->page = page;
    top->menu = menu;
    top->entry_mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number;
    fix_cursor(state);
}

// A function to apply the incomplete-mode rule when the Background page is left: Image with no
// image, or Slideshow with no folder, goes back to the mode the page opened with
static SettingsEvent leave_background(SettingsState *state)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
    const char *missing = NULL;
    if (mode->value.number == MODE_IMAGE && settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1)->value.text[0] == '\0')
        missing = "No image was chosen";
    else if (mode->value.number == MODE_SLIDESHOW && settings_slot(state, SET_ID_SLIDESHOW_DIRECTORY, -1)->value.text[0] == '\0')
        missing = "No folder was chosen";
    if (missing == NULL)
        return event;
    event.before = mode->value;
    mode->value.number = state->stack[state->depth].entry_mode;
    snprintf(state->notice, sizeof(state->notice), "%s, so Mode went back to %s", missing, MODE_LABELS[mode->value.number]);
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = mode;
    return event;
}

// A function to act on one key of the remote
SettingsEvent settings_command(SettingsState *state, SettingsCommand command)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    PageRef *top = &state->stack[state->depth];
    SettingsRow *row = top->cursor >= 0 && top->cursor < count ? &rows[top->cursor] : NULL;
    bool failure_page = top->page == SETTINGS_PAGE_SAVE_FAILED;

    switch (command) {
        case SETTINGS_UP:
        case SETTINGS_DOWN: {
            int step = command == SETTINGS_UP ? -1 : 1;
            for (int i = top->cursor + step; i >= 0 && i < count; i += step) {
                if (selectable(&rows[i])) {
                    top->cursor = i;
                    event.kind = SETTINGS_EVENT_MOVED;
                    break;
                }
            }
            break;
        }
        case SETTINGS_LEFT:
        case SETTINGS_RIGHT:
            if (row != NULL && row->kind == SETTINGS_ROW_SETTING) {
                SettingSlot *slot = row->slot;
                SettingValue next = setting_step(slot->def, &slot->value, &slot->entry, command == SETTINGS_RIGHT ? 1 : -1);
                if (!setting_equal(slot->def, &next, &slot->value)) {
                    event.before = slot->value;
                    slot->value = next;
                    event.kind = SETTINGS_EVENT_CHANGED;
                    event.slot = slot;
                }
            }
            break;
        case SETTINGS_OK:
            if (row == NULL || !selectable(row))
                break;
            if (row->kind == SETTINGS_ROW_LINK) {
                push_page(state, row->target, row->menu);
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_BROWSE) {
                event.kind = SETTINGS_EVENT_BROWSE;
                event.slot = row->slot;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_DISCARD) {
                for (int i = 0; i < state->slot_count; i++)
                    state->slots[i].value = state->slots[i].entry;
                event.kind = SETTINGS_EVENT_DISCARD;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_RETRY)
                event.kind = SETTINGS_EVENT_RETRY;
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_LEAVE)
                event.kind = SETTINGS_EVENT_LEAVE;
            break;
        case SETTINGS_BACK:
            if (state->depth == 0) {
                event.kind = SETTINGS_EVENT_CLOSE;
                break;
            }
            if (top->page == SETTINGS_PAGE_BACKGROUND)
                event = leave_background(state);
            state->depth--;
            if (event.kind == SETTINGS_EVENT_NONE)
                event.kind = SETTINGS_EVENT_MOVED;
            break;
        case SETTINGS_HOME:
        case SETTINGS_CLOSE:
            if (failure_page)
                break;
            if (top->page == SETTINGS_PAGE_BACKGROUND)
                event = leave_background(state);
            event.kind = command == SETTINGS_HOME ? SETTINGS_EVENT_CLOSE_HOME : SETTINGS_EVENT_CLOSE;
            break;
    }
    fix_cursor(state);
    return event;
}

// A function to set a path chosen in the folder browser
SettingsEvent settings_choose(SettingsState *state, SettingSlot *slot, const char *path)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    if (strlen(path) >= SETTING_TEXT_MAX || strcmp(slot->value.text, path) == 0)
        return event;
    event.before = slot->value;
    snprintf(slot->value.text, SETTING_TEXT_MAX, "%s", path);
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = slot;
    return event;
}

// A function to show why a save failed, with Try again and Leave without saving
void settings_show_save_failed(SettingsState *state, const char *message)
{
    snprintf(state->failure, sizeof(state->failure), "%s", message);
    if (state->stack[state->depth].page != SETTINGS_PAGE_SAVE_FAILED)
        push_page(state, SETTINGS_PAGE_SAVE_FAILED, -1);
    else
        fix_cursor(state);
}

// A function to get the cursor on the page on show
int settings_cursor(const SettingsState *state)
{
    return state->stack[state->depth].cursor;
}

// A function to get the page on show
SettingsPage settings_page(const SettingsState *state)
{
    return state->stack[state->depth].page;
}

// A function to write the path of pages to the one on show: "Settings", "Menus", "Games", joined by ARROW
void settings_path(const SettingsState *state, char *out, size_t size)
{
    snprintf(out, size, "Settings");
    for (int i = 1; i <= state->depth; i++) {
        const PageRef *page = &state->stack[i];
        const char *name = "";
        switch (page->page) {
            case SETTINGS_PAGE_BACKGROUND: name = "Background"; break;
            case SETTINGS_PAGE_MENUS: name = "Menus"; break;
            case SETTINGS_PAGE_MENU: name = page->menu < 0 ? "All menus" : state->names[page->menu]; break;
            case SETTINGS_PAGE_TITLES: name = "Titles"; break;
            case SETTINGS_PAGE_SAVE_FAILED: name = "Couldn't save"; break;
            case SETTINGS_PAGE_TOP: break;
        }
        size_t used = strlen(out);
        snprintf(out + used, size - used, "%s%s", ARROW, name);
    }
}

// A function to say which menu the preview should show: the one a MENU page edits, or the one
// highlighted on the Menus page; -1 for the menu settings were opened from
int settings_preview_menu(SettingsState *state)
{
    const PageRef *top = &state->stack[state->depth];
    if (top->page == SETTINGS_PAGE_MENU)
        return top->menu;
    if (top->page == SETTINGS_PAGE_MENUS) {
        SettingsRow rows[SETTINGS_MAX_ROWS];
        int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
        if (top->cursor >= 0 && top->cursor < count && rows[top->cursor].kind == SETTINGS_ROW_LINK)
            return rows[top->cursor].menu;
    }
    return -1;
}

// A function to get the note the last command left for the caption, or ""
const char *settings_notice(const SettingsState *state)
{
    return state->notice;
}
