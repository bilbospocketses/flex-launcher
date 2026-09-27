// The built-in icon library: a manifest, icons.ini, whose sections name icons and whose 'file' keys give
// each one's path inside the library folder. Pure: inih and the C library only, so it is unit-tested
// without SDL.
#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdbool.h>

#define LIBRARY_MANIFEST "icons.ini"
#define LIBRARY_NAME_MAX 32
#define LIBRARY_FALLBACK_ICON "apps"

// Receives one message per problem found while loading
typedef void (*LibraryWarn)(const char *message);

void        library_set_warn(LibraryWarn function);
bool        library_is_name(const char *field);
int         library_load(const char *root);
const char *library_lookup(const char *name);
const char *library_legacy_name(const char *path);
const char *library_rescue(const char *field, const char **name);
void        library_free(void);

#endif
