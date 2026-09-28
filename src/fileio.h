// File access with UTF-8 paths on every platform. Windows' narrow file APIs read a path in the
// system code page, so there every call converts it to UTF-16 and uses the wide API. Pure: no
// SDL, no launcher headers, so tests/test_fileio.c builds it on its own.
#ifndef FILEIO_H
#define FILEIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define FILEIO_REPLACE_ATTEMPTS 10   // Windows: how often a held file is retried...
#define FILEIO_REPLACE_WAIT_MS 100   // ...and how long apart: about a second in all

typedef struct {
    char *name;   // UTF-8 name of the file or folder, without its folder
    bool is_dir;
    bool hidden;  // Windows: the hidden or system attribute; elsewhere: the name starts with '.'
} FileioEntry;

FILE *fileio_open(const char *path, const char *mode);
bool fileio_exists(const char *path);
bool fileio_is_dir(const char *path);
bool fileio_is_writable(const char *path);
char *fileio_read_all(const char *path, size_t *length);
bool fileio_write_all(const char *path, const char *data, size_t length);
bool fileio_copy(const char *from, const char *to);
bool fileio_replace(const char *from, const char *to);
bool fileio_remove(const char *path);
bool fileio_make_dirs(const char *path);
bool fileio_real_path(const char *path, char *out, size_t size);   // on Linux, follows symbolic links; on Windows, the path as given
int fileio_list(const char *folder, FileioEntry **entries);
void fileio_free_list(FileioEntry *entries, int count);
const char *fileio_last_error(void);

#ifdef _WIN32
#include <wchar.h>
wchar_t *fileio_wide(const char *text);   // For other Windows calls that take a path or command
#endif

typedef struct {
    char *label;  // What the browser shows: "Pictures", "Home", "C:", "/", a mount's name
    char *path;
} FileioPlace;

int fileio_places(FileioPlace **places);
void fileio_free_places(FileioPlace *places, int count);
#ifndef _WIN32
int fileio_places_under(const char *folder, FileioPlace **places);   // /media's mounts, none looked at
#endif

#endif
