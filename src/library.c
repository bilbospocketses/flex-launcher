#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <ini.h>
#include "library.h"

typedef struct {
    char name[LIBRARY_NAME_MAX + 1];
    char *file;   // as written in the manifest
    char *path;   // the full path once the file is checked; NULL if the icon was rejected
} LibraryIcon;

static LibraryIcon *icons = NULL;
static int icon_count = 0;
static int icon_capacity = 0;
static LibraryWarn warn_function = NULL;
static char *parsing = NULL;   // the manifest section being read
static int target = -1;        // its index in icons, or -1 while its keys are skipped

// A function to format a message and pass it to the warn function, if one is set
static void warn(const char *format, ...)
{
    if (warn_function == NULL)
        return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    warn_function(message);
}

void library_set_warn(LibraryWarn function)
{
    warn_function = function;
}

// A function to tell a library icon name (1-32 of a-z, 0-9 and '-') from a file path
bool library_is_name(const char *field)
{
    if (field == NULL)
        return false;
    size_t length = strlen(field);
    if (length == 0 || length > LIBRARY_NAME_MAX)
        return false;
    for (size_t i = 0; i < length; i++) {
        char c = field[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    return true;
}

static int find_icon(const char *name)
{
    for (int i = 0; i < icon_count; i++) {
        if (!strcmp(icons[i].name, name))
            return i;
    }
    return -1;
}

static int add_icon(const char *name)
{
    if (icon_count == icon_capacity) {
        int capacity = icon_capacity ? icon_capacity * 2 : 64;
        LibraryIcon *grown = realloc(icons, (size_t) capacity * sizeof(LibraryIcon));
        if (grown == NULL)
            return -1;
        icons = grown;
        icon_capacity = capacity;
    }
    LibraryIcon *icon = &icons[icon_count];
    snprintf(icon->name, sizeof(icon->name), "%s", name);
    icon->file = NULL;
    icon->path = NULL;
    return icon_count++;
}

// The inih handler. inih reports keys, not section headers, so a change of section name marks a new
// section. A repeated section keeps the first; a repeated 'file' keeps the first.
static int handler(void *user, const char *section, const char *key, const char *value)
{
    (void) user;
    if (parsing == NULL || strcmp(parsing, section)) {
        free(parsing);
        parsing = strdup(section);
        target = -1;
        if (!library_is_name(section))
            warn("Icon library: [%s] is not a valid icon name (a-z, 0-9 and '-', up to %i characters), skipping it",
                section, LIBRARY_NAME_MAX);
        else if (find_icon(section) >= 0)
            warn("Icon library: [%s] appears twice, keeping the first", section);
        else
            target = add_icon(section);
    }
    if (target >= 0 && !strcmp(key, "file")) {
        if (icons[target].file != NULL)
            warn("Icon library: [%s] has 'file' twice, keeping the first", section);
        else
            icons[target].file = strdup(value);
    }
    return 1;
}

// A function to check that a manifest path stays inside the library: relative, with no '..' part
static bool inside_library(const char *file)
{
    if (file[0] == '\0' || file[0] == '/' || file[0] == '\\')
        return false;
    if (isalpha((unsigned char) file[0]) && file[1] == ':')
        return false;
    const char *part = file;
    while (*part != '\0') {
        size_t length = strcspn(part, "/\\");
        if (length == 2 && part[0] == '.' && part[1] == '.')
            return false;
        part += length;
        if (*part != '\0')
            part++;
    }
    return true;
}

// A function to read root/icons.ini, keeping every icon whose file is inside the library and exists
int library_load(const char *root)
{
    library_free();
    size_t root_length = strlen(root);
    const char *separator = (root_length > 0 && (root[root_length - 1] == '/' || root[root_length - 1] == '\\'))
                            ? "" : "/";
    size_t manifest_size = root_length + 1 + strlen(LIBRARY_MANIFEST) + 1;
    char *manifest = malloc(manifest_size);
    if (manifest == NULL)
        return -1;
    snprintf(manifest, manifest_size, "%s%s%s", root, separator, LIBRARY_MANIFEST);
    FILE *file = fopen(manifest, "r");
    if (file == NULL) {
        warn("Icon library: could not open %s", manifest);
        free(manifest);
        return -1;
    }
    int error = ini_parse_file(file, handler, NULL);
    fclose(file);
    if (error > 0)
        warn("Icon library: %s line %i could not be read, skipping it", manifest, error);
    else if (error < 0)
        warn("Icon library: could not read %s", manifest);
    free(manifest);
    free(parsing);
    parsing = NULL;
    target = -1;

    int count = 0;
    for (int i = 0; i < icon_count; i++) {
        LibraryIcon *icon = &icons[i];
        if (icon->file == NULL) {
            warn("Icon library: [%s] has no 'file', skipping it", icon->name);
            continue;
        }
        if (!inside_library(icon->file)) {
            warn("Icon library: [%s] file '%s' must be a path inside the library, skipping it", icon->name, icon->file);
            continue;
        }
        size_t path_size = root_length + 1 + strlen(icon->file) + 1;
        char *path = malloc(path_size);
        if (path == NULL)
            continue;
        snprintf(path, path_size, "%s%s%s", root, separator, icon->file);
        FILE *probe = fopen(path, "rb");
        if (probe == NULL) {
            warn("Icon library: [%s] file %s does not exist, skipping it", icon->name, path);
            free(path);
            continue;
        }
        fclose(probe);
        icon->path = path;
        count++;
    }
    return count;
}

const char *library_lookup(const char *name)
{
    if (name == NULL)
        return NULL;
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].path != NULL && !strcmp(icons[i].name, name))
            return icons[i].path;
    }
    return NULL;
}

// A function to map the file name of one of the seven icons older versions shipped to its library name
const char *library_legacy_name(const char *path)
{
    static const char *const legacy[][2] = {
        { "kodi.png", "kodi" },       { "plex.png", "plex" },       { "steam.png", "steam" },
        { "retroarch.png", "retroarch" }, { "system.png", "settings" },
        { "restart.png", "restart" }, { "sleep.png", "sleep" },
    };
    if (path == NULL)
        return NULL;
    const char *base = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    char lower[16];
    size_t length = strlen(base);
    if (length == 0 || length >= sizeof(lower))
        return NULL;
    for (size_t i = 0; i <= length; i++)
        lower[i] = (char) tolower((unsigned char) base[i]);
    for (size_t i = 0; i < sizeof(legacy) / sizeof(legacy[0]); i++) {
        if (!strcmp(lower, legacy[i][0]))
            return legacy[i][1];
    }
    return NULL;
}

void library_free(void)
{
    for (int i = 0; i < icon_count; i++) {
        free(icons[i].file);
        free(icons[i].path);
    }
    free(icons);
    icons = NULL;
    icon_count = 0;
    icon_capacity = 0;
    free(parsing);
    parsing = NULL;
    target = -1;
}
