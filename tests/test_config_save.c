#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "config_save.h"
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define DIR "config-save-fixture"
#define CONFIG DIR "/config.ini"

static const char *const ORIGINAL =
    "; my launcher\n"
    "[Layout]\n"
    "Rows=1\n"
    "MaxButtons=4 ; the old name\n"
    "\n"
    "[Games]\n"
    "IconSize=128\n"
    "Entry1=One;apps;:quit\n";

// A function to make a file read-only or writable again
static void set_read_only(const char *path, bool read_only)
{
#ifdef _WIN32
    SetFileAttributesA(path, read_only ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
#else
    chmod(path, read_only ? 0444 : 0644);
#endif
}

// A function to tell whether read-only files can be tested: root writes them anyway
static bool can_test_read_only(void)
{
#ifndef _WIN32
    if (geteuid() == 0) {
        printf("skipped a read-only check: running as root\n");
        return false;
    }
#endif
    return true;
}

// A function to start a check from a known file, with no backup or temporary file lying about
static void reset(const char *path, const char *text)
{
    char other[256];
    set_read_only(path, false);
    fileio_remove(path);
    snprintf(other, sizeof(other), "%s.bak", path);
    fileio_remove(other);
    snprintf(other, sizeof(other), "%s.tmp", path);
    fileio_remove(other);
    CHECK(fileio_make_dirs(DIR));
    if (text != NULL)
        CHECK(fileio_write_all(path, text, strlen(text)));
}

// A function to test whether a file holds exactly a text
static bool holds(const char *path, const char *text)
{
    char *content = fileio_read_all(path, NULL);
    bool same = content != NULL && strcmp(content, text) == 0;
    free(content);
    return same;
}

// A function to test the edits: a changed value, an alias edited in place, a removed key and a
// new key under a menu header, with the old file kept as .bak and no .tmp left behind
static void test_saves_only_the_edits(void)
{
    reset(CONFIG, ORIGINAL);
    ConfigEdit edits[] = {
        { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY },
        { "Layout", "Columns", "MaxButtons", "5", INIDOC_AFTER_LAST_KEY },
        { "Games", "IconSize", NULL, NULL, INIDOC_UNDER_HEADER },
        { "Games", "Rows", NULL, "3", INIDOC_UNDER_HEADER }
    };
    ConfigSaveResult result;
    CHECK(config_save(CONFIG, NULL, NULL, edits, 4, &result));
    CHECK(holds(CONFIG,
        "; my launcher\n"
        "[Layout]\n"
        "Rows=2\n"
        "MaxButtons=5 ; the old name\n"
        "\n"
        "[Games]\n"
        "Rows=3\n"
        "Entry1=One;apps;:quit\n"));
    CHECK(strstr(result.path, "config-save-fixture") != NULL);
    CHECK(strstr(result.backup, "config.ini.bak") != NULL);
    CHECK(holds(CONFIG ".bak", ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".tmp"));
}

// A function to test that a change made on disk meanwhile, by hand, survives the save
static void test_keeps_a_hand_edit_made_meanwhile(void)
{
    reset(CONFIG, ORIGINAL);
    const char *edited_meanwhile =
        "; my launcher\n"
        "[Layout]\n"
        "Rows=1\n"
        "MaxButtons=4 ; the old name\n"
        "\n"
        "[Games]\n"
        "Columns=6\n"
        "IconSize=128\n"
        "Entry1=One;apps;:quit\n";
    CHECK(fileio_write_all(CONFIG, edited_meanwhile, strlen(edited_meanwhile)));
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    char *content = fileio_read_all(CONFIG, NULL);
    CHECK(content != NULL && strstr(content, "Columns=6\n") != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
}

// A function to test that a value config.ini cannot hold fails the save and leaves the file alone
static void test_refused_value_changes_nothing(void)
{
    reset(CONFIG, ORIGINAL);
    ConfigEdit edit = { "Background", "Image", NULL, "/a ;b.png", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "comment") != NULL);
    CHECK(holds(CONFIG, ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".bak"));
    CHECK(!fileio_exists(CONFIG ".tmp"));
}

// A function to test that a read-only file fails with the reason and is left alone
static void test_read_only_fails(void)
{
    if (!can_test_read_only())
        return;
    reset(CONFIG, ORIGINAL);
    set_read_only(CONFIG, true);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "permission denied") != NULL);
    CHECK(holds(CONFIG, ORIGINAL));
    set_read_only(CONFIG, false);
}

// A function to test the fallback: a read-only config under the system prefix is saved as the
// user's own copy, and the system copy is left alone
static void test_system_copy_falls_back_to_the_user_config(void)
{
    if (!can_test_read_only())
        return;
    const char *system_config = DIR "/system/config.ini";
    const char *user_config = DIR "/home/.config/streamflex/config.ini";
    CHECK(fileio_make_dirs(DIR "/system"));
    reset(system_config, ORIGINAL);
    reset(user_config, NULL);
    set_read_only(system_config, true);
    char prefix[CONFIG_SAVE_PATH_MAX];
    CHECK(fileio_real_path(DIR "/system", prefix, sizeof(prefix) - 1));
    strcat(prefix, "/");
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(system_config, prefix, user_config, &edit, 1, &result));
    CHECK_STR(result.path, user_config);
    CHECK_STR(result.backup, "");
    CHECK(holds(system_config, ORIGINAL));
    char *content = fileio_read_all(user_config, NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL && strstr(content, "; my launcher\n") != NULL);
    free(content);
    set_read_only(system_config, false);

    // Without a system prefix there is no fallback
    set_read_only(system_config, true);
    CHECK(!config_save(system_config, NULL, NULL, &edit, 1, &result));
    set_read_only(system_config, false);
}

// A function to test that a config that has vanished fails with a reason
static void test_missing_file_fails(void)
{
    reset(CONFIG, NULL);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "not found") != NULL);
    CHECK(!fileio_exists(CONFIG));
}

#ifndef _WIN32
// A function to test that a symbolic link is followed: the file it points to changes, the link stays
static void test_follows_a_symbolic_link(void)
{
    reset(DIR "/real.ini", ORIGINAL);
    remove(DIR "/link.ini");
    CHECK(symlink("real.ini", DIR "/link.ini") == 0);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(DIR "/link.ini", NULL, NULL, &edit, 1, &result));
    struct stat info;
    CHECK(lstat(DIR "/link.ini", &info) == 0 && S_ISLNK(info.st_mode));
    char *content = fileio_read_all(DIR "/real.ini", NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
}
#endif

int main(void)
{
    test_saves_only_the_edits();
    test_keeps_a_hand_edit_made_meanwhile();
    test_refused_value_changes_nothing();
    test_read_only_fails();
    test_system_copy_falls_back_to_the_user_config();
    test_missing_file_fails();
#ifndef _WIN32
    test_follows_a_symbolic_link();
#endif
    return check_report();
}
