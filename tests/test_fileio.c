#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// A folder and a file with non-ASCII names ("fileio-fixture-éß", "café.txt"), relative to the
// folder CTest runs the test in
#define DIR "fileio-fixture-\xC3\xA9\xC3\x9F"
#define CAFE "caf\xC3\xA9.txt"

// A function to make a file read-only or writable again, for the permission checks. The path
// is UTF-8, so Windows needs the wide API.
static void set_read_only(const char *path, bool read_only)
{
#ifdef _WIN32
    wchar_t wide[512];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 512);
    SetFileAttributesW(wide, read_only ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
#else
    chmod(path, read_only ? 0444 : 0644);
#endif
}

// A function to test that files and folders with non-ASCII names can be made, written, read and listed
static void test_non_ascii_round_trip(void)
{
    CHECK(fileio_make_dirs(DIR "/deeper/still"));
    CHECK(fileio_is_dir(DIR "/deeper/still"));
    CHECK(fileio_write_all(DIR "/" CAFE, "one\r\ntwo\n", 9));
    CHECK(fileio_exists(DIR "/" CAFE));
    CHECK(!fileio_is_dir(DIR "/" CAFE));

    size_t length = 0;
    char *text = fileio_read_all(DIR "/" CAFE, &length);
    CHECK(text != NULL && length == 9 && memcmp(text, "one\r\ntwo\n", 9) == 0 && text[9] == '\0');
    free(text);

    FILE *file = fileio_open(DIR "/" CAFE, "rb");
    CHECK(file != NULL);
    if (file != NULL)
        fclose(file);

    FileioEntry *entries = NULL;
    int count = fileio_list(DIR, &entries);
    bool found_file = false, found_dir = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, CAFE) == 0 && !entries[i].is_dir)
            found_file = true;
        if (strcmp(entries[i].name, "deeper") == 0 && entries[i].is_dir)
            found_dir = true;
        CHECK(strcmp(entries[i].name, ".") != 0 && strcmp(entries[i].name, "..") != 0);
    }
    CHECK(found_file);
    CHECK(found_dir);
    fileio_free_list(entries, count);
}

// A function to test copy, replace and remove
static void test_copy_replace_remove(void)
{
    CHECK(fileio_write_all(DIR "/a.ini", "old", 3));
    CHECK(fileio_copy(DIR "/a.ini", DIR "/a.ini.bak"));
    CHECK(fileio_write_all(DIR "/a.ini.tmp", "new", 3));
    CHECK(fileio_replace(DIR "/a.ini.tmp", DIR "/a.ini"));
    CHECK(!fileio_exists(DIR "/a.ini.tmp"));
    char *text = fileio_read_all(DIR "/a.ini", NULL);
    CHECK(text != NULL && strcmp(text, "new") == 0);
    free(text);
    text = fileio_read_all(DIR "/a.ini.bak", NULL);
    CHECK(text != NULL && strcmp(text, "old") == 0);
    free(text);
    CHECK(fileio_remove(DIR "/a.ini.bak"));
    CHECK(!fileio_exists(DIR "/a.ini.bak"));
}

// A function to test that failures return an error and say why
static void test_failures(void)
{
    FileioEntry *entries = NULL;
    CHECK(fileio_list(DIR "/missing", &entries) == -1);
    CHECK(fileio_last_error()[0] != '\0');
    CHECK(fileio_read_all(DIR "/missing.ini", NULL) == NULL);
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
    CHECK(!fileio_exists(DIR "/missing.ini"));
}

// A function to test the writable check. Root can write anything, so there it is skipped.
static void test_writable(void)
{
    set_read_only(DIR "/locked.ini", false);   // A run stopped halfway may have left it read-only
    CHECK(fileio_write_all(DIR "/locked.ini", "x", 1));
    CHECK(fileio_is_writable(DIR "/locked.ini"));
#ifndef _WIN32
    if (geteuid() == 0) {
        printf("skipped the read-only check: running as root\n");
        return;
    }
#endif
    set_read_only(DIR "/locked.ini", true);
    CHECK(!fileio_is_writable(DIR "/locked.ini"));
    CHECK(strstr(fileio_last_error(), "permission denied") != NULL);
    set_read_only(DIR "/locked.ini", false);
}

// A function to test that a name starting with '.' counts as hidden outside Windows
static void test_hidden(void)
{
#ifndef _WIN32
    CHECK(fileio_write_all(DIR "/.hidden", "x", 1));
    FileioEntry *entries = NULL;
    int count = fileio_list(DIR, &entries);
    bool hidden = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, ".hidden") == 0)
            hidden = entries[i].hidden;
    }
    CHECK(hidden);
    fileio_free_list(entries, count);
#endif
}

// A function to test the real path (absolute on Linux, the path as given on Windows), and that
// one too long for its buffer is refused and left empty rather than cut short
static void test_real_path(void)
{
    CHECK(fileio_write_all(DIR "/" CAFE, "x", 1));
    char out[1024];
    CHECK(fileio_real_path(DIR "/" CAFE, out, sizeof(out)));
#ifndef _WIN32
    CHECK(out[0] == '/');
    size_t length = strlen(out);
    size_t name_length = strlen(CAFE);
    CHECK(length > name_length && strcmp(out + length - name_length, CAFE) == 0);
#else
    CHECK(strcmp(out, DIR "/" CAFE) == 0);   // Only Linux follows links: Windows keeps the path as given
#endif

    char too_small[8];
    CHECK(!fileio_real_path(DIR "/" CAFE, too_small, sizeof(too_small)));
    CHECK(strstr(fileio_last_error(), "too long") != NULL);
    CHECK(too_small[0] == '\0');

    // Linux resolves the path, so a missing file fails and says why. The call above left "too
    // long", so "not found" can only come from this one.
#ifndef _WIN32
    CHECK(!fileio_real_path(DIR "/missing.ini", out, sizeof(out)));
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
#endif
}

#ifdef _WIN32
// A function run on a thread: keep the file open, as an antivirus scan does, then let it go
static DWORD WINAPI hold_file(LPVOID handle)
{
    Sleep(300);
    CloseHandle((HANDLE) handle);
    return 0;
}

// A function to test that a replace waits out another program holding the file open
static void test_replace_waits_for_a_held_file(void)
{
    CHECK(fileio_write_all(DIR "/held.ini", "old", 3));
    CHECK(fileio_write_all(DIR "/held.ini.tmp", "new", 3));
    wchar_t wide[512];
    MultiByteToWideChar(CP_UTF8, 0, DIR "/held.ini", -1, wide, 512);
    HANDLE handle = CreateFileW(wide, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(handle != INVALID_HANDLE_VALUE);
    HANDLE thread = CreateThread(NULL, 0, hold_file, handle, 0, NULL);
    CHECK(fileio_replace(DIR "/held.ini.tmp", DIR "/held.ini"));
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    char *text = fileio_read_all(DIR "/held.ini", NULL);
    CHECK(text != NULL && strcmp(text, "new") == 0);
    free(text);
}

// A function to test that a replace keeps the target's hidden and system attributes, so a file
// the user hid is still hidden afterwards
static void test_replace_keeps_a_hidden_target_hidden(void)
{
    wchar_t wide[512];
    MultiByteToWideChar(CP_UTF8, 0, DIR "/hidden.ini", -1, wide, 512);
    SetFileAttributesW(wide, FILE_ATTRIBUTE_NORMAL);   // A run stopped halfway may have left it hidden
    CHECK(fileio_write_all(DIR "/hidden.ini", "old", 3));
    CHECK(SetFileAttributesW(wide, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
    CHECK(fileio_write_all(DIR "/hidden.ini.tmp", "new", 3));
    CHECK(fileio_replace(DIR "/hidden.ini.tmp", DIR "/hidden.ini"));
    DWORD attributes = GetFileAttributesW(wide);
    CHECK(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN) != 0);
    CHECK(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_SYSTEM) != 0);
    char *text = fileio_read_all(DIR "/hidden.ini", NULL);
    CHECK(text != NULL && strcmp(text, "new") == 0);
    free(text);
    SetFileAttributesW(wide, FILE_ATTRIBUTE_NORMAL);
}

// A function to test the UTF-16 copy that start_process() launches commands with
static void test_wide(void)
{
    wchar_t *wide = fileio_wide("caf\xC3\xA9 \"x\"");
    CHECK(wide != NULL && wcscmp(wide, L"caf\x00e9 \"x\"") == 0);   // An escape: MSVC reads the source as cp1252
    free(wide);
    CHECK(fileio_wide("bad \xC3") == NULL);   // A lead byte with nothing after it
    CHECK(strstr(fileio_last_error(), "UTF-8") != NULL);
}
#endif

int main(void)
{
    test_non_ascii_round_trip();
    test_copy_replace_remove();
    test_failures();
    test_writable();
    test_hidden();
    test_real_path();
#ifdef _WIN32
    test_replace_waits_for_a_held_file();
    test_replace_keeps_a_hidden_target_hidden();
    test_wide();
#endif
    return check_report();
}
