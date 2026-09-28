#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser.h"

static const char *const IMAGE_EXTENSIONS[] = { ".jpg", ".jpeg", ".png", ".webp" };
static const char *const TOO_FEW = "a slideshow needs 2 or more images";

struct Browser {
    BrowserMode mode;
    BrowserList list;
    BrowserCheck check;
    void *context;
    BrowserPlace *places;   // Copies of the places given
    int place_count;
    char *folder;           // The folder on show; NULL while the places are shown
    BrowserRow *rows;
    int row_count;
    int cursor;
    char chosen[BROWSER_PATH_MAX];
};

// A function to tell a path separator, in either style
static bool is_separator(char c)
{
    return c == '/' || c == '\\';
}

// A function to lower-case an ASCII letter, for sorting and extensions
static int lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : (unsigned char) c;
}

// A function to compare names without regard to ASCII case, then exactly
static int compare_names(const char *a, const char *b)
{
    const char *x = a;
    const char *y = b;
    while (*x != '\0' && lower(*x) == lower(*y)) {
        x++;
        y++;
    }
    int difference = lower(*x) - lower(*y);
    return difference != 0 ? difference : strcmp(a, b);
}

// A function to sort entries by name, for qsort
static int compare_entries(const void *a, const void *b)
{
    const FileioEntry *x = *(const FileioEntry *const *) a;
    const FileioEntry *y = *(const FileioEntry *const *) b;
    return compare_names(x->name, y->name);
}

// A function to tell an image file by its extension, whatever its case
bool browser_is_image(const char *name)
{
    size_t length = strlen(name);
    for (size_t i = 0; i < sizeof(IMAGE_EXTENSIONS) / sizeof(IMAGE_EXTENSIONS[0]); i++) {
        size_t extension_length = strlen(IMAGE_EXTENSIONS[i]);
        if (length <= extension_length)
            continue;
        const char *tail = name + length - extension_length;
        size_t k = 0;
        while (k < extension_length && lower(tail[k]) == IMAGE_EXTENSIONS[i][k])
            k++;
        if (k == extension_length)
            return true;
    }
    return false;
}

// A function to tell a root, which has no parent: "/", "C:", "C:\" or "\\server\share"
static bool is_root(const char *path, size_t length)
{
    if (length == 1 && is_separator(path[0]))
        return true;
    if ((length == 2 || length == 3) && path[1] == ':' && (length == 2 || is_separator(path[2])))
        return true;
    if (length >= 2 && is_separator(path[0]) && is_separator(path[1])) {
        int separators = 0;
        for (size_t i = 2; i < length; i++) {
            if (is_separator(path[i]))
                separators++;
        }
        return separators <= 1;
    }
    return false;
}

// A function to find a path's parent folder; false for a root or a bare name
bool browser_parent(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    while (length > 1 && is_separator(path[length - 1]) && !is_root(path, length))
        length--;
    if (length == 0 || is_root(path, length))
        return false;
    size_t cut = length;
    while (cut > 0 && !is_separator(path[cut - 1]))
        cut--;
    if (cut == 0)
        return false;

    // Keep the separator when the parent is a root ("/", "C:\"), drop it otherwise
    if (!is_root(path, cut))
        cut--;
    if (cut + 1 > size)
        return false;
    memcpy(out, path, cut);
    out[cut] = '\0';
    return true;
}

// A function to join a folder and a name with the folder's own kind of separator
static bool join_path(const char *folder, const char *name, char *out, size_t size)
{
    size_t length = strlen(folder);
    bool windows = strchr(folder, '\\') != NULL || (length >= 2 && folder[1] == ':');
    const char *between = length > 0 && is_separator(folder[length - 1]) ? "" : (windows ? "\\" : "/");
    int written = snprintf(out, size, "%s%s%s", folder, between, name);
    return written > 0 && (size_t) written < size;
}

// A function to free the rows on show
static void free_rows(Browser *browser)
{
    for (int i = 0; i < browser->row_count; i++) {
        free(browser->rows[i].name);
        free(browser->rows[i].path);
    }
    free(browser->rows);
    browser->rows = NULL;
    browser->row_count = 0;
}

// A function to add a row
static void add_row(Browser *browser, int capacity, BrowserRowKind kind, const char *name, const char *path,
                    bool enabled, const char *why)
{
    if (browser->row_count >= capacity)
        return;
    BrowserRow *row = &browser->rows[browser->row_count++];
    row->kind = kind;
    row->name = strdup(name);
    row->path = strdup(path);
    row->enabled = enabled;
    row->why = why;
    row->image_count = 0;
}

// A function to put the cursor on the row for a path, or on the first row
static void select_path(Browser *browser, const char *path)
{
    browser->cursor = 0;
    for (int i = 0; path != NULL && i < browser->row_count; i++) {
        if (strcmp(browser->rows[i].path, path) == 0) {
            browser->cursor = i;
            return;
        }
    }
}

// A function to show the places, with the cursor on `selected` when it is one of them
static void load_places(Browser *browser, const char *selected)
{
    free_rows(browser);
    free(browser->folder);
    browser->folder = NULL;
    browser->rows = calloc((size_t) (browser->place_count > 0 ? browser->place_count : 1), sizeof(BrowserRow));
    for (int i = 0; browser->rows != NULL && i < browser->place_count; i++)
        add_row(browser, browser->place_count, BROWSER_ROW_PLACE, browser->places[i].label, browser->places[i].path, true, NULL);
    select_path(browser, selected);
}

// A function to show a folder: folders first, then images, each sorted by name, hidden files left
// out; in folder mode "Use this folder" comes first. False when the folder cannot be listed.
static bool load_folder(Browser *browser, const char *folder, const char *selected)
{
    FileioEntry *entries = NULL;
    int count = browser->list(folder, &entries, browser->context);
    if (count < 0)
        return false;
    FileioEntry **folders = calloc((size_t) (count > 0 ? count : 1), sizeof(FileioEntry*));
    FileioEntry **images = calloc((size_t) (count > 0 ? count : 1), sizeof(FileioEntry*));
    int folder_count = 0;
    int image_count = 0;
    for (int i = 0; folders != NULL && images != NULL && i < count; i++) {
        if (entries[i].hidden)
            continue;
        if (entries[i].is_dir)
            folders[folder_count++] = &entries[i];
        else if (browser_is_image(entries[i].name))
            images[image_count++] = &entries[i];
    }
    qsort(folders, (size_t) folder_count, sizeof(FileioEntry*), compare_entries);
    qsort(images, (size_t) image_count, sizeof(FileioEntry*), compare_entries);

    char *copy = strdup(folder);   // `folder` may be a row's path, which is about to be freed
    free_rows(browser);
    free(browser->folder);
    browser->folder = copy;
    int capacity = folder_count + image_count + 1;
    browser->rows = calloc((size_t) capacity, sizeof(BrowserRow));
    char path[BROWSER_PATH_MAX];
    if (browser->rows != NULL && browser->mode == BROWSER_FOLDER) {
        const char *why = image_count < 2 ? TOO_FEW : (browser->check != NULL ? browser->check(copy, browser->context) : NULL);
        add_row(browser, capacity, BROWSER_ROW_USE_FOLDER, "Use this folder", copy, why == NULL, why);
        browser->rows[browser->row_count - 1].image_count = image_count;
    }
    for (int i = 0; browser->rows != NULL && i < folder_count; i++) {
        if (join_path(copy, folders[i]->name, path, sizeof(path)))
            add_row(browser, capacity, BROWSER_ROW_FOLDER, folders[i]->name, path, true, NULL);
    }
    for (int i = 0; browser->rows != NULL && i < image_count; i++) {
        if (!join_path(copy, images[i]->name, path, sizeof(path)))
            continue;
        const char *why = NULL;
        if (browser->mode == BROWSER_IMAGE && browser->check != NULL)
            why = browser->check(path, browser->context);
        add_row(browser, capacity, BROWSER_ROW_IMAGE, images[i]->name, path, browser->mode == BROWSER_IMAGE && why == NULL, why);
    }
    free(folders);
    free(images);
    fileio_free_list(entries, count);
    select_path(browser, selected);
    return true;
}

// A function to open the browser: at `start` (an image opens its folder with the image highlighted),
// else at the first place that can be listed (Pictures, then Home, ...), else at the places
Browser *browser_open(BrowserMode mode, const char *start, const BrowserPlace *places, int place_count,
                      BrowserList list, BrowserCheck check, void *context)
{
    Browser *browser = calloc(1, sizeof(Browser));
    if (browser == NULL)
        return NULL;
    browser->mode = mode;
    browser->list = list;
    browser->check = check;
    browser->context = context;
    browser->places = calloc((size_t) (place_count > 0 ? place_count : 1), sizeof(BrowserPlace));
    for (int i = 0; browser->places != NULL && i < place_count; i++) {
        browser->places[i].label = strdup(places[i].label);
        browser->places[i].path = strdup(places[i].path);
        browser->place_count++;
    }
    bool opened = false;
    if (start != NULL && start[0] != '\0') {
        char parent[BROWSER_PATH_MAX];
        if (mode == BROWSER_IMAGE && browser_is_image(start)) {
            if (browser_parent(start, parent, sizeof(parent)))
                opened = load_folder(browser, parent, start);
        }
        else
            opened = load_folder(browser, start, NULL);
    }
    for (int i = 0; !opened && i < browser->place_count; i++)
        opened = load_folder(browser, browser->places[i].path, NULL);
    if (!opened)
        load_places(browser, NULL);
    return browser;
}

// A function to free the browser
void browser_free(Browser *browser)
{
    if (browser == NULL)
        return;
    free_rows(browser);
    free(browser->folder);
    for (int i = 0; i < browser->place_count; i++) {
        free((char*) browser->places[i].label);
        free((char*) browser->places[i].path);
    }
    free(browser->places);
    free(browser);
}

// A function to act on one key: move, page, open a folder or place, choose, or go back up
BrowserResult browser_command(Browser *browser, BrowserCommand command, int page_rows)
{
    int last = browser->row_count - 1;
    int before = browser->cursor;
    if (page_rows < 1)
        page_rows = 1;
    switch (command) {
        case BROWSER_UP:
            if (browser->cursor > 0)
                browser->cursor--;
            break;
        case BROWSER_DOWN:
            if (browser->cursor < last)
                browser->cursor++;
            break;
        case BROWSER_PAGE_UP:
            browser->cursor = browser->cursor - page_rows < 0 ? 0 : browser->cursor - page_rows;
            break;
        case BROWSER_PAGE_DOWN:
            browser->cursor = browser->cursor + page_rows > last ? (last > 0 ? last : 0) : browser->cursor + page_rows;
            break;
        case BROWSER_OK: {
            if (browser->cursor < 0 || browser->cursor > last)
                return BROWSER_NONE;
            const BrowserRow *row = &browser->rows[browser->cursor];
            if (row->kind == BROWSER_ROW_PLACE || row->kind == BROWSER_ROW_FOLDER) {
                char path[BROWSER_PATH_MAX];
                snprintf(path, sizeof(path), "%s", row->path);
                return load_folder(browser, path, NULL) ? BROWSER_MOVED : BROWSER_NONE;
            }
            if (!row->enabled)
                return BROWSER_NONE;
            snprintf(browser->chosen, sizeof(browser->chosen), "%s", row->path);
            return BROWSER_CHOSEN;
        }
        case BROWSER_BACK: {
            if (browser->folder == NULL)
                return BROWSER_CLOSED;
            char from[BROWSER_PATH_MAX];
            char parent[BROWSER_PATH_MAX];
            snprintf(from, sizeof(from), "%s", browser->folder);
            if (!browser_parent(from, parent, sizeof(parent)) || !load_folder(browser, parent, from))
                load_places(browser, from);
            return BROWSER_MOVED;
        }
    }
    return browser->cursor != before ? BROWSER_MOVED : BROWSER_NONE;
}

// A function to count the rows on show
int browser_row_count(const Browser *browser)
{
    return browser->row_count;
}

// A function to get a row on show
const BrowserRow *browser_row(const Browser *browser, int index)
{
    return index >= 0 && index < browser->row_count ? &browser->rows[index] : NULL;
}

// A function to get the cursor
int browser_cursor(const Browser *browser)
{
    return browser->cursor;
}

// A function to get the folder on show; NULL while the places are shown
const char *browser_folder(const Browser *browser)
{
    return browser->folder;
}

// A function to get the path just chosen
const char *browser_chosen(const Browser *browser)
{
    return browser->chosen;
}

// A function to find a folder's first image by name, for previewing a folder
bool browser_first_image(const Browser *browser, const char *folder, char *out, size_t size)
{
    FileioEntry *entries = NULL;
    int count = browser->list(folder, &entries, browser->context);
    const FileioEntry *first = NULL;
    for (int i = 0; i < count; i++) {
        if (entries[i].hidden || entries[i].is_dir || !browser_is_image(entries[i].name))
            continue;
        if (first == NULL || compare_names(entries[i].name, first->name) < 0)
            first = &entries[i];
    }
    bool found = first != NULL && join_path(folder, first->name, out, size);
    fileio_free_list(entries, count > 0 ? count : 0);
    return found;
}
