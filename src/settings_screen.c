// The settings screen: a narrow column of settings on the left and a live preview of the launcher
// on the right, driven by the remote alone. settings.c holds the pages and the values; this file
// draws them, puts each change into the running launcher, and saves on the way out.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "settings_screen.h"
#include "config_save.h"
#include "browser.h"
#include "fileio.h"
#include "inidoc.h"
#include "image.h"
#include "util.h"
#include "debug.h"

extern Config config;
extern Geometry geo;
extern SDL_Renderer *renderer;
extern Menu *current_menu;
extern LayoutGeometry layout;
extern SDL_Texture *background_override;

#define MARGIN_RATIO 0.03F         // Of the screen height
#define HEADER_FONT_RATIO 0.045F
#define ROW_FONT_RATIO 0.028F
#define SMALL_FONT_RATIO 0.02F
#define ROWS_TOP_RATIO 0.17F
#define ROW_HEIGHT_RATIO 1.6F      // Of the row font's line height
#define MIN_COLUMN_RATIO 0.20F     // Of the screen width
#define MAX_COLUMN_RATIO 0.32F
#define TEXT_CACHE_SIZE 96
#define ALPHA_VALUE 180            // A row's value
#define ALPHA_DIM 110              // Greyed rows, the page path and the key hint
#define ALPHA_FILL 40              // The highlighted row
#define ALPHA_OUTLINE 220
#define ALPHA_DIVIDER 46
#define ALPHA_FRAME 77             // The preview's outline
#define LEFT_ARROW "\xE2\x80\xB9"  // U+2039, a single left-pointing angle quote
#define RIGHT_ARROW "\xE2\x80\xBA" // U+203A, a single right-pointing angle quote

static const SDL_Color BACKDROP = { 0x0B, 0x16, 0x20, 0xFF };
static const SDL_Color WHITE = { 0xFF, 0xFF, 0xFF, 0xFF };

// A line of text already rendered, kept while it is still being drawn
typedef struct {
    TTF_Font *font;
    char *text;
    SDL_Texture *texture;
    int w;
    int h;
    Uint32 used;
} CachedText;

static SettingsState *model = NULL;   // NULL while settings are closed
static Menu **menus = NULL;           // The launcher's menus, in the model's order
static int menu_count = 0;
static Menu *origin = NULL;           // The menu settings opened over
static bool go_home = false;          // Go to the default menu after closing (:home)
static SDL_Texture *preview = NULL;   // The scene at full size; NULL when the renderer has no targets
static TTF_Font *font_header = NULL;
static TTF_Font *font_row = NULL;
static TTF_Font *font_small = NULL;
static CachedText text_cache[TEXT_CACHE_SIZE];
static Uint32 text_clock = 0;
static int margin = 0;
static int column_width = 0;
static int row_height = 0;
static int first_row = 0;             // The first row shown when a list is longer than the column
static SDL_Rect preview_rect;
static char counted_folder[SETTING_TEXT_MAX]; // The folder the Folder row last counted; "" counts again
static int counted_images = -1;               // Its images; -1 when it could not be listed

static Browser *browser = NULL;          // The folder browser, while it is open
static SettingSlot *browser_slot = NULL; // The Image or Folder setting it chooses for
static int browser_first = 0;            // Its first row on show
static int browser_page = 1;             // How many of its rows fit: Left and Right move this far
static char browser_note[128] = "";      // Why the last OK did nothing, for the caption

// The preview's image, decoded on its own thread so moving through a folder never stalls
static SDL_Thread *decode_thread = NULL;
static SDL_atomic_t decode_done;
static char decode_path[BROWSER_PATH_MAX];  // What the thread is decoding
static SDL_Surface *decode_surface = NULL;  // Its result; NULL when it failed
static char wanted_path[BROWSER_PATH_MAX];  // What the preview should show; "" = the real background
static char shown_path[BROWSER_PATH_MAX];   // What background_override holds
static char broken_path[BROWSER_PATH_MAX];  // The last image that could not be decoded
static const char CANNOT_OPEN[] = "This image cannot be opened";

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

// A function to tell whether settings are open
bool settings_is_open(void)
{
    return model != NULL;
}

// A function to measure a line of text
static int text_width(TTF_Font *font, const char *text)
{
    int w = 0;
    int h = 0;
    if (text[0] != '\0')
        TTF_SizeUTF8(font, text, &w, &h);
    return w;
}

// A function to get a line of text as a texture, from the cache when it was drawn recently
static CachedText *cached_text(TTF_Font *font, const char *text)
{
    text_clock++;
    int oldest = 0;
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        CachedText *entry = &text_cache[i];
        if (entry->texture != NULL && entry->font == font && strcmp(entry->text, text) == 0) {
            entry->used = text_clock;
            return entry;
        }
        if (entry->used < text_cache[oldest].used)
            oldest = i;
    }
    CachedText *entry = &text_cache[oldest];
    if (entry->texture != NULL) {
        SDL_DestroyTexture(entry->texture);
        free(entry->text);
        memset(entry, 0, sizeof(*entry));
    }
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, WHITE);
    if (surface == NULL)
        return NULL;
    entry->texture = SDL_CreateTextureFromSurface(renderer, surface);
    entry->w = surface->w;
    entry->h = surface->h;
    SDL_FreeSurface(surface);
    if (entry->texture == NULL)
        return NULL;
    entry->font = font;
    entry->text = strdup(text);
    if (entry->text == NULL) {
        SDL_DestroyTexture(entry->texture);
        memset(entry, 0, sizeof(*entry));
        return NULL;
    }
    entry->used = text_clock;
    return entry;
}

// A function to empty the text cache
static void clear_text_cache(void)
{
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (text_cache[i].texture != NULL)
            SDL_DestroyTexture(text_cache[i].texture);
        free(text_cache[i].text);
    }
    memset(text_cache, 0, sizeof(text_cache));
}

// A function to draw a line of text cut with "..." to fit; `right` puts its right edge at x
static void draw_text(TTF_Font *font, const char *text, int x, int y, int max_width, Uint8 alpha, bool right)
{
    if (text == NULL || text[0] == '\0' || max_width <= 0)
        return;
    char buffer[SETTING_TEXT_MAX + 16];
    copy_string(buffer, text, sizeof(buffer));
    int w = text_width(font, buffer);
    if (w > max_width)
        utf8_truncate(buffer, w, max_width);
    CachedText *line = cached_text(font, buffer);
    if (line == NULL)
        return;
    SDL_SetTextureAlphaMod(line->texture, alpha);
    SDL_Rect rect = { right ? x - line->w : x, y, line->w, line->h };
    SDL_RenderCopy(renderer, line->texture, NULL, &rect);
}

// A function to draw a paragraph wrapped to a width; returns its height
static int draw_wrapped(TTF_Font *font, const char *text, int x, int y, int width, Uint8 alpha)
{
    if (text == NULL || text[0] == '\0' || width <= 0)
        return 0;
    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(font, text, WHITE, (Uint32) width);
    if (surface == NULL)
        return 0;
    int h = surface->h;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = { x, y, surface->w, surface->h };
    SDL_FreeSurface(surface);
    if (texture != NULL) {
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_DestroyTexture(texture);
    }
    return h;
}

// A function to name the section a setting lives in: its own, or its menu's
static const char *section_of(const SettingSlot *slot)
{
    return slot->def->section != NULL ? slot->def->section : menus[slot->menu]->name;
}

// A function to read a setting's value from the running launcher
static SettingValue read_value(SettingId id, int menu_index)
{
    SettingValue value;
    memset(&value, 0, sizeof(value));
    Menu *menu = menu_index >= 0 ? menus[menu_index] : NULL;
    switch (id) {
        case SET_ID_BACKGROUND_MODE:
            value.number = (int) config.background_mode;
            break;
        case SET_ID_BACKGROUND_COLOR:
            value.color.r = config.background_color.r;
            value.color.g = config.background_color.g;
            value.color.b = config.background_color.b;
            break;
        case SET_ID_BACKGROUND_IMAGE:
            copy_string(value.text, config.background_image != NULL ? config.background_image : "", sizeof(value.text));
            break;
        case SET_ID_SLIDESHOW_DIRECTORY:
            copy_string(value.text, config.slideshow_directory != NULL ? config.slideshow_directory : "", sizeof(value.text));
            break;
        case SET_ID_SLIDESHOW_DURATION:
            value.number = (int) (config.slideshow_image_duration / 1000);
            break;
        case SET_ID_SLIDESHOW_FADE:
            value.number = (int) config.slideshow_transition_time;
            break;
        case SET_ID_LAYOUT_ROWS:
            value.number = (int) config.rows;
            break;
        case SET_ID_LAYOUT_COLUMNS:
            value.number = (int) config.max_buttons;
            break;
        case SET_ID_LAYOUT_ICON_SIZE:
            value.inherit = config.icon_size == 0;
            value.number = config.icon_size;
            break;
        case SET_ID_TITLE_SIZE:
            value.percent = config.title_font_size_pct > 0;
            value.number = value.percent ? config.title_font_size_pct : (int) config.title_font_size;
            break;
        case SET_ID_MENU_ROWS:
            value.inherit = menu->overrides.rows == 0;
            value.number = menu->overrides.rows;
            break;
        case SET_ID_MENU_COLUMNS:
            value.inherit = menu->overrides.columns == 0;
            value.number = menu->overrides.columns;
            break;
        case SET_ID_MENU_ICON_SIZE:
            value.inherit = menu->overrides.icon_cap == 0;
            value.number = menu->overrides.icon_cap;
            break;
        case SET_ID_COUNT:
            break;
    }
    return value;
}

// A function to replace a config path with a copy of a new one; "" leaves it unset
static void replace_path(char **path, const char *text)
{
    free(*path);
    *path = text[0] != '\0' ? strdup(text) : NULL;
}

// A function to put a setting's value into the running launcher, then refresh what it affects
static void apply_slot(const SettingSlot *slot, bool refresh)
{
    const SettingValue *value = &slot->value;
    Menu *menu = slot->menu >= 0 ? menus[slot->menu] : NULL;
    switch (slot->def->id) {
        case SET_ID_BACKGROUND_MODE:
            config.background_mode = (ModeBackground) value->number;
            break;
        case SET_ID_BACKGROUND_COLOR:
            config.background_color.r = value->color.r;
            config.background_color.g = value->color.g;
            config.background_color.b = value->color.b;
            break;
        case SET_ID_BACKGROUND_IMAGE:
            replace_path(&config.background_image, value->text);
            break;
        case SET_ID_SLIDESHOW_DIRECTORY:
            replace_path(&config.slideshow_directory, value->text);
            break;
        case SET_ID_SLIDESHOW_DURATION:
            config.slideshow_image_duration = (Uint32) value->number * 1000;
            break;
        case SET_ID_SLIDESHOW_FADE:
            config.slideshow_transition_time = (Uint32) value->number;
            update_slideshow_timing();
            break;
        case SET_ID_LAYOUT_ROWS:
            config.rows = (unsigned int) value->number;
            break;
        case SET_ID_LAYOUT_COLUMNS:
            config.max_buttons = (unsigned int) value->number;
            break;
        case SET_ID_LAYOUT_ICON_SIZE:
            config.icon_size = value->inherit ? 0 : (Uint16) value->number;
            break;
        case SET_ID_TITLE_SIZE:
            if (value->percent)
                config.title_font_size_pct = value->number;
            else {
                config.title_font_size_pct = 0;
                config.title_font_size = (unsigned int) value->number;
            }
            break;
        case SET_ID_MENU_ROWS:
            menu->overrides.rows = value->inherit ? 0 : value->number;
            break;
        case SET_ID_MENU_COLUMNS:
            menu->overrides.columns = value->inherit ? 0 : value->number;
            break;
        case SET_ID_MENU_ICON_SIZE:
            menu->overrides.icon_cap = value->inherit ? 0 : value->number;
            break;
        case SET_ID_COUNT:
            break;
    }
    if (!refresh)
        return;
    switch (slot->def->refresh) {
        case SET_REFRESH_LAYOUT:
            refresh_layout();
            break;
        case SET_REFRESH_TITLES:
            reload_titles();
            break;
        case SET_REFRESH_BACKGROUND:
            reload_background();
            break;
        case SET_REFRESH_NONE:
            break;
    }
}

// A function to put every value into the launcher (after Discard), refreshing everything once
static void apply_all(void)
{
    for (int i = 0; i < settings_slot_count(model); i++)
        apply_slot(settings_slot_at(model, i), false);
    reload_background();
    reload_titles();
}

// A function to log a change for -d: "[Section] Key old -> new"
static void log_change(const SettingSlot *slot, const SettingValue *before)
{
    char old_text[SETTING_TEXT_MAX];
    char new_text[SETTING_TEXT_MAX];
    setting_format(slot->def, before, old_text, sizeof(old_text));
    setting_format(slot->def, &slot->value, new_text, sizeof(new_text));
    log_debug("Settings: [%s] %s %s -> %s", section_of(slot), slot->def->key,
        old_text[0] != '\0' ? old_text : "(none)", new_text[0] != '\0' ? new_text : "(none)");
}

// A function to open the screen's own fonts: the bundled default, sized from the screen height
static bool open_fonts(void)
{
    char *path = find_default_font(FILENAME_DEFAULT_FONT);
    if (path == NULL) {
        log_error("Settings: could not find the font %s", FILENAME_DEFAULT_FONT);
        return false;
    }
    float height = (float) geo.screen_height;
    font_header = TTF_OpenFont(path, max_int(8, (int) (HEADER_FONT_RATIO * height)));
    font_row = TTF_OpenFont(path, max_int(8, (int) (ROW_FONT_RATIO * height)));
    font_small = TTF_OpenFont(path, max_int(8, (int) (SMALL_FONT_RATIO * height)));
    free(path);
    if (font_header == NULL || font_row == NULL || font_small == NULL) {
        log_error("Settings: could not open the font %s\n%s", FILENAME_DEFAULT_FONT, TTF_GetError());
        return false;
    }
    return true;
}

// A function to size the column and the preview for this screen. The column is as wide as its
// widest row needs, within limits, and keeps that width while settings are open, so the preview
// never jumps. The preview keeps the screen's shape.
static void measure_layout(void)
{
    static const char *const labels[] = {
        "Background", "Menus", "Titles", "Discard changes", "Mode", "Colour", "Image", "Folder",
        "Change every", "Fade", "Rows", "Columns", "Largest button", "Size", "All menus", "Try again",
        "Leave without saving", "Use this folder"
    };
    static const char *const values[] = {
        LEFT_ARROW " Transparent " RIGHT_ARROW, LEFT_ARROW " Custom #000000 " RIGHT_ARROW,
        LEFT_ARROW " All menus (1024 px) " RIGHT_ARROW, LEFT_ARROW " Fixed 512 " RIGHT_ARROW,
        "12 \xC3\x97 10 " RIGHT_ARROW
    };
    int w = geo.screen_width;
    int h = geo.screen_height;
    margin = (int) (MARGIN_RATIO * (float) h);
    row_height = (int) (ROW_HEIGHT_RATIO * (float) TTF_FontHeight(font_row));
    int widest_label = 0;
    int widest_value = 0;
    for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
        widest_label = max_int(widest_label, text_width(font_row, labels[i]));
    for (int i = 0; i < menu_count; i++)
        widest_label = max_int(widest_label, text_width(font_row, menus[i]->name));
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        widest_value = max_int(widest_value, text_width(font_row, values[i]));
    column_width = widest_label + widest_value + 3 * margin;
    column_width = max_int(column_width, (int) (MIN_COLUMN_RATIO * (float) w));
    column_width = min_int(column_width, (int) (MAX_COLUMN_RATIO * (float) w));

    int x = 2 * margin + column_width;
    int available_w = w - x - margin;
    int caption = 2 * TTF_FontHeight(font_small);
    int available_h = h - 2 * margin - caption;
    int preview_w = available_w;
    int preview_h = preview_w * h / w;
    if (preview_h > available_h) {
        preview_h = available_h;
        preview_w = preview_h * w / h;
    }
    preview_rect.x = x + (available_w - preview_w) / 2;
    preview_rect.y = (h - preview_h - caption) / 2;
    preview_rect.w = preview_w;
    preview_rect.h = preview_h;
}

// A function to show in the preview the menu the page is about: a menu being edited or
// highlighted, else the one settings opened over. A menu with no entries cannot be shown.
static void follow_preview(void)
{
    int index = settings_preview_menu(model);
    Menu *want = index >= 0 && index < menu_count ? menus[index] : origin;
    if (want->num_entries == 0)
        want = origin;
    if (want != current_menu)
        show_menu(want);
}

// A function run on its own thread: decode one image for the preview
static int decode_image(void *data)
{
    UNUSED(data);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: it slows the decode down, so a key can land during it
    const char *delay = getenv("STREAMFLEX_TEST_DECODE_DELAY_MS");
    if (delay != NULL)
        SDL_Delay((Uint32) atoi(delay));
#endif
    decode_surface = IMG_Load(decode_path);
    SDL_AtomicSet(&decode_done, 1);
    return 0;
}

// A function to start decoding the wanted image, unless another is being decoded already, or it
// is on show, or it already failed
static void start_decode(void)
{
    if (decode_thread != NULL || wanted_path[0] == '\0' || strcmp(wanted_path, shown_path) == 0
    || strcmp(wanted_path, broken_path) == 0)
        return;
    copy_string(decode_path, wanted_path, sizeof(decode_path));
    SDL_AtomicSet(&decode_done, 0);
    decode_thread = SDL_CreateThread(decode_image, "Preview image", NULL);
}

// A function to ask for an image as the preview's background; "" goes back to the real one
static void want_preview_image(const char *path)
{
    copy_string(wanted_path, path, sizeof(wanted_path));
    if (wanted_path[0] == '\0' && background_override != NULL) {
        SDL_DestroyTexture(background_override);
        background_override = NULL;
        shown_path[0] = '\0';
    }
    start_decode();
}

// A function to say what the caption says while the browser is open: why the last OK did nothing,
// else that the highlighted image cannot be opened, else why the highlighted row is disabled
static const char *browser_caption(void)
{
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    if (browser_note[0] != '\0')
        return browser_note;
    if (row != NULL && row->kind == BROWSER_ROW_IMAGE && strcmp(row->path, broken_path) == 0)
        return CANNOT_OPEN;
    return row != NULL && row->why != NULL ? row->why : "";
}

// A function to pick up a finished decode (with `wait`, the one in flight, waiting for it): show
// it if it is still wanted, then start the next
static void poll_decode(bool wait)
{
    if (decode_thread == NULL || (!wait && !SDL_AtomicGet(&decode_done)))
        return;
    SDL_WaitThread(decode_thread, NULL);
    decode_thread = NULL;
    if (decode_surface == NULL) {
        copy_string(broken_path, decode_path, sizeof(broken_path));
        log_debug("Settings: could not open %s", decode_path);
        if (browser != NULL && strcmp(browser_caption(), CANNOT_OPEN) == 0)
            log_debug("Settings: the caption says %s for %s", CANNOT_OPEN, decode_path);
    }
    else if (strcmp(decode_path, wanted_path) == 0) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, decode_surface);
        if (texture != NULL) {
            if (background_override != NULL)
                SDL_DestroyTexture(background_override);
            background_override = texture;
            copy_string(shown_path, decode_path, sizeof(shown_path));
            log_debug("Settings: the preview shows %s", shown_path);
        }
    }
    if (decode_surface != NULL)
        SDL_FreeSurface(decode_surface);
    decode_surface = NULL;
    start_decode();
}

// A function to wait out a decode in flight and drop the preview's image
static void stop_decoding(void)
{
    if (decode_thread != NULL) {
        SDL_WaitThread(decode_thread, NULL);
        decode_thread = NULL;
    }
    if (decode_surface != NULL)
        SDL_FreeSurface(decode_surface);
    decode_surface = NULL;
    wanted_path[0] = '\0';
    shown_path[0] = '\0';
    broken_path[0] = '\0';
    if (background_override != NULL)
        SDL_DestroyTexture(background_override);
    background_override = NULL;
}

// A function to list a folder for the browser
static int list_folder(const char *folder, FileioEntry **entries, void *context)
{
    UNUSED(context);
    return fileio_list(folder, entries);
}

// A function to tell the browser whether config.ini can hold a path for its setting
static const char *check_path(const char *path, void *context)
{
    const SettingSlot *slot = context;
    return inidoc_check(slot->def->key, path);
}

// A function to preview what the browser's cursor is on: an image, or a folder's first image
static void preview_highlighted(void)
{
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    char path[BROWSER_PATH_MAX] = "";
    if (row != NULL && row->kind == BROWSER_ROW_IMAGE)
        copy_string(path, row->path, sizeof(path));
    else if (row != NULL && (row->kind == BROWSER_ROW_FOLDER || row->kind == BROWSER_ROW_USE_FOLDER))
        browser_first_image(browser, row->path, path, sizeof(path));
    want_preview_image(path);
}

// A function to open the folder browser for an Image or Folder setting, at its current path
static void open_browser(SettingSlot *slot)
{
    FileioPlace *places = NULL;
    int count = fileio_places(&places);
    BrowserPlace *list = calloc((size_t) (count > 0 ? count : 1), sizeof(BrowserPlace));
    for (int i = 0; list != NULL && i < count; i++) {
        list[i].label = places[i].label;
        list[i].path = places[i].path;
    }
    BrowserMode mode = slot->def->id == SET_ID_BACKGROUND_IMAGE ? BROWSER_IMAGE : BROWSER_FOLDER;
    browser = list != NULL ? browser_open(mode, slot->value.text, list, count, list_folder, check_path, slot) : NULL;
    free(list);
    fileio_free_places(places, count);
    if (browser == NULL)
        return;
    browser_slot = slot;
    browser_first = 0;
    browser_note[0] = '\0';
    log_debug("Settings: browsing %s", browser_folder(browser) != NULL ? browser_folder(browser) : "the places");
    preview_highlighted();
}

// A function to close the folder browser, back to the Background page
static void close_browser(void)
{
    browser_free(browser);
    browser = NULL;
    browser_slot = NULL;
    want_preview_image("");
}

// A function to count a folder's images as the browser does (files with an image's extension,
// hidden ones left out); -1 when it cannot be listed
static int count_images(const char *folder)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(folder, &entries);
    int images = count < 0 ? -1 : 0;
    for (int i = 0; i < count; i++) {
        if (!entries[i].hidden && !entries[i].is_dir && browser_is_image(entries[i].name))
            images++;
    }
    fileio_free_list(entries, count > 0 ? count : 0);
    return images;
}

// A function to add the folder's image count to the Folder row's value, after a middle dot. The
// folder is counted again only when it changes or the Background page opens, never every frame.
static void describe_folder_row(SettingsRow *row)
{
    const char *folder = row->slot->value.text;
    if (folder[0] == '\0')
        return;
    bool recounted = strcmp(folder, counted_folder) != 0;
    if (recounted) {
        copy_string(counted_folder, folder, sizeof(counted_folder));
        counted_images = count_images(folder);
    }
    if (counted_images >= 0) {
        size_t used = strlen(row->value);
        snprintf(row->value + used, sizeof(row->value) - used,
            counted_images == 1 ? " \xC2\xB7 %i image" : " \xC2\xB7 %i images", counted_images);
    }
    if (recounted)
        log_debug("Settings: the Folder row shows %s", row->value);
}

// A function to free what the screen holds while open
static void free_screen(void)
{
    if (browser != NULL)
        close_browser();
    stop_decoding();
    if (preview != NULL)
        SDL_DestroyTexture(preview);
    preview = NULL;
    clear_text_cache();
    if (font_header != NULL)
        TTF_CloseFont(font_header);
    if (font_row != NULL)
        TTF_CloseFont(font_row);
    if (font_small != NULL)
        TTF_CloseFont(font_small);
    font_header = NULL;
    font_row = NULL;
    font_small = NULL;
    settings_free(model);
    model = NULL;
    free(menus);
    menus = NULL;
    menu_count = 0;
}

// A function to close settings, back to the menu they opened over (or the default menu for :home)
static void close_settings(void)
{
    if (origin != NULL && current_menu != origin)
        show_menu(origin);
    free_screen();
    log_debug("Settings closed");
    if (go_home)
        show_home();
}

// A function to save every changed setting into config.ini; on failure, show why
static bool save_changes(void)
{
    int count = settings_slot_count(model);
    ConfigEdit *edits = calloc((size_t) count, sizeof(ConfigEdit));
    char (*values)[SETTING_TEXT_MAX] = calloc((size_t) count, SETTING_TEXT_MAX);
    if (edits == NULL || values == NULL) {
        free(edits);
        free(values);
        settings_show_save_failed(model, "Couldn't save: out of memory");
        return false;
    }
    int n = 0;
    for (int i = 0; i < count; i++) {
        SettingSlot *slot = settings_slot_at(model, i);
        if (!settings_changed(slot))
            continue;
        setting_format(slot->def, &slot->value, values[n], SETTING_TEXT_MAX);
        edits[n].section = section_of(slot);
        edits[n].key = slot->def->key;
        edits[n].alias = slot->def->alias;
        edits[n].value = slot->value.inherit ? NULL : values[n];
        edits[n].placement = slot->def->section != NULL ? INIDOC_AFTER_LAST_KEY : INIDOC_UNDER_HEADER;
        n++;
    }
    ConfigSaveResult result;
    bool ok;
#ifdef __unix__
    // The packaged config cannot be written; the user's own goes where the launcher looks first
    char user_config[MAX_PATH_CHARS + 1];
    const char *home = getenv("HOME");
    if (home != NULL)
        join_paths(user_config, sizeof(user_config), 4, home, ".config", EXECUTABLE_TITLE, FILENAME_DEFAULT_CONFIG);
    ok = config_save(config.config_path, PATH_CONFIG_SYSTEM, home != NULL ? user_config : NULL, edits, n, &result);
#else
    ok = config_save(config.config_path, NULL, NULL, edits, n, &result);
#endif
    free(edits);
    free(values);
    if (ok) {
        log_debug("Settings saved %i change(s) to %s (backup: %s)", n, result.path,
            result.backup[0] != '\0' ? result.backup : "none");
        if (strcmp(result.path, config.config_path) != 0) {
            free(config.config_path);
            config.config_path = strdup(result.path);
        }
        return true;
    }
    char message[CONFIG_SAVE_PATH_MAX + 600];
    snprintf(message, sizeof(message), "Couldn't save to %s: %s", result.path, result.why);
    log_error("%s", message);
    settings_show_save_failed(model, message);
    return false;
}

// A function to save and close; nothing is written when nothing changed
static void save_and_close(void)
{
    if (!settings_any_changed(model)) {
        log_debug("Settings: nothing changed");
        close_settings();
    }
    else if (save_changes())
        close_settings();
}

// A function to act on what a key did in the model
static void handle_event(const SettingsEvent *event)
{
    switch (event->kind) {
        case SETTINGS_EVENT_CHANGED:
            log_change(event->slot, &event->before);
            apply_slot(event->slot, true);
            break;
        case SETTINGS_EVENT_DISCARD:
            log_debug("Settings: discarded the changes");
            apply_all();
            break;
        case SETTINGS_EVENT_BROWSE:
            open_browser(event->slot);
            return;
        case SETTINGS_EVENT_CLOSE:
        case SETTINGS_EVENT_CLOSE_HOME:
            if (event->slot != NULL) {
                log_change(event->slot, &event->before);
                apply_slot(event->slot, true);
            }
            go_home = event->kind == SETTINGS_EVENT_CLOSE_HOME;
            save_and_close();
            return;
        case SETTINGS_EVENT_RETRY:
            save_and_close();
            return;
        case SETTINGS_EVENT_LEAVE:
            log_debug("Settings: leaving without saving");
            close_settings();
            return;
        case SETTINGS_EVENT_MOVED:
        case SETTINGS_EVENT_NONE:
            break;
    }
    follow_preview();
}

// A function to turn a special command into one of the screen's keys
static bool to_settings_command(const char *command, SettingsCommand *out)
{
    static const struct {
        const char *name;
        SettingsCommand command;
    } keys[] = {
        { SCMD_UP, SETTINGS_UP }, { SCMD_DOWN, SETTINGS_DOWN }, { SCMD_LEFT, SETTINGS_LEFT },
        { SCMD_RIGHT, SETTINGS_RIGHT }, { SCMD_SELECT, SETTINGS_OK }, { SCMD_BACK, SETTINGS_BACK },
        { SCMD_HOME, SETTINGS_HOME }, { SCMD_SETTINGS, SETTINGS_CLOSE }
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (MATCH(command, keys[i].name)) {
            *out = keys[i].command;
            return true;
        }
    }
    return false;
}

// A function to act on a key while the folder browser is open: move, page, open, choose or go back
static void handle_browser_command(const char *command)
{
    BrowserCommand key;
    browser_note[0] = '\0';
    if (MATCH(command, SCMD_UP))
        key = BROWSER_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = BROWSER_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = BROWSER_PAGE_UP;
    else if (MATCH(command, SCMD_RIGHT))
        key = BROWSER_PAGE_DOWN;
    else if (MATCH(command, SCMD_SELECT))
        key = BROWSER_OK;
    else if (MATCH(command, SCMD_BACK))
        key = BROWSER_BACK;
    else if (MATCH(command, SCMD_HOME) || MATCH(command, SCMD_SETTINGS)) {
        // Leave the browser without choosing, then close settings as the pages would
        close_browser();
        SettingsEvent event = settings_command(model, MATCH(command, SCMD_HOME) ? SETTINGS_HOME : SETTINGS_CLOSE);
        handle_event(&event);
        return;
    }
    else {
        log_debug("Settings: ignoring '%s' while settings are open", command);
        return;
    }

    BrowserResult result = browser_command(browser, key, browser_page);
    if (result == BROWSER_CLOSED) {
        close_browser();
        return;
    }
    if (result == BROWSER_CHOSEN) {
        char chosen[BROWSER_PATH_MAX];
        copy_string(chosen, browser_chosen(browser), sizeof(chosen));
        if (browser_slot->def->id == SET_ID_BACKGROUND_IMAGE) {
            // OK can come before the image's decode has finished: wait for it, to know whether it
            // opens. An image highlighted before may still be decoding, with this one next.
            if (decode_thread != NULL && strcmp(decode_path, chosen) != 0)
                poll_decode(true);
            if (decode_thread != NULL && strcmp(decode_path, chosen) == 0) {
                log_debug("Settings: OK waited for the decode of %s", chosen);
                poll_decode(true);
            }
        }
        if (strcmp(chosen, broken_path) == 0) {
            snprintf(browser_note, sizeof(browser_note), "%s", CANNOT_OPEN);
            log_debug("Settings: %s: %s", browser_note, chosen);
            return;
        }
        SettingSlot *slot = browser_slot;
        close_browser();
        log_debug("Settings: chose %s", chosen);
        SettingsEvent event = settings_choose(model, slot, chosen);
        handle_event(&event);
        return;
    }
    if (key == BROWSER_OK && result == BROWSER_NONE) {
        // A disabled row says why; a folder or place that could not be listed says what went wrong
        const BrowserRow *row = browser_row(browser, browser_cursor(browser));
        if (row != NULL && row->why != NULL)
            snprintf(browser_note, sizeof(browser_note), "%s", row->why);
        else if (row != NULL && (row->kind == BROWSER_ROW_FOLDER || row->kind == BROWSER_ROW_PLACE))
            snprintf(browser_note, sizeof(browser_note), "Can't open %s: %s", row->name, fileio_last_error());
        if (browser_note[0] != '\0')
            log_debug("Settings: %s", browser_note);
    }
    preview_highlighted();
}

// A function to act on a special command while settings are open: the remote's keys move through
// them, and every other command waits until they close
void settings_handle_command(const char *command)
{
    SettingsCommand key;
    if (model == NULL)
        return;
    if (browser != NULL) {
        handle_browser_command(command);
        return;
    }
    if (!to_settings_command(command, &key)) {
        log_debug("Settings: ignoring '%s' while settings are open", command);
        return;
    }
    SettingsPage before = settings_page(model);
    SettingsEvent event = settings_command(model, key);
    handle_event(&event);
    if (model != NULL && before != SETTINGS_PAGE_BACKGROUND && settings_page(model) == SETTINGS_PAGE_BACKGROUND)
        counted_folder[0] = '\0';   // The Background page opened: count the Folder's images again
}

// A function to draw one row; returns the height it took
static int draw_row(const SettingsRow *row, bool highlighted, int x, int y, int width)
{
    int pad = margin / 2;
    int text_y = y + (row_height - TTF_FontHeight(font_row)) / 2;
    if (row->kind == SETTINGS_ROW_DIVIDER) {
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_DIVIDER);
        SDL_RenderDrawLine(renderer, x, y + row_height / 2, x + width, y + row_height / 2);
        return row_height;
    }
    if (row->kind == SETTINGS_ROW_NOTE) {
        int h = draw_wrapped(font_small, row->note, x + pad, y, width - 2 * pad, ALPHA_VALUE);
        return max_int(row_height, h + row_height / 2);
    }
    if (highlighted) {
        SDL_Rect box = { x, y, width, row_height - 2 };
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FILL);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_OUTLINE);
        SDL_RenderDrawRect(renderer, &box);
    }
    char value[320];
    if (row->kind == SETTINGS_ROW_SETTING && highlighted)
        snprintf(value, sizeof(value), LEFT_ARROW " %s " RIGHT_ARROW, row->value);
    else if (row->kind == SETTINGS_ROW_LINK || row->kind == SETTINGS_ROW_BROWSE)
        snprintf(value, sizeof(value), "%s " RIGHT_ARROW, row->value);
    else
        snprintf(value, sizeof(value), "%s", row->value);
    int value_width = min_int(text_width(font_row, value), width / 2);
    draw_text(font_row, row->label, x + pad, text_y, width - value_width - 3 * pad, row->enabled ? 255 : ALPHA_DIM, false);
    draw_text(font_row, value, x + width - pad, text_y, width / 2, highlighted ? 255 : ALPHA_VALUE, true);
    return row_height;
}

// A function to draw the model's rows between two heights, scrolled to keep the cursor in view
static void draw_model_rows(int x, int top, int bottom)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(model, rows, SETTINGS_MAX_ROWS);
    int cursor = settings_cursor(model);
    int visible = max_int(1, (bottom - top) / row_height);
    if (cursor < first_row)
        first_row = cursor;
    if (cursor >= first_row + visible)
        first_row = cursor - visible + 1;
    first_row = max_int(0, min_int(first_row, count - visible));
    int y = top;
    for (int i = first_row; i < count && y + row_height <= bottom; i++) {
        if (rows[i].slot != NULL && rows[i].slot->def->id == SET_ID_SLIDESHOW_DIRECTORY)
            describe_folder_row(&rows[i]);
        y += draw_row(&rows[i], i == cursor, x, y, column_width);
    }
}

// A function to draw the browser's rows, scrolled to keep the cursor in view
static void draw_browser_rows(int x, int top, int bottom)
{
    int count = browser_row_count(browser);
    int cursor = browser_cursor(browser);
    browser_page = max_int(1, (bottom - top) / row_height);
    if (cursor < browser_first)
        browser_first = cursor;
    if (cursor >= browser_first + browser_page)
        browser_first = cursor - browser_page + 1;
    browser_first = max_int(0, min_int(browser_first, count - browser_page));
    int y = top;
    for (int i = browser_first; i < count && y + row_height <= bottom; i++) {
        const BrowserRow *row = browser_row(browser, i);
        bool opens = row->kind == BROWSER_ROW_PLACE || row->kind == BROWSER_ROW_FOLDER;
        SettingsRow shown;
        memset(&shown, 0, sizeof(shown));
        shown.kind = opens ? SETTINGS_ROW_LINK : SETTINGS_ROW_ACTION;
        shown.enabled = opens || row->enabled;
        copy_string(shown.label, row->name, sizeof(shown.label));
        if (row->kind == BROWSER_ROW_USE_FOLDER)
            snprintf(shown.value, sizeof(shown.value), row->image_count == 1 ? "%i image" : "%i images", row->image_count);
        y += draw_row(&shown, i == cursor, x, y, column_width);
    }
}

// A function to draw the column: the title, the page path (or the folder being browsed), the rows
// and the key hint
static void draw_column(void)
{
    char path[512];
    int x = margin;
    draw_text(font_header, "Settings", x, margin, column_width, 255, false);
    if (browser != NULL)
        copy_string(path, browser_folder(browser) != NULL ? browser_folder(browser) : "Places", sizeof(path));
    else
        settings_path(model, path, sizeof(path));
    draw_text(font_small, path, x, margin + TTF_FontHeight(font_header), column_width, ALPHA_DIM, false);
    int top = (int) (ROWS_TOP_RATIO * (float) geo.screen_height);
    int hint_y = geo.screen_height - margin - TTF_FontHeight(font_small);
    if (browser != NULL)
        draw_browser_rows(x, top, hint_y - margin);
    else
        draw_model_rows(x, top, hint_y - margin);
    const char *hint = browser != NULL ? "Left and right page \xC2\xB7 OK opens or chooses \xC2\xB7 Back goes up"
                     : settings_page(model) == SETTINGS_PAGE_TOP
                       ? "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back saves and closes"
                       : "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back goes back";
    draw_text(font_small, hint, x, hint_y, column_width, ALPHA_DIM, false);
}

// A function to draw the caption under the preview: which menu, its grid and titles, and any note
static void draw_caption(void)
{
    char caption[512];
    char titles[32];
    char why[256];
    LayoutGeometry geometry;
    describe_titles(&layout, titles, sizeof(titles));
    bool reduced = compute_menu_layout(current_menu, &geometry, why, sizeof(why)) == 0 && why[0] != '\0';
    snprintf(caption, sizeof(caption), "Preview: %s \xC2\xB7 %i \xC3\x97 %i, %i px buttons, %s%s", current_menu->name,
        layout.columns, layout.rows, layout.button, titles, reduced ? " (reduced to fit the screen)" : "");
    int y = preview_rect.y + preview_rect.h + margin / 2;
    draw_text(font_small, caption, preview_rect.x, y, preview_rect.w, ALPHA_VALUE, false);
    const char *note = browser != NULL ? browser_caption() : settings_notice(model);
    draw_text(font_small, note, preview_rect.x, y + TTF_FontHeight(font_small), preview_rect.w, 255, false);
}

// A function to draw one frame of the screen and present it
void settings_draw(void)
{
    if (model == NULL)
        return;
    poll_decode(false);
    if (preview != NULL) {
        SDL_SetRenderTarget(renderer, preview);
        draw_scene(true);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, BACKDROP.r, BACKDROP.g, BACKDROP.b, BACKDROP.a);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, preview, NULL, &preview_rect);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FRAME);
        SDL_RenderDrawRect(renderer, &preview_rect);
        draw_caption();
    }
    else {
        // No render targets: the scene fills the screen and the column sits on a dark backing
        draw_scene(true);
        SDL_Rect backing = { 0, 0, column_width + 2 * margin, geo.screen_height };
        SDL_SetRenderDrawColor(renderer, BACKDROP.r, BACKDROP.g, BACKDROP.b, 220);
        SDL_RenderFillRect(renderer, &backing);
    }
    draw_column();
    present_frame();
}

// A function to open settings over the menu on show
void settings_open(void)
{
    if (model != NULL || current_menu == NULL)
        return;
    menu_count = (int) config.num_menus;
    menus = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(Menu*));
    const char **names = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(char*));
    int found = 0;
    for (Menu *menu = config.first_menu; menus != NULL && names != NULL && menu != NULL && found < menu_count; menu = menu->next) {
        menus[found] = menu;
        names[found] = menu->name;
        found++;
    }
    menu_count = found;
    model = menus != NULL && names != NULL ? settings_create(names, menu_count) : NULL;
    free(names);
    if (model == NULL) {
        log_error("Settings: out of memory");
        free_screen();
        return;
    }
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        SettingValue value = read_value((SettingId) id, -1);
        settings_set_entry(model, (SettingId) id, -1, &value);
    }
    for (int m = 0; m < menu_count; m++) {
        for (int id = SET_ID_MENU_ROWS; id < SET_ID_COUNT; id++) {
            SettingValue value = read_value((SettingId) id, m);
            settings_set_entry(model, (SettingId) id, m, &value);
        }
    }
    if (!open_fonts()) {
        free_screen();
        return;
    }
    origin = current_menu;
    go_home = false;
    first_row = 0;
    counted_folder[0] = '\0';
    measure_layout();
    if (SDL_RenderTargetSupported(renderer)) {
        preview = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, geo.screen_width, geo.screen_height);
        if (preview == NULL)
            log_error("Settings: no preview texture, the menu is drawn behind the settings instead\n%s", SDL_GetError());
        else {
            SDL_SetTextureBlendMode(preview, SDL_BLENDMODE_NONE);
            log_debug("Settings: the preview is at %i,%i, %i x %i", preview_rect.x, preview_rect.y,
                preview_rect.w, preview_rect.h);
        }
    }
    log_debug("Settings opened over menu '%s'", origin->name);
}

// A function to let go of settings at quit, saving nothing
void settings_close_now(void)
{
    if (model != NULL)
        free_screen();
}
