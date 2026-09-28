#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <io.h>
#else
#include <dirent.h>
#include <limits.h>
#include <mntent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

static char last_error[160] = "";

// A function to remember why the last call failed
static void set_error(const char *reason)
{
    snprintf(last_error, sizeof(last_error), "%s", reason);
}

// A function to tell the caller why the last call failed
const char *fileio_last_error(void)
{
    return last_error;
}

// A function to describe a C library error in a few words
static void set_errno_error(int code)
{
    switch (code) {
        case EACCES:
        case EPERM:
            set_error("permission denied");
            break;
#ifdef EROFS
        case EROFS:
            set_error("the file system is read-only");
            break;
#endif
        case ENOSPC:
            set_error("the disk is full");
            break;
        case ENOENT:
        case ENOTDIR:
            set_error("not found");
            break;
        default:
            set_error(strerror(code));
    }
}

#ifdef _WIN32
// A function to describe a Windows error code in a few words
static void set_windows_error(DWORD code)
{
    char text[64];
    switch (code) {
        case ERROR_ACCESS_DENIED:
            set_error("permission denied");
            break;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            set_error("the file is in use by another program");
            break;
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL:
            set_error("the disk is full");
            break;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            set_error("not found");
            break;
        case ERROR_WRITE_PROTECT:
            set_error("the disk is write-protected");
            break;
        default:
            snprintf(text, sizeof(text), "Windows error %lu", (unsigned long) code);
            set_error(text);
    }
}

// A function to convert a UTF-8 string to a new UTF-16 one
static wchar_t *to_wide(const char *text)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (count <= 0) {
        set_error("the path is not valid UTF-8");
        return NULL;
    }
    wchar_t *wide = malloc((size_t) count * sizeof(wchar_t));
    if (wide != NULL)
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count);
    return wide;
}

// A function to convert a UTF-16 string to a new UTF-8 one
static char *to_utf8(const wchar_t *wide)
{
    int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (count <= 0)
        return NULL;
    char *text = malloc((size_t) count);
    if (text != NULL)
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, count, NULL, NULL);
    return text;
}

// A function to give other Windows code a UTF-16 copy of a UTF-8 string, such as a command to launch
wchar_t *fileio_wide(const char *text)
{
    return to_wide(text);
}
#endif

// A function to open a file whose path is UTF-8
FILE *fileio_open(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t *wide_path = to_wide(path);
    wchar_t *wide_mode = to_wide(mode);
    FILE *file = NULL;
    if (wide_path != NULL && wide_mode != NULL) {
        file = _wfopen(wide_path, wide_mode);
        if (file == NULL)
            set_errno_error(errno);
    }
    free(wide_path);
    free(wide_mode);
    return file;
#else
    FILE *file = fopen(path, mode);
    if (file == NULL)
        set_errno_error(errno);
    return file;
#endif
}

// A function to tell whether a file or folder exists and can be read
bool fileio_exists(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    bool exists = wide != NULL && _waccess(wide, 4) == 0;
    free(wide);
    return exists;
#else
    return access(path, R_OK) == 0;
#endif
}

// A function to tell whether a path is a folder
bool fileio_is_dir(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    DWORD attributes = wide != NULL ? GetFileAttributesW(wide) : INVALID_FILE_ATTRIBUTES;
    free(wide);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

// A function to tell whether an existing file can be opened for writing
bool fileio_is_writable(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    if (wide == NULL)
        return false;
    HANDLE handle = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();

        // Another program holding it is a moment's wait, which fileio_replace() handles
        if (code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION)
            return true;
        set_windows_error(code);
        return false;
    }
    CloseHandle(handle);
    return true;
#else
    if (access(path, W_OK) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to read a whole file into a new NUL-terminated buffer
char *fileio_read_all(const char *path, size_t *length)
{
    FILE *file = fileio_open(path, "rb");
    if (file == NULL)
        return NULL;
    size_t capacity = 4096;
    size_t used = 0;
    char *buffer = malloc(capacity + 1);
    while (buffer != NULL) {
        used += fread(buffer + used, 1, capacity - used, file);
        if (used < capacity)
            break;
        capacity *= 2;
        char *bigger = realloc(buffer, capacity + 1);
        if (bigger == NULL) {
            free(buffer);
            buffer = NULL;
        }
        else
            buffer = bigger;
    }
    bool failed = ferror(file) != 0;
    fclose(file);
    if (buffer == NULL || failed) {
        free(buffer);
        set_error(failed ? "the file could not be read" : "out of memory");
        return NULL;
    }
    buffer[used] = '\0';
    if (length != NULL)
        *length = used;
    return buffer;
}

// A function to write a whole file and flush it to the disk before closing it
bool fileio_write_all(const char *path, const char *data, size_t length)
{
    FILE *file = fileio_open(path, "wb");
    if (file == NULL)
        return false;
    bool ok = fwrite(data, 1, length, file) == length && fflush(file) == 0;
#ifdef _WIN32
    ok = ok && _commit(_fileno(file)) == 0;
#else
    ok = ok && fsync(fileno(file)) == 0;
#endif
    if (!ok)
        set_errno_error(errno);
    if (fclose(file) != 0 && ok) {
        set_errno_error(errno);
        ok = false;
    }
    return ok;
}

// A function to copy a file (config files are small, so it goes through memory)
bool fileio_copy(const char *from, const char *to)
{
    size_t length = 0;
    char *data = fileio_read_all(from, &length);
    if (data == NULL)
        return false;
    bool ok = fileio_write_all(to, data, length);
    free(data);
    return ok;
}

// A function to put one file in place of another in a single step. On Windows, antivirus
// scanners and indexers open a file that has just changed, and the replace is refused while
// they hold it, so it is tried again for about a second.
bool fileio_replace(const char *from, const char *to)
{
#ifdef _WIN32
    wchar_t *wide_from = to_wide(from);
    wchar_t *wide_to = to_wide(to);

    // The new file takes the old one's hidden and system attributes, so a file the user hid stays
    // hidden. Read-only is not carried over: a read-only file is refused before it gets here.
    DWORD kept = 0;
    if (wide_to != NULL) {
        DWORD attributes = GetFileAttributesW(wide_to);
        if (attributes != INVALID_FILE_ATTRIBUTES)
            kept = attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }
    bool ok = false;
    for (int attempt = 0; wide_from != NULL && wide_to != NULL && attempt < FILEIO_REPLACE_ATTEMPTS; attempt++) {
        if (MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            ok = true;
            break;
        }
        DWORD code = GetLastError();
        set_windows_error(code);
        if (code != ERROR_SHARING_VIOLATION && code != ERROR_LOCK_VIOLATION && code != ERROR_ACCESS_DENIED)
            break;
        Sleep(FILEIO_REPLACE_WAIT_MS);
    }

    // The new content is in place by now, so an attribute that cannot be set does not fail the replace
    if (ok && kept != 0) {
        DWORD attributes = GetFileAttributesW(wide_to);
        if (attributes != INVALID_FILE_ATTRIBUTES)
            SetFileAttributesW(wide_to, attributes | kept);
    }
    free(wide_from);
    free(wide_to);
    return ok;
#else
    // The new file takes the old one's permission bits
    struct stat info;
    if (stat(to, &info) == 0)
        chmod(from, info.st_mode & 07777);
    if (rename(from, to) != 0) {
        set_errno_error(errno);
        return false;
    }
    return true;
#endif
}

// A function to delete a file
bool fileio_remove(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    bool ok = wide != NULL && DeleteFileW(wide);
    if (wide != NULL && !ok)
        set_windows_error(GetLastError());
    free(wide);
    return ok;
#else
    if (remove(path) == 0)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to make one folder, which may exist already
static bool make_dir(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = to_wide(path);
    bool ok = wide != NULL && (CreateDirectoryW(wide, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    if (wide != NULL && !ok)
        set_windows_error(GetLastError());
    free(wide);
    return ok;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return true;
    set_errno_error(errno);
    return false;
#endif
}

// A function to make a folder and every folder above it that is missing
bool fileio_make_dirs(const char *path)
{
    size_t length = strlen(path);
    char *buffer = malloc(length + 1);
    if (buffer == NULL)
        return false;
    memcpy(buffer, path, length + 1);

    // Make each parent in turn: cut the path at each separator, skipping a leading one ("/")
    // and a drive's ("C:\")
    for (size_t i = 1; i < length; i++) {
        if ((buffer[i] == '/' || buffer[i] == '\\') && buffer[i - 1] != ':') {
            char separator = buffer[i];
            buffer[i] = '\0';
            if (!make_dir(buffer)) {
                free(buffer);
                return false;
            }
            buffer[i] = separator;
        }
    }
    bool ok = make_dir(buffer);
    free(buffer);
    return ok;
}

// A function to copy a path into the caller's buffer. One that does not fit is refused and left
// empty, so a caller that ignores the result never uses a shorter path naming a different file.
static bool copy_path(char *out, size_t size, const char *path)
{
    int written = snprintf(out, size, "%s", path);
    if (written < 0 || written >= (int) size) {
        if (size > 0)
            out[0] = '\0';
        set_error("the path is too long");
        return false;
    }
    return true;
}

// A function to find the file a path really names: on Linux a symbolic link is followed, so a
// save writes the file it points to and leaves the link in place
bool fileio_real_path(const char *path, char *out, size_t size)
{
#ifdef _WIN32
    return copy_path(out, size, path);
#else
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) {
        // Say why it could not be resolved, unless the path did not even fit
        int saved = errno;
        if (copy_path(out, size, path))
            set_errno_error(saved);
        return false;
    }
    return copy_path(out, size, resolved);
#endif
}

// A function to add one entry to a growing list; the list takes over `name`
static void add_entry(FileioEntry **entries, int *count, int *capacity, char *name, bool is_dir, bool hidden)
{
    if (name == NULL)
        return;
    if (*count == *capacity) {
        int grown = *capacity ? *capacity * 2 : 32;
        FileioEntry *bigger = realloc(*entries, (size_t) grown * sizeof(FileioEntry));
        if (bigger == NULL) {
            free(name);
            return;
        }
        *entries = bigger;
        *capacity = grown;
    }
    (*entries)[*count] = (FileioEntry) { .name = name, .is_dir = is_dir, .hidden = hidden };
    (*count)++;
}

// A function to list a folder's files and folders, without "." and ".."
int fileio_list(const char *folder, FileioEntry **entries)
{
    int count = 0;
    int capacity = 0;
    *entries = NULL;
#ifdef _WIN32
    size_t length = strlen(folder);
    char *pattern = malloc(length + 3);
    if (pattern == NULL)
        return -1;
    bool separator = length > 0 && (folder[length - 1] == '\\' || folder[length - 1] == '/');
    snprintf(pattern, length + 3, "%s%s*", folder, separator ? "" : "\\");
    wchar_t *wide = to_wide(pattern);
    free(pattern);
    if (wide == NULL)
        return -1;
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(wide, &data);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND)
            return 0;
        set_windows_error(code);
        return -1;
    }
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
            continue;
        add_entry(entries, &count, &capacity, to_utf8(data.cFileName),
                  (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
                  (data.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0);
    } while (FindNextFileW(handle, &data));
    FindClose(handle);
#else
    DIR *dir = opendir(folder);
    if (dir == NULL) {
        set_errno_error(errno);
        return -1;
    }
    size_t folder_length = strlen(folder);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // The listing says what each entry is, so a folder holding a network mount whose server is
        // off is listed without touching the mount: stat() on it would block, on a hard NFS mount
        // for good. Only a link is followed, since a link to a folder should open like one, and an
        // entry of unknown kind (some file systems give none) is looked up.
        bool is_dir = entry->d_type == DT_DIR;
        if (entry->d_type == DT_LNK || entry->d_type == DT_UNKNOWN) {
            size_t size = folder_length + strlen(entry->d_name) + 2;
            char *full = malloc(size);
            struct stat info;
            if (full != NULL) {
                snprintf(full, size, "%s/%s", folder, entry->d_name);
                is_dir = stat(full, &info) == 0 && S_ISDIR(info.st_mode);
                free(full);
            }
        }
        add_entry(entries, &count, &capacity, strdup(entry->d_name), is_dir, entry->d_name[0] == '.');
    }
    closedir(dir);
#endif
    return count;
}

// A function to free a list from fileio_list
void fileio_free_list(FileioEntry *entries, int count)
{
    for (int i = 0; i < count; i++)
        free(entries[i].name);
    free(entries);
}

// A function to add a starting place without looking at its folder
static void append_place(FileioPlace **places, int *count, const char *label, const char *path)
{
    if (path == NULL)
        return;
    FileioPlace *grown = realloc(*places, (size_t) (*count + 1) * sizeof(FileioPlace));
    if (grown == NULL)
        return;
    *places = grown;
    (*places)[*count].label = strdup(label);
    (*places)[*count].path = strdup(path);
    (*count)++;
}

#ifdef _WIN32
// A function to tell a path on a network share: a UNC path ("\\server\share") or a drive letter
// mapped to one. GetDriveTypeW answers for a drive's root from the drive map, without touching the
// drive, so it cannot wait on the network.
static bool is_network_path(const char *path)
{
    if ((path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/'))
        return true;
    if (path[0] == '\0' || path[1] != ':')
        return false;
    wchar_t root[4] = { (wchar_t) (unsigned char) path[0], L':', L'\\', L'\0' };
    return GetDriveTypeW(root) == DRIVE_REMOTE;
}
#else
// File systems whose server can be switched off. Any FUSE one counts too, since its daemon can hang.
static const char *const NETWORK_TYPES[] = {
    "nfs", "nfs4", "cifs", "smb3", "smbfs", "ncpfs", "afs", "9p", "ceph", "glusterfs", "davfs", "autofs"
};

// A function to tell whether a path is a folder or inside it: "/mnt" and "/mnt/nas" are in "/mnt",
// "/mntx" is not
static bool is_in(const char *path, const char *folder)
{
    size_t length = strlen(folder);
    if (length == 0 || strncmp(path, folder, length) != 0)
        return false;
    return folder[length - 1] == '/' || path[length] == '\0' || path[length] == '/';
}

// A function to tell a path on a network file system, from the mount table: the type of the mount
// with the longest path that holds it. Reading /proc/self/mounts touches none of the mounts.
static bool on_network_mount(const char *path)
{
    FILE *table = setmntent("/proc/self/mounts", "r");
    if (table == NULL)
        return false;
    struct mntent mount;
    char buffer[4096];
    size_t longest = 0;
    bool network = false;
    while (getmntent_r(table, &mount, buffer, (int) sizeof(buffer)) != NULL) {
        size_t length = strlen(mount.mnt_dir);
        if (length < longest || !is_in(path, mount.mnt_dir))
            continue;
        longest = length;   // On a tie the later mount wins: it hides the earlier one
        network = strncmp(mount.mnt_type, "fuse.", 5) == 0;
        for (size_t i = 0; !network && i < sizeof(NETWORK_TYPES) / sizeof(NETWORK_TYPES[0]); i++)
            network = strcmp(mount.mnt_type, NETWORK_TYPES[i]) == 0;
    }
    endmntent(table);
    return network;
}

// A function to tell a path that may be on a network share: in /media or /mnt, where drives and
// shares are mounted, or on a network file system
static bool is_network_path(const char *path)
{
    return is_in(path, "/media") || is_in(path, "/mnt") || on_network_mount(path);
}
#endif

// A function to add a starting place when its folder exists. A folder on a network share is added
// without looking, because looking can wait for the network (see fileio_places).
static void add_place(FileioPlace **places, int *count, const char *label, const char *path)
{
    if (path == NULL || (!is_network_path(path) && !fileio_is_dir(path)))
        return;
    append_place(places, count, label, path);
}

#ifndef _WIN32
// A function to add each folder in a folder as a place: the drives and shares mounted in /media and
// /mnt. Only the folder itself is read, never anything in it, not even with stat(): on a network
// mount whose server is off that blocks, and on a hard NFS mount it never returns. So a name the
// folder lists as a folder, a link or of unknown kind is taken on trust. A folder that is itself a
// network mount is listed whole, unread.
static void add_places_under(FileioPlace **places, int *count, const char *folder)
{
    if (on_network_mount(folder)) {
        append_place(places, count, folder, folder);
        return;
    }
    DIR *dir = opendir(folder);
    if (dir == NULL)
        return;
    size_t folder_length = strlen(folder);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')   // ".", ".." and hidden folders
            continue;
        if (entry->d_type != DT_DIR && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN)
            continue;
        size_t size = folder_length + strlen(entry->d_name) + 2;
        char *path = malloc(size);
        if (path != NULL) {
            snprintf(path, size, "%s/%s", folder, entry->d_name);
            append_place(places, count, entry->d_name, path);
            free(path);
        }
    }
    closedir(dir);
}

// A function to list each folder in a folder as a place, the way /media and /mnt are listed
int fileio_places_under(const char *folder, FileioPlace **places)
{
    int count = 0;
    *places = NULL;
    add_places_under(places, &count, folder);
    return count;
}
#endif

// A function to list where the folder browser can start: Pictures first, then Home, then the
// drives (Windows) or the file system's root and what is mounted in /media and /mnt (elsewhere).
// A network drive or share, or a known folder on one, is listed without being looked at: when its
// server is off, looking blocks until the network times out (Windows tries to reconnect), and on a
// hard NFS mount it never returns, while the browser asks for its places each time it opens. This
// is deliberate: one that cannot be listed is refused only when chosen, like any other folder the
// browser cannot list.
int fileio_places(FileioPlace **places)
{
    int count = 0;
    *places = NULL;
#ifdef _WIN32
    // An empty card reader or DVD drive must not pop up "There is no disk in the drive"
    UINT old_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

    // Without KF_FLAG_DONT_VERIFY the shell looks for the folder itself, network or not;
    // add_place() looks only when the folder is local
    PWSTR wide = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Pictures, (DWORD) KF_FLAG_DONT_VERIFY, NULL, &wide))) {
        char *path = to_utf8(wide);
        add_place(places, &count, "Pictures", path);
        free(path);
    }
    CoTaskMemFree(wide);
    wide = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Profile, (DWORD) KF_FLAG_DONT_VERIFY, NULL, &wide))) {
        char *path = to_utf8(wide);
        add_place(places, &count, "Home", path);
        free(path);
    }
    CoTaskMemFree(wide);

    // The drive map tells each drive's kind without touching it. Only a card reader or DVD drive is
    // looked at, to leave out an empty one.
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if ((drives & (1u << i)) == 0)
            continue;
        char path[4] = { (char) ('A' + i), ':', '\\', '\0' };
        char label[3] = { (char) ('A' + i), ':', '\0' };
        wchar_t root[4] = { (wchar_t) (L'A' + i), L':', L'\\', L'\0' };
        switch (GetDriveTypeW(root)) {
            case DRIVE_FIXED:
            case DRIVE_RAMDISK:
            case DRIVE_REMOTE:
                append_place(places, &count, label, path);
                break;
            case DRIVE_REMOVABLE:
            case DRIVE_CDROM:
                add_place(places, &count, label, path);
                break;
            default:   // DRIVE_NO_ROOT_DIR or DRIVE_UNKNOWN
                break;
        }
    }
    SetErrorMode(old_mode);
#else
    const char *home = getenv("HOME");
    const char *pictures = getenv("XDG_PICTURES_DIR");
    char buffer[4096];
    if (pictures == NULL && home != NULL) {
        snprintf(buffer, sizeof(buffer), "%s/Pictures", home);
        pictures = buffer;
    }
    add_place(places, &count, "Pictures", pictures);
    add_place(places, &count, "Home", home);
    add_place(places, &count, "/", "/");
    add_places_under(places, &count, "/media");
    add_places_under(places, &count, "/mnt");
#endif
    return count;
}

// A function to free a list from fileio_places
void fileio_free_places(FileioPlace *places, int count)
{
    for (int i = 0; i < count; i++) {
        free(places[i].label);
        free(places[i].path);
    }
    free(places);
}
