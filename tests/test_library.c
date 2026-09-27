#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "check.h"
#include "library.h"

static int warnings = 0;

// A function to count the warnings the library reports, printing each so a failure is readable
static void count_warning(const char *message)
{
    warnings++;
    printf("  warning: %s\n", message);
}

static bool ends_with(const char *string, const char *suffix)
{
    if (string == NULL)
        return false;
    size_t a = strlen(string), b = strlen(suffix);
    return a >= b && !strcmp(string + a - b, suffix);
}

// A function to check that library_legacy_name maps a path to the expected name, or to NULL
static bool legacy_is(const char *path, const char *expected)
{
    const char *name = library_legacy_name(path);
    if (expected == NULL)
        return name == NULL;
    return name != NULL && !strcmp(name, expected);
}

static void test_is_name(void)
{
    CHECK(library_is_name("netflix"));
    CHECK(library_is_name("tv-shows"));
    CHECK(library_is_name("a"));
    CHECK(library_is_name("abc123"));
    CHECK(library_is_name("abcdefghijklmnopqrstuvwxyz012345"));   // 32 characters
    CHECK(!library_is_name("abcdefghijklmnopqrstuvwxyz0123456")); // 33
    CHECK(!library_is_name(NULL));
    CHECK(!library_is_name(""));
    CHECK(!library_is_name("Netflix"));
    CHECK(!library_is_name("netflix.png"));
    CHECK(!library_is_name("icons/netflix"));
    CHECK(!library_is_name("icons\\netflix"));
    CHECK(!library_is_name("net flix"));
    CHECK(!library_is_name("net_flix"));
    CHECK(!library_is_name("./kodi"));
}

static void test_load_good(void)
{
    const char *root = LIBRARY_FIXTURES "/good";
    warnings = 0;
    CHECK_INT(library_load(root), 4);
    CHECK_INT(warnings, 0);
    const char *path = library_lookup("netflix");
    CHECK(ends_with(path, "brands/netflix.png"));
    CHECK(path != NULL && !strncmp(path, root, strlen(root)));
    CHECK(ends_with(library_lookup("movies"), "generic/movies.svg"));
    CHECK(ends_with(library_lookup("apps"), "generic/apps.svg"));
    CHECK(library_lookup("nope") == NULL);
    CHECK(library_lookup("Netflix") == NULL);
    CHECK(library_lookup(NULL) == NULL);
}

static void test_load_bad(void)
{
    warnings = 0;
    CHECK_INT(library_load(LIBRARY_FIXTURES "/bad"), 4);
    CHECK_INT(warnings, 8);
    CHECK(ends_with(library_lookup("dup"), "generic/ok.svg"));     // the first [dup] wins
    CHECK(ends_with(library_lookup("twice"), "generic/ok.svg"));   // the first 'file' wins
    CHECK(ends_with(library_lookup("unknown"), "generic/ok.svg")); // unknown keys are ignored
    CHECK(ends_with(library_lookup("ok"), "generic/ok.svg"));
    CHECK(library_lookup("nofile") == NULL);
    CHECK(library_lookup("abs") == NULL);
    CHECK(library_lookup("abs-win") == NULL);
    CHECK(library_lookup("escape") == NULL);   // ../outside.svg exists and is still refused
    CHECK(library_lookup("missing") == NULL);
    CHECK(library_lookup("Bad_Name") == NULL);
}

static void test_load_none(void)
{
    warnings = 0;
    CHECK_INT(library_load(LIBRARY_FIXTURES "/none"), -1);
    CHECK_INT(warnings, 1);
    CHECK(library_lookup("netflix") == NULL);
}

static void test_reload_and_free(void)
{
    library_load(LIBRARY_FIXTURES "/bad");
    CHECK_INT(library_load(LIBRARY_FIXTURES "/good"), 4);
    CHECK(library_lookup("dup") == NULL);   // nothing survives from the previous load
    library_free();
    CHECK(library_lookup("netflix") == NULL);
    library_free();                          // freeing twice is harmless
}

static void test_legacy(void)
{
    CHECK(legacy_is("C:\\StreamFlex\\assets\\icons\\kodi.png", "kodi"));
    CHECK(legacy_is("/usr/share/streamflex/assets/icons/plex.png", "plex"));
    CHECK(legacy_is("./assets/icons/steam.png", "steam"));
    CHECK(legacy_is("retroarch.png", "retroarch"));
    CHECK(legacy_is("/x/system.png", "settings"));
    CHECK(legacy_is("/x/restart.png", "restart"));
    CHECK(legacy_is("/x/SLEEP.PNG", "sleep"));
    CHECK(legacy_is("/x/netflix.png", NULL));
    CHECK(legacy_is("/x/kodi.svg", NULL));
    CHECK(legacy_is("/x/kodi.png.bak", NULL));
    CHECK(legacy_is("", NULL));
    CHECK(legacy_is(NULL, NULL));
}

// A function to check that library_rescue finds the expected icon (by name and file) for a field, or nothing
static bool rescues_to(const char *field, const char *expected_name, const char *expected_file)
{
    const char *name = "untouched";
    const char *path = library_rescue(field, &name);
    if (expected_name == NULL)
        return path == NULL;
    return path != NULL && ends_with(path, expected_file) && !strcmp(name, expected_name);
}

static void test_rescue(void)
{
    library_load(LIBRARY_FIXTURES "/good");
    // A path to one of the seven icons older versions shipped
    CHECK(rescues_to("C:\\StreamFlex\\assets\\icons\\kodi.png", "kodi", "brands/kodi.png"));
    CHECK(rescues_to("/usr/share/streamflex/assets/icons/KODI.PNG", "kodi", "brands/kodi.png"));
    CHECK(rescues_to("/x/system.png", NULL, NULL));          // maps to settings, which this library lacks
    // A name typed with capitals or stray spaces
    CHECK(rescues_to("Netflix", "netflix", "brands/netflix.png"));
    CHECK(rescues_to(" movies", "movies", "generic/movies.svg"));
    CHECK(rescues_to("MOVIES \t", "movies", "generic/movies.svg"));
    // Nothing to rescue
    CHECK(rescues_to("Nope", NULL, NULL));
    CHECK(rescues_to("netflix.png", NULL, NULL));
    CHECK(rescues_to("net flix", NULL, NULL));
    CHECK(rescues_to("   ", NULL, NULL));
    CHECK(rescues_to("", NULL, NULL));
    CHECK(rescues_to(NULL, NULL, NULL));
    CHECK(rescues_to("abcdefghijklmnopqrstuvwxyz0123456789", NULL, NULL));
    CHECK(library_rescue("Netflix", NULL) != NULL);          // the name out-parameter is optional
    library_free();
    CHECK(library_rescue("Netflix", NULL) == NULL);          // nothing loaded, nothing rescued
}

int main(void)
{
    library_set_warn(count_warning);
    test_is_name();
    test_load_good();
    test_load_bad();
    test_load_none();
    test_reload_and_free();
    test_legacy();
    test_rescue();
    return check_report();
}
