// File access with UTF-8 paths on every platform. Windows' narrow file APIs read a path in the
// system code page, so there every call converts it to UTF-16 and uses the wide API. Pure: no
// SDL, no launcher headers, so tests/test_fileio.c builds it on its own. Memory comes from alloc.h.
// A call that fails says why in fileio_last_error(), which each thread keeps for itself.
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
bool fileio_exists(const char *path);    // exists and can be read
bool fileio_present(const char *path);   // exists, whether or not it can be read
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
const char *fileio_last_warning(void);   // What the last replace could not keep, though it succeeded; "" if nothing
void fileio_set_error(const char *reason);   // For modules built on these, such as the browser, to say why

#ifdef _WIN32
#include <wchar.h>
wchar_t *fileio_wide(const char *text);   // For other Windows calls that take a path or command
#endif

typedef struct {
    char *label;  // What the browser shows: "Pictures", "Home", "C:", "/", a mount's name
    char *path;
    bool network; // It may be on a network share, so it was listed without being looked at
} FileioPlace;

int fileio_places(FileioPlace **places);
void fileio_free_places(FileioPlace *places, int count);
#ifndef _WIN32
int fileio_places_under(const char *folder, FileioPlace **places);   // /media's mounts, none looked at
void fileio_set_mount_table(const char *path);   // Unit tests only: a pretend /proc/self/mounts; NULL for the real one
#endif

#endif
