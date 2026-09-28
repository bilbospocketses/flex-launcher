// config.ini held as its lines, for changing single settings without disturbing anything else:
// comments, blank lines, order, spacing and line endings all survive. Each line is read the way
// inih reads it, so a key found here is the key the launcher reads. Pure: no SDL, no globals.
#ifndef INIDOC_H
#define INIDOC_H

#include <stdbool.h>
#include <stddef.h>

// inih reads a line into a 200-byte buffer, so a line longer than 199 bytes is split in two
#define INIDOC_MAX_LINE 199

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
