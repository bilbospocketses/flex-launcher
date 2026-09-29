#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef _WIN32
typedef DIR Folder;   // dirent.h's DIR, which the fixture's name takes over below
#endif

// A folder and a file with non-ASCII names ("fileio-fixture-éß", "café.txt"), relative to the
// folder CTest runs the test in
#define DIR "fileio-fixture-\xC3\xA9\xC3\x9F"
#define CAFE "caf\xC3\xA9.txt"

#ifdef _WIN32
// A function to convert a UTF-8 path for the wide API
static void widen(const char *path, wchar_t *wide, int size)
{
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, size);
}

// A function to delete a folder and everything in it, read-only and hidden files included
static void remove_tree_wide(const wchar_t *folder)
{
    wchar_t pattern[1024];
    swprintf(pattern, 1024, L"%ls\\*", folder);
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(pattern, &data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
                continue;
            wchar_t path[1024];
            swprintf(path, 1024, L"%ls\\%ls", folder, data.cFileName);
            SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                remove_tree_wide(path);
            else
                DeleteFileW(path);
        } while (FindNextFileW(handle, &data));
        FindClose(handle);
    }
    RemoveDirectoryW(folder);
}

// A function to delete a folder and everything in it
static void remove_tree(const char *folder)
{
    wchar_t wide[1024];
    widen(folder, wide, 1024);
    remove_tree_wide(wide);
}
#else
// A function to delete a folder and everything in it. A link is deleted, never followed, and a
// folder a run stopped halfway left unsearchable is opened up first.
static void remove_tree(const char *folder)
{
    chmod(folder, 0755);
    Folder *dir = opendir(folder);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", folder, entry->d_name);
            struct stat info;
            if (lstat(path, &info) == 0 && S_ISDIR(info.st_mode))
                remove_tree(path);
            else
                unlink(path);
        }
        closedir(dir);
    }
    rmdir(folder);
}
#endif

// A function to make a file read-only or writable again, for the permission checks. The path
// is UTF-8, so Windows needs the wide API.
static void set_read_only(const char *path, bool read_only)
{
#ifdef _WIN32
    wchar_t wide[512];
    widen(path, wide, 512);
    SetFileAttributesW(wide, read_only ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
#else
    chmod(path, read_only ? 0444 : 0644);
#endif
}

// A function to find an entry in a listing by name; NULL when it is not there
static const FileioEntry *find_entry(const FileioEntry *entries, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0)
            return &entries[i];
    }
    return NULL;
}

// A function to find a place by its label; NULL when it is not there
static const FileioPlace *find_place(const FileioPlace *places, int count, const char *label)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(places[i].label, label) == 0)
            return &places[i];
    }
    return NULL;
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

// A function to leave a reason behind that no later check expects, so each check below proves
// that its own call set the reason it reads
static void set_other_reason(void)
{
    char too_small[4];
    CHECK(!fileio_real_path(DIR "/" CAFE, too_small, sizeof(too_small)));
    CHECK(strstr(fileio_last_error(), "too long") != NULL);
}

// A function to test that failures return an error and say why
static void test_failures(void)
{
    FileioEntry *entries = NULL;
    CHECK(fileio_list(DIR "/missing", &entries) == -1);
    CHECK(fileio_last_error()[0] != '\0');
    CHECK(fileio_read_all(DIR "/missing.ini", NULL) == NULL);
    CHECK(strstr(fileio_last_error(), "not found") != NULL);

    // Writing, removing and replacing say why they failed
    set_other_reason();
    CHECK(!fileio_write_all(DIR "/no-such-folder/a.ini", "x", 1));
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
    set_other_reason();
    CHECK(!fileio_remove(DIR "/missing.ini"));
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
    set_other_reason();
    CHECK(fileio_write_all(DIR "/target.ini", "x", 1));
    CHECK(!fileio_replace(DIR "/missing.tmp", DIR "/target.ini"));
    CHECK(strstr(fileio_last_error(), "not found") != NULL);

    // Telling whether a path exists, or is a folder, says why not
    set_other_reason();
    CHECK(!fileio_exists(DIR "/missing.ini"));
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
    set_other_reason();
    CHECK(!fileio_is_dir(DIR "/missing"));
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
    set_other_reason();
    CHECK(!fileio_is_dir(DIR "/" CAFE));
    CHECK(strstr(fileio_last_error(), "not a folder") != NULL);
    set_other_reason();
    CHECK(fileio_present(DIR "/" CAFE));
    CHECK(!fileio_present(DIR "/missing.ini"));
    CHECK(strstr(fileio_last_error(), "not found") != NULL);

    // Listing a file says it is not a folder
    set_other_reason();
    CHECK(fileio_list(DIR "/" CAFE, &entries) == -1);
    CHECK(strstr(fileio_last_error(), "not a folder") != NULL);
    CHECK(entries == NULL);
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

// A function to test that a hidden file is listed as hidden: on Windows by its hidden attribute,
// elsewhere by a name starting with '.'
static void test_hidden(void)
{
#ifdef _WIN32
    const char *name = "hidden.txt";
    CHECK(fileio_write_all(DIR "/hidden.txt", "x", 1));
    wchar_t wide[512];
    widen(DIR "/hidden.txt", wide, 512);
    CHECK(SetFileAttributesW(wide, FILE_ATTRIBUTE_HIDDEN));
#else
    const char *name = ".hidden";
    CHECK(fileio_write_all(DIR "/.hidden", "x", 1));
#endif
    FileioEntry *entries = NULL;
    int count = fileio_list(DIR, &entries);
    const FileioEntry *hidden = find_entry(entries, count, name);
    const FileioEntry *shown = find_entry(entries, count, CAFE);
    CHECK(hidden != NULL && hidden->hidden);
    CHECK(shown != NULL && !shown->hidden);
    fileio_free_list(entries, count);
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

// What the other thread in test_errors_are_per_thread() read as its reason
static char other_thread_error[160];

// A function run on another thread: fail in a way of its own, and keep the reason it gets
static void fail_on_another_thread(void)
{
    char too_small[4];
    fileio_real_path(DIR "/" CAFE, too_small, sizeof(too_small));
    snprintf(other_thread_error, sizeof(other_thread_error), "%s", fileio_last_error());
}

#ifdef _WIN32
// A function to start fail_on_another_thread() the Windows way
static DWORD WINAPI other_thread(LPVOID unused)
{
    (void) unused;
    fail_on_another_thread();
    return 0;
}
#else
// A function to start fail_on_another_thread() the POSIX way
static void *other_thread(void *unused)
{
    (void) unused;
    fail_on_another_thread();
    return NULL;
}
#endif

// A function to test that each thread keeps its own reason: another thread's failure never
// changes what this one reads
static void test_errors_are_per_thread(void)
{
    CHECK(fileio_read_all(DIR "/missing.ini", NULL) == NULL);
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, other_thread, NULL, 0, NULL);
    CHECK(thread != NULL);
    if (thread != NULL) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
#else
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, other_thread, NULL) == 0);
    pthread_join(thread, NULL);
#endif
    CHECK(strstr(other_thread_error, "too long") != NULL);
    CHECK(strstr(fileio_last_error(), "not found") != NULL);
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
    widen(DIR "/held.ini", wide, 512);
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
// the user hid is still hidden afterwards, even when the new file has no attribute of its own
static void test_replace_keeps_a_hidden_target_hidden(void)
{
    wchar_t wide[512];
    wchar_t wide_tmp[512];
    widen(DIR "/hidden.ini", wide, 512);
    widen(DIR "/hidden.ini.tmp", wide_tmp, 512);
    CHECK(fileio_write_all(DIR "/hidden.ini", "old", 3));
    CHECK(SetFileAttributesW(wide, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
    CHECK(fileio_write_all(DIR "/hidden.ini.tmp", "new", 3));
    CHECK(SetFileAttributesW(wide_tmp, FILE_ATTRIBUTE_NORMAL));
    CHECK(fileio_replace(DIR "/hidden.ini.tmp", DIR "/hidden.ini"));
    DWORD attributes = GetFileAttributesW(wide);
    CHECK(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN) != 0);
    CHECK(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_SYSTEM) != 0);
    CHECK_STR(fileio_last_warning(), "");
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

// A function to test that folders are made on a share ("\\server\share\..."), whose server and
// share cannot be made and are not tried. The local machine's own device path, "\\.\C:\...", has
// the same two parts.
static void test_make_dirs_on_a_share(void)
{
    wchar_t wide[MAX_PATH];
    DWORD length = GetCurrentDirectoryW(MAX_PATH, wide);
    CHECK(length > 0 && length < MAX_PATH);
    char current[3 * MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, current, (int) sizeof(current), NULL, NULL);
    char path[4 * MAX_PATH];
    snprintf(path, sizeof(path), "\\\\.\\%s\\" DIR "\\share\\one\\two", current);
    CHECK(fileio_make_dirs(path));
    CHECK(fileio_is_dir(DIR "/share/one/two"));
}

// Set while the watcher in test_places_keep_the_process_error_mode() runs, and what it saw
static volatile LONG watching = 0;
static volatile LONG saw_the_mode = 0;

// A function run on a thread: watch the process's error mode for SEM_FAILCRITICALERRORS
static DWORD WINAPI watch_error_mode(LPVOID unused)
{
    (void) unused;
    while (InterlockedCompareExchange(&watching, 0, 0) != 0) {
        if (GetErrorMode() & SEM_FAILCRITICALERRORS)
            InterlockedExchange(&saw_the_mode, 1);
    }
    return 0;
}

// A function to test that listing the places never changes the error mode other threads use: it
// changes its own thread's alone
static void test_places_keep_the_process_error_mode(void)
{
    UINT before = GetErrorMode();
    SetErrorMode(before & ~(UINT) SEM_FAILCRITICALERRORS);
    InterlockedExchange(&watching, 1);
    HANDLE thread = CreateThread(NULL, 0, watch_error_mode, NULL, 0, NULL);
    CHECK(thread != NULL);
    for (int i = 0; i < 20; i++) {
        FileioPlace *places = NULL;
        int count = fileio_places(&places);
        fileio_free_places(places, count);
    }
    InterlockedExchange(&watching, 0);
    if (thread != NULL) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
    CHECK(saw_the_mode == 0);
    SetErrorMode(before);
}
#endif

// A function to test the starting places: at least one, each with a label and a path. None is
// looked at here: a place on a network share is listed unlooked, and looking would wait on it.
static void test_places(void)
{
    FileioPlace *places = NULL;
    int count = fileio_places(&places);
    CHECK(count >= 1);
    for (int i = 0; i < count; i++) {
        CHECK(places[i].label != NULL && places[i].label[0] != '\0');
        CHECK(places[i].path != NULL && places[i].path[0] != '\0');
    }
#ifdef _WIN32
    // A drive is listed only when the drive map knows its kind: never one with no root or of an
    // unknown kind. The system drive is always listed, so this runs at least once. A mapped drive
    // is marked as on the network, and no other drive is.
    int drives = 0;
    for (int i = 0; i < count; i++) {
        const char *path = places[i].path;
        if (path[0] == '\0' || path[1] != ':' || path[2] != '\\' || path[3] != '\0')
            continue;
        wchar_t root[4] = { (wchar_t) (unsigned char) path[0], L':', L'\\', L'\0' };
        UINT type = GetDriveTypeW(root);
        CHECK(type != DRIVE_NO_ROOT_DIR && type != DRIVE_UNKNOWN);
        CHECK(places[i].network == (type == DRIVE_REMOTE));
        drives++;
    }
    CHECK(drives >= 1);
#endif
    fileio_free_places(places, count);
}

#ifndef _WIN32
// The fixture's absolute path, for the pretend mount table and the places, which need one
static char fixture[PATH_MAX];

// A function to write a pretend mount table: an NFS share at <fixture>/nas and an old-style
// sshfs mount, whose type is plain "fuse", at <fixture>/sshfs
static void use_pretend_mounts(void)
{
    char table[3 * PATH_MAX];
    snprintf(table, sizeof(table),
        "proc /proc proc rw 0 0\n"
        "nas:/export %s/nas nfs4 rw 0 0\n"
        "sshfs#me@host: %s/sshfs fuse rw 0 0\n", fixture, fixture);
    CHECK(fileio_write_all(DIR "/mounts", table, strlen(table)));
    fileio_set_mount_table(DIR "/mounts");
}

// A function to set an environment variable, or clear it when `value` is NULL
static void set_variable(const char *name, const char *value)
{
    if (value != NULL)
        setenv(name, value, 1);
    else
        unsetenv(name);
}

// A function to copy an environment variable, to put it back later; NULL when it is not set
static char *save_variable(const char *name)
{
    const char *value = getenv(name);
    return value != NULL ? strdup(value) : NULL;
}

// A function to put an environment variable back as it was, and free the copy
static void restore_variable(const char *name, char *saved)
{
    set_variable(name, saved);
    free(saved);
}

// A function to test that the folders in a mount root such as /media are listed without being looked
// at: a folder, and a link to a folder that is not there (as a dead network mount is), are both
// listed; a file and a hidden folder are not
static void test_places_under(void)
{
    char *user = save_variable("USER");
    set_variable("USER", "streamflex-tester");
    CHECK(fileio_make_dirs(DIR "/media/usb"));
    CHECK(fileio_make_dirs(DIR "/media/.hidden"));
    CHECK(fileio_write_all(DIR "/media/notes.txt", "x", 1));
    CHECK(symlink("/streamflex-no-such-folder", DIR "/media/nas") == 0);
    FileioPlace *places = NULL;
    int count = fileio_places_under(DIR "/media", &places);
    CHECK_INT(count, 2);
    const FileioPlace *usb = find_place(places, count, "usb");
    const FileioPlace *nas = find_place(places, count, "nas");
    CHECK(usb != NULL && strcmp(usb->path, DIR "/media/usb") == 0);
    CHECK(nas != NULL && strcmp(nas->path, DIR "/media/nas") == 0);
    fileio_free_places(places, count);
    restore_variable("USER", user);
}

// A function to test that the user's own folder in /media, where desktops mount drives, is listed
// as the drives inside it, not as one more place
static void test_places_under_the_user_folder(void)
{
    char *user = save_variable("USER");
    set_variable("USER", "streamflex-tester");
    CHECK(fileio_make_dirs(DIR "/media2/streamflex-tester/USB STICK"));
    CHECK(fileio_make_dirs(DIR "/media2/streamflex-tester/Camera"));
    CHECK(fileio_make_dirs(DIR "/media2/someone-else"));
    FileioPlace *places = NULL;
    int count = fileio_places_under(DIR "/media2", &places);
    CHECK_INT(count, 3);
    const FileioPlace *stick = find_place(places, count, "USB STICK");
    const FileioPlace *camera = find_place(places, count, "Camera");
    CHECK(stick != NULL && strcmp(stick->path, DIR "/media2/streamflex-tester/USB STICK") == 0);
    CHECK(camera != NULL && strcmp(camera->path, DIR "/media2/streamflex-tester/Camera") == 0);
    CHECK(find_place(places, count, "someone-else") != NULL);
    CHECK(find_place(places, count, "streamflex-tester") == NULL);
    fileio_free_places(places, count);
    restore_variable("USER", user);
}

// A function to test that a mount whose type is plain "fuse" (older sshfs) counts as a network
// mount: it is listed whole, and nothing in it is read
static void test_places_under_a_fuse_mount(void)
{
    use_pretend_mounts();
    CHECK(fileio_make_dirs(DIR "/sshfs/inside"));
    char folder[PATH_MAX + 16];
    snprintf(folder, sizeof(folder), "%s/sshfs", fixture);
    FileioPlace *places = NULL;
    int count = fileio_places_under(folder, &places);
    CHECK_INT(count, 1);
    CHECK(count == 1 && strcmp(places[0].path, folder) == 0 && places[0].network);
    fileio_free_places(places, count);
    fileio_set_mount_table(NULL);
}

// A function to test what a listing says each entry is: a folder and a file as the folder's own
// listing says, a link as what it names, a link to nothing as no folder, without failing, and a
// link onto a network mount as a folder, without looking at it
static void test_list_kinds(void)
{
    use_pretend_mounts();
    CHECK(fileio_make_dirs(DIR "/kinds/folder"));
    CHECK(fileio_write_all(DIR "/kinds/file.png", "x", 1));
    CHECK(symlink("/streamflex-no-such-folder", DIR "/kinds/dead") == 0);
    CHECK(symlink("folder", DIR "/kinds/to-folder") == 0);
    CHECK(symlink("../nas/photos", DIR "/kinds/to-nas") == 0);

    FileioEntry *entries = NULL;
    int count = fileio_list(DIR "/kinds", &entries);
    CHECK_INT(count, 5);
    const FileioEntry *folder = find_entry(entries, count, "folder");
    const FileioEntry *file = find_entry(entries, count, "file.png");
    const FileioEntry *dead = find_entry(entries, count, "dead");
    const FileioEntry *link = find_entry(entries, count, "to-folder");
    const FileioEntry *nas = find_entry(entries, count, "to-nas");
    CHECK(folder != NULL && folder->is_dir && !folder->hidden);
    CHECK(file != NULL && !file->is_dir && !file->hidden);
    CHECK(dead != NULL && !dead->is_dir);
    CHECK(link != NULL && link->is_dir);
    CHECK(nas != NULL && nas->is_dir);
    fileio_free_list(entries, count);
    fileio_set_mount_table(NULL);

    // With read but no search permission the entries cannot be looked up, as a dead network mount
    // cannot, yet a subfolder is still a folder: the listing says so, and nothing looks it up.
    // Root ignores permissions, so there it is skipped.
    if (geteuid() == 0) {
        printf("skipped the unsearchable folder check: running as root\n");
        return;
    }
    CHECK(chmod(DIR "/kinds", 0644) == 0);
    count = fileio_list(DIR "/kinds", &entries);
    folder = find_entry(entries, count, "folder");
    CHECK(folder != NULL && folder->is_dir);
    fileio_free_list(entries, count);
    chmod(DIR "/kinds", 0755);
}

// A function to test Pictures and Home: Pictures from the desktop's user-dirs.dirs when the
// variable is not set (a German desktop's "Bilder"), and a Pictures folder that is a link onto a
// network mount, listed unlooked and marked
static void test_places_pictures(void)
{
    char *home = save_variable("HOME");
    char *pictures = save_variable("XDG_PICTURES_DIR");
    char *config = save_variable("XDG_CONFIG_HOME");
    set_variable("XDG_PICTURES_DIR", NULL);
    set_variable("XDG_CONFIG_HOME", NULL);

    // user-dirs.dirs names the folder, in the desktop's language, after other lines
    char path[PATH_MAX + 32];
    snprintf(path, sizeof(path), "%s/de", fixture);
    set_variable("HOME", path);
    CHECK(fileio_make_dirs(DIR "/de/.config"));
    CHECK(fileio_make_dirs(DIR "/de/Bilder"));
    const char *dirs =
        "# This file is written by xdg-user-dirs-update\n"
        "XDG_DESKTOP_DIR=\"$HOME/Schreibtisch\"\n"
        "XDG_PICTURES_DIR=\"$HOME/Bilder\"\n";
    CHECK(fileio_write_all(DIR "/de/.config/user-dirs.dirs", dirs, strlen(dirs)));
    FileioPlace *places = NULL;
    int count = fileio_places(&places);
    const FileioPlace *place = find_place(places, count, "Pictures");
    snprintf(path, sizeof(path), "%s/de/Bilder", fixture);
    CHECK(place != NULL && strcmp(place->path, path) == 0 && !place->network);
    place = find_place(places, count, "Home");
    CHECK(place != NULL && !place->network);
    fileio_free_places(places, count);

    // Pictures is a link onto the NAS, which is off: it is listed, marked, and never looked at
    use_pretend_mounts();
    snprintf(path, sizeof(path), "%s/linked", fixture);
    set_variable("HOME", path);
    CHECK(fileio_make_dirs(DIR "/linked"));
    snprintf(path, sizeof(path), "%s/nas/pictures", fixture);
    CHECK(symlink(path, DIR "/linked/Pictures") == 0);
    count = fileio_places(&places);
    place = find_place(places, count, "Pictures");
    snprintf(path, sizeof(path), "%s/linked/Pictures", fixture);
    CHECK(place != NULL && strcmp(place->path, path) == 0 && place->network);
    fileio_free_places(places, count);
    fileio_set_mount_table(NULL);

    restore_variable("HOME", home);
    restore_variable("XDG_PICTURES_DIR", pictures);
    restore_variable("XDG_CONFIG_HOME", config);
}

// A function to test that a replace whose new file cannot take the old one's permissions still
// replaces, and says what it could not keep. A dangling link stands in for the new file: chmod()
// follows it to nothing, while the rename moves the link itself.
static void test_replace_warns_when_permissions_are_lost(void)
{
    CHECK(fileio_write_all(DIR "/kept.ini", "old", 3));
    CHECK(chmod(DIR "/kept.ini", 0600) == 0);
    CHECK(fileio_write_all(DIR "/kept.ini.tmp", "new", 3));
    CHECK(fileio_replace(DIR "/kept.ini.tmp", DIR "/kept.ini"));
    CHECK_STR(fileio_last_warning(), "");
    struct stat info;
    CHECK(stat(DIR "/kept.ini", &info) == 0 && (info.st_mode & 07777) == 0600);

    CHECK(symlink("/streamflex-no-such-file", DIR "/dangling.tmp") == 0);
    CHECK(fileio_replace(DIR "/dangling.tmp", DIR "/kept.ini"));
    CHECK(strstr(fileio_last_warning(), "permissions") != NULL);
    CHECK(lstat(DIR "/kept.ini", &info) == 0 && S_ISLNK(info.st_mode));
}
#endif

int main(void)
{
    remove_tree(DIR);   // Start from nothing, so every run makes the same checks
    CHECK(fileio_make_dirs(DIR));
#ifndef _WIN32
    CHECK(fileio_real_path(DIR, fixture, sizeof(fixture)));
#endif
    test_non_ascii_round_trip();
    test_copy_replace_remove();
    test_failures();
    test_writable();
    test_hidden();
    test_real_path();
    test_errors_are_per_thread();
#ifdef _WIN32
    test_replace_waits_for_a_held_file();
    test_replace_keeps_a_hidden_target_hidden();
    test_wide();
    test_make_dirs_on_a_share();
    test_places_keep_the_process_error_mode();
#endif
    test_places();
#ifndef _WIN32
    test_places_under();
    test_places_under_the_user_folder();
    test_places_under_a_fuse_mount();
    test_list_kinds();
    test_places_pictures();
    test_replace_warns_when_permissions_are_lost();
#endif
    return check_report();
}
