#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config_save.h"
#include "fileio.h"
#include "alloc.h"

// A function to tell whether a path lies under a folder
static bool starts_with(const char *path, const char *prefix)
{
    return strncmp(path, prefix, strlen(prefix)) == 0;
}

// A function to cut a file's path down to its folder, in place
static void cut_to_folder(char *path)
{
    size_t length = strlen(path);
    while (length > 0 && path[length - 1] != '/' && path[length - 1] != '\\')
        length--;
    if (length > 1)
        length--;
    path[length] = '\0';
}

// A function to apply the edits to a config's text; false (with the reason) when one cannot be written.
// A key and its older alias never both survive an edit: the alias alone is edited in its place,
// and beside the key it is removed, so the file never holds two lines for one setting.
static bool apply_edits(IniDoc *doc, const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    for (int i = 0; i < count; i++) {
        const ConfigEdit *edit = &edits[i];
        const char *key = edit->key;
        if (edit->alias != NULL && inidoc_get(doc, edit->section, edit->alias) != NULL) {
            if (inidoc_get(doc, edit->section, key) == NULL && edit->value != NULL)
                key = edit->alias;
            else
                inidoc_remove(doc, edit->section, edit->alias);
        }
        if (edit->value == NULL)
            inidoc_remove(doc, edit->section, key);
        else if (!inidoc_set(doc, edit->section, key, edit->value, edit->placement)) {
            snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s",
                key, edit->section, inidoc_why(doc));
            return false;
        }
    }
    return true;
}

// A function to turn the save to the user's own config, which the launcher searches before the
// system copy. One that exists already (an earlier save made it, say) is the file the launcher
// reads next, so it is read and changed like any other, and fails the save when it cannot be read
// or written, rather than being replaced without a backup; otherwise its folder is made and the
// system copy in `source` is its starting point. `source` and `result->path` end up naming the
// file to read and the file to write.
static bool use_user_config(const char *user_config, char *source, size_t size, ConfigSaveResult *result)
{
    if (strlen(user_config) >= sizeof(result->path)) {
        snprintf(result->why, sizeof(result->why), "the user config's path is too long");
        return false;
    }
    snprintf(result->path, sizeof(result->path), "%s", user_config);
    bool present = fileio_present(user_config);
    if (!present && strcmp(fileio_last_error(), "not found") != 0) {
        snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
        return false;
    }
    if (present) {
        if (!fileio_real_path(user_config, source, size) || !fileio_is_writable(source)) {
            snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
            return false;
        }
        snprintf(result->path, sizeof(result->path), "%s", source);
        return true;
    }
    char folder[CONFIG_SAVE_PATH_MAX];
    snprintf(folder, sizeof(folder), "%s", user_config);
    cut_to_folder(folder);
    if (!fileio_make_dirs(folder)) {
        snprintf(result->why, sizeof(result->why), "could not make the folder %.300s: %s", folder, fileio_last_error());
        return false;
    }
    return true;
}

// A function to keep a copy of the file about to be replaced as <file>.bak. The copy is written
// beside it first and then moved over the old backup, so the old one survives until the new one
// is ready, and Windows lets a move replace a hidden backup, which a plain write cannot.
static bool keep_backup(ConfigSaveResult *result)
{
    char temporary[CONFIG_SAVE_PATH_MAX];
    int written = snprintf(result->backup, sizeof(result->backup), "%s.bak", result->path);
    int temporary_written = snprintf(temporary, sizeof(temporary), "%s.bak.tmp", result->path);
    if (written < 0 || written >= (int) sizeof(result->backup) ||
        temporary_written < 0 || temporary_written >= (int) sizeof(temporary)) {
        snprintf(result->why, sizeof(result->why), "could not write the backup: the path is too long");
        result->backup[0] = '\0';
        return false;
    }
    if (!fileio_copy(result->path, temporary) || !fileio_replace(temporary, result->backup)) {
        snprintf(result->why, sizeof(result->why), "could not write the backup %.300s: %s", result->backup, fileio_last_error());
        fileio_remove(temporary);
        result->backup[0] = '\0';
        return false;
    }
    return true;
}

// A function to save the settings screen's changes. `loaded` is the file the launcher read.
// When it cannot be written but lies under `system_prefix` (the packaged copy on Linux), the
// save goes to `user_config` instead, which the launcher searches first; pass NULL for both
// where there is no such fallback (Windows).
bool config_save(const char *loaded, const char *system_prefix, const char *user_config,
                 const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    memset(result, 0, sizeof(*result));
    char source[CONFIG_SAVE_PATH_MAX];
    if (!fileio_real_path(loaded, source, sizeof(source))) {
        snprintf(result->path, sizeof(result->path), "%s", loaded);
        snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
        return false;
    }
    snprintf(result->path, sizeof(result->path), "%s", source);

    if (!fileio_is_writable(source)) {
        if (system_prefix == NULL || user_config == NULL || !starts_with(source, system_prefix)) {
            snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
            return false;
        }
        if (!use_user_config(user_config, source, sizeof(source), result))
            return false;
    }

    // Read the file as it is on disk now, so a change made meanwhile by hand survives
    size_t length = 0;
    char *text = fileio_read_all(source, &length);
    if (text == NULL) {
        snprintf(result->why, sizeof(result->why), "could not read %.300s: %s", source, fileio_last_error());
        return false;
    }
    IniDoc *doc = inidoc_parse(text, length);
    alloc_free(text);
    if (doc == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }
    if (!apply_edits(doc, edits, count, result)) {
        inidoc_free(doc);
        return false;
    }
    char *output = inidoc_serialize(doc, &length);
    inidoc_free(doc);
    if (output == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }

    // Keep the file as it was, then write the new one beside it and swap it in whole. A file that
    // cannot be looked up (out of memory, say) is never taken for one that is not there, which
    // would replace it without a backup.
    bool present = fileio_present(result->path);
    bool ok = true;
    if (!present && strcmp(fileio_last_error(), "not found") != 0) {
        snprintf(result->why, sizeof(result->why), "could not look for %.300s: %s", result->path, fileio_last_error());
        ok = false;
    }
    ok = ok && (!present || keep_backup(result));
    char temporary[CONFIG_SAVE_PATH_MAX + 4];
    snprintf(temporary, sizeof(temporary), "%s.tmp", result->path);
    if (ok && !fileio_write_all(temporary, output, length)) {
        snprintf(result->why, sizeof(result->why), "could not write %.300s: %s", temporary, fileio_last_error());
        ok = false;
    }
    else if (ok && !fileio_replace(temporary, result->path)) {
        snprintf(result->why, sizeof(result->why), "could not replace the file: %s", fileio_last_error());
        ok = false;
    }
    if (!ok)
        fileio_remove(temporary);
    else
        snprintf(result->warning, sizeof(result->warning), "%s", fileio_last_warning());
    alloc_free(output);
    return ok;
}
