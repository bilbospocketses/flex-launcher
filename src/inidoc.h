// config.ini held as its lines, for changing single settings without disturbing anything else:
// comments, blank lines, order, spacing and line endings all survive. Each line is read the way
// inih reads it, so a key found here is the key the launcher reads. Pure: no SDL, no globals.
// A key's continuation lines (indented lines after it, which inih reads as its value again) go
// with it: setting or removing the key removes them, and a new key is never placed where a line
// would become its continuation.
#ifndef INIDOC_H
#define INIDOC_H

#include <stdbool.h>
#include <stddef.h>

// inih reads a line into a 200-byte buffer, so it misreads a line longer than 199 bytes: newer
// versions cut it off and report an error the launcher ignores, older ones read the rest as another line
#define INIDOC_MAX_LINE 199

// inih reads a section's name into a 50-byte buffer (MAX_SECTION in its ini.c), so it keeps only the
// first 49 bytes of a longer name, and two names that agree that far are one section to it
#define INIDOC_MAX_SECTION 49

typedef enum {
    INIDOC_AFTER_LAST_KEY,  // After the section's last key, before any trailing blank or comment lines
    INIDOC_UNDER_HEADER     // Directly under the section header (menus: above the entries)
} IniDocPlacement;

typedef struct IniDoc IniDoc;

IniDoc *inidoc_parse(const char *text, size_t length);
char *inidoc_serialize(const IniDoc *doc, size_t *length);
const char *inidoc_get(const IniDoc *doc, const char *section, const char *key);
bool inidoc_set(IniDoc *doc, const char *section, const char *key, const char *value, IniDocPlacement placement);
bool inidoc_remove(IniDoc *doc, const char *section, const char *key);
const char *inidoc_check(const char *key, const char *value);
void inidoc_free(IniDoc *doc);

#endif
