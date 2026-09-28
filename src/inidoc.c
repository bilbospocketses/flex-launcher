#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inidoc.h"

typedef enum {
    LINE_OTHER,          // Blank, a comment, or a line inih cannot read
    LINE_SECTION,
    LINE_KEY,
    LINE_CONTINUATION    // Indented after a key: inih reads it as more of that key's value
} LineKind;

typedef struct {
    char *text;          // The line as written, without its line ending
    const char *eol;     // "\n", "\r\n", or "" for a last line with no ending
    LineKind kind;
    char *name;          // SECTION: the section's name; KEY: the key's name, as inih reads them
    char *value;         // KEY: the value, as inih reads it
    size_t value_start;  // KEY: where the value starts in `text`; just after the separator when empty
    size_t value_length; // KEY: how many bytes of `text` the value spans
    int section;         // Index of the SECTION line this one sits under; -1 before the first
} Line;

struct IniDoc {
    Line *lines;
    int count;
    int capacity;
    bool bom;            // The file started with a UTF-8 byte order mark
    const char *eol;     // The ending new lines get: the file's first one, else "\n"
};

static const char *const EOL_LF = "\n";
static const char *const EOL_CRLF = "\r\n";
static const char *const EOL_NONE = "";

// A function to tell whitespace as inih does (isspace in the C locale)
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// A function to copy part of a string into a new one
static char *copy_span(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, start, length);
        copy[length] = '\0';
    }
    return copy;
}

// A function to find, as inih's find_chars_or_comment() does, the first of `chars` from `start`,
// or a ';' that follows whitespace (a trailing comment), or the end of the text
static size_t find_chars_or_comment(const char *text, size_t start, const char *chars)
{
    bool was_space = false;
    size_t i = start;
    while (text[i] != '\0' && (chars == NULL || strchr(chars, text[i]) == NULL) && !(was_space && text[i] == ';')) {
        was_space = is_space(text[i]);
        i++;
    }
    return i;
}

// A function to name the section a line sits under; lines before the first header are in ""
static const char *section_name(const IniDoc *doc, int section)
{
    return section < 0 ? "" : doc->lines[section].name;
}

// A function to read every line the way inih's ini_parse_stream() does. It runs again after
// each edit: config files are small, and reading them whole keeps every index honest.
static void classify_all(IniDoc *doc)
{
    int section = -1;
    bool after_key = false;   // inih's prev_name: a key has been read since the last header
    for (int i = 0; i < doc->count; i++) {
        Line *line = &doc->lines[i];
        free(line->name);
        free(line->value);
        line->name = NULL;
        line->value = NULL;
        line->kind = LINE_OTHER;
        line->section = section;
        const char *text = line->text;
        size_t start = 0;
        while (is_space(text[start]))
            start++;
        if (text[start] == '\0' || text[start] == ';' || text[start] == '#')
            continue;
        if (after_key && start > 0) {
            line->kind = LINE_CONTINUATION;
            continue;
        }
        if (text[start] == '[') {
            size_t end = find_chars_or_comment(text, start + 1, "]");
            if (text[end] == ']') {
                line->kind = LINE_SECTION;
                line->name = copy_span(text + start + 1, end - start - 1);
                line->section = i;
                section = i;
                after_key = false;
            }
            continue;
        }
        size_t end = find_chars_or_comment(text, start, "=:");
        if (text[end] != '=' && text[end] != ':')
            continue;
        size_t name_end = end;
        while (name_end > start && is_space(text[name_end - 1]))
            name_end--;
        size_t value_begin = end + 1;
        size_t value_end = find_chars_or_comment(text, value_begin, NULL);
        size_t first = value_begin;
        while (first < value_end && is_space(text[first]))
            first++;
        size_t last = value_end;
        while (last > first && is_space(text[last - 1]))
            last--;
        line->kind = LINE_KEY;
        line->name = copy_span(text + start, name_end - start);
        line->value = copy_span(text + first, last - first);
        line->value_start = first == last ? value_begin : first;
        line->value_length = last - first;
        after_key = true;
    }
}

// A function to make room for one more line
static bool grow(IniDoc *doc)
{
    if (doc->count < doc->capacity)
        return true;
    int capacity = doc->capacity ? doc->capacity * 2 : 16;
    Line *lines = realloc(doc->lines, (size_t) capacity * sizeof(Line));
    if (lines == NULL)
        return false;
    doc->lines = lines;
    doc->capacity = capacity;
    return true;
}

// A function to add a line read from the file
static bool append_raw(IniDoc *doc, const char *text, size_t length, const char *eol)
{
    if (!grow(doc))
        return false;
    Line *line = &doc->lines[doc->count];
    memset(line, 0, sizeof(*line));
    line->text = copy_span(text, length);
    line->eol = eol;
    if (line->text == NULL)
        return false;
    doc->count++;
    return true;
}

// A function to insert a new line at an index. A new last line takes over "no line ending" from
// the old last line, so a file that did not end with a newline still does not.
static bool insert_line(IniDoc *doc, int at, const char *text)
{
    if (!grow(doc))
        return false;
    char *copy = copy_span(text, strlen(text));
    if (copy == NULL)
        return false;
    memmove(&doc->lines[at + 1], &doc->lines[at], (size_t) (doc->count - at) * sizeof(Line));
    Line *line = &doc->lines[at];
    memset(line, 0, sizeof(*line));
    line->text = copy;
    line->eol = doc->eol;
    doc->count++;
    if (at == doc->count - 1 && at > 0 && doc->lines[at - 1].eol[0] == '\0') {
        doc->lines[at - 1].eol = doc->eol;
        line->eol = EOL_NONE;
    }
    classify_all(doc);
    return true;
}

// A function to delete a line. When the deleted line was the last and had no line ending, the
// new last line loses its ending too.
static void remove_line(IniDoc *doc, int at)
{
    bool was_open_end = at == doc->count - 1 && doc->lines[at].eol[0] == '\0';
    free(doc->lines[at].text);
    free(doc->lines[at].name);
    free(doc->lines[at].value);
    memmove(&doc->lines[at], &doc->lines[at + 1], (size_t) (doc->count - at - 1) * sizeof(Line));
    doc->count--;
    if (was_open_end && doc->count > 0)
        doc->lines[doc->count - 1].eol = EOL_NONE;
    classify_all(doc);
}

// A function to find the last line setting a key in a section: the one the parser ends up with
static int find_key(const IniDoc *doc, const char *section, const char *key)
{
    for (int i = doc->count - 1; i >= 0; i--) {
        const Line *line = &doc->lines[i];
        if (line->kind == LINE_KEY && strcmp(line->name, key) == 0 && strcmp(section_name(doc, line->section), section) == 0)
            return i;
    }
    return -1;
}

// A function to find the last header of a section
static int find_header(const IniDoc *doc, const char *section)
{
    for (int i = doc->count - 1; i >= 0; i--) {
        if (doc->lines[i].kind == LINE_SECTION && strcmp(doc->lines[i].name, section) == 0)
            return i;
    }
    return -1;
}

// A function to find a key's last line: the key itself, or its last continuation line
static int end_of_key(const IniDoc *doc, int i)
{
    while (i + 1 < doc->count && doc->lines[i + 1].kind == LINE_CONTINUATION)
        i++;
    return i;
}

// A function to count the keys, to prove an insert made exactly one more
static int count_keys(const IniDoc *doc)
{
    int keys = 0;
    for (int i = 0; i < doc->count; i++) {
        if (doc->lines[i].kind == LINE_KEY)
            keys++;
    }
    return keys;
}

// A function to read a file's text into lines
IniDoc *inidoc_parse(const char *text, size_t length)
{
    IniDoc *doc = calloc(1, sizeof(IniDoc));
    if (doc == NULL)
        return NULL;
    doc->eol = EOL_LF;
    bool eol_found = false;
    size_t i = 0;
    if (length >= 3 && (unsigned char) text[0] == 0xEF && (unsigned char) text[1] == 0xBB && (unsigned char) text[2] == 0xBF) {
        doc->bom = true;
        i = 3;
    }
    while (i < length) {
        size_t end = i;
        while (end < length && text[end] != '\n')
            end++;
        const char *eol = EOL_NONE;
        size_t text_end = end;
        if (end < length) {
            eol = EOL_LF;
            if (end > i && text[end - 1] == '\r') {
                eol = EOL_CRLF;
                text_end = end - 1;
            }
            if (!eol_found) {
                doc->eol = eol;
                eol_found = true;
            }
        }
        if (!append_raw(doc, text + i, text_end - i, eol)) {
            inidoc_free(doc);
            return NULL;
        }
        i = end < length ? end + 1 : end;
    }
    classify_all(doc);
    return doc;
}

// A function to write the lines back out as one text
char *inidoc_serialize(const IniDoc *doc, size_t *length)
{
    size_t total = doc->bom ? 3 : 0;
    for (int i = 0; i < doc->count; i++)
        total += strlen(doc->lines[i].text) + strlen(doc->lines[i].eol);
    char *out = malloc(total + 1);
    if (out == NULL)
        return NULL;
    size_t used = 0;
    if (doc->bom) {
        memcpy(out, "\xEF\xBB\xBF", 3);
        used = 3;
    }
    for (int i = 0; i < doc->count; i++) {
        size_t text_length = strlen(doc->lines[i].text);
        size_t eol_length = strlen(doc->lines[i].eol);
        memcpy(out + used, doc->lines[i].text, text_length);
        used += text_length;
        memcpy(out + used, doc->lines[i].eol, eol_length);
        used += eol_length;
    }
    out[used] = '\0';
    if (length != NULL)
        *length = used;
    return out;
}

// A function to read a key's value, as the parser would get it
const char *inidoc_get(const IniDoc *doc, const char *section, const char *key)
{
    int i = find_key(doc, section, key);
    return i < 0 ? NULL : doc->lines[i].value;
}

// A function to say why `key=value` cannot be written so that inih reads it back unchanged,
// or NULL when it can
const char *inidoc_check(const char *key, const char *value)
{
    size_t length = strlen(value);
    if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL)
        return "it contains a line break";
    if (length > 0 && (is_space(value[0]) || is_space(value[length - 1])))
        return "it starts or ends with a space, which config.ini would drop";
    if (value[0] == ';')
        return "it starts with a semicolon, which config.ini would read as a comment";
    for (size_t i = 1; i < length; i++) {
        if (value[i] == ';' && is_space(value[i - 1]))
            return "it has a semicolon after a space, which config.ini would read as a comment";
    }
    if (strlen(key) + 1 + length > INIDOC_MAX_LINE)
        return "it is too long for one line of config.ini (199 bytes at most)";
    return NULL;
}

// A function to put a new value into an existing key's line, keeping everything around it
static bool replace_value(IniDoc *doc, int i, const char *section, const char *key, const char *value)
{
    Line *line = &doc->lines[i];
    size_t text_length = strlen(line->text);
    size_t head = line->value_start;
    size_t tail = line->value_start + line->value_length;
    size_t value_length = strlen(value);
    size_t new_length = head + value_length + (text_length - tail);
    if (new_length > INIDOC_MAX_LINE)
        return false;
    char *text = malloc(new_length + 1);
    if (text == NULL)
        return false;
    memcpy(text, line->text, head);
    memcpy(text + head, value, value_length);
    memcpy(text + head + value_length, line->text + tail, text_length - tail + 1);
    char *old = line->text;
    line->text = text;
    classify_all(doc);

    // Prove the line reads back as intended; if not, put the old line back
    if (find_key(doc, section, key) != i || strcmp(doc->lines[i].value, value) != 0) {
        doc->lines[i].text = old;
        free(text);
        classify_all(doc);
        return false;
    }
    free(old);
    return true;
}

// A function to set a key's value: in its line when it exists, else on a new line placed as asked
bool inidoc_set(IniDoc *doc, const char *section, const char *key, const char *value, IniDocPlacement placement)
{
    if (inidoc_check(key, value) != NULL)
        return false;
    int existing = find_key(doc, section, key);
    if (existing >= 0)
        return replace_value(doc, existing, section, key, value);

    size_t size = strlen(key) + strlen(value) + 2;
    char *text = malloc(size);
    if (text == NULL)
        return false;
    snprintf(text, size, "%s=%s", key, value);
    int keys_before = count_keys(doc);
    int header = find_header(doc, section);
    int at;
    if (header < 0) {
        // A missing section goes at the end, after a blank line
        size_t header_size = strlen(section) + 3;
        char *header_text = malloc(header_size);
        bool ok = header_text != NULL;
        if (ok) {
            snprintf(header_text, header_size, "[%s]", section);
            if (doc->count > 0 && doc->lines[doc->count - 1].text[0] != '\0')
                ok = insert_line(doc, doc->count, "");
            ok = ok && insert_line(doc, doc->count, header_text);
        }
        free(header_text);
        if (!ok) {
            free(text);
            return false;
        }
        at = doc->count;
    }
    else {
        // Under the header, unless the line there is indented: it would become the new key's continuation
        at = header + 1;
        const char *next = at < doc->count ? doc->lines[at].text : "";
        if (placement == INIDOC_AFTER_LAST_KEY || is_space(next[0])) {
            for (int i = 0; i < doc->count; i++) {
                const Line *line = &doc->lines[i];
                if (line->kind == LINE_KEY && strcmp(section_name(doc, line->section), section) == 0)
                    at = end_of_key(doc, i) + 1;
            }
        }
    }
    bool ok = insert_line(doc, at, text);
    free(text);

    // Prove the file now holds exactly one more key, reading as intended
    const char *read_back = ok ? inidoc_get(doc, section, key) : NULL;
    if (ok && (count_keys(doc) != keys_before + 1 || read_back == NULL || strcmp(read_back, value) != 0)) {
        remove_line(doc, at);
        return false;
    }
    return ok;
}

// A function to remove every line that sets a key in a section, with any continuation lines
bool inidoc_remove(IniDoc *doc, const char *section, const char *key)
{
    bool removed = false;
    int i;
    while ((i = find_key(doc, section, key)) >= 0) {
        for (int k = end_of_key(doc, i); k >= i; k--)
            remove_line(doc, k);
        removed = true;
    }
    return removed;
}

// A function to free a document
void inidoc_free(IniDoc *doc)
{
    if (doc == NULL)
        return;
    for (int i = 0; i < doc->count; i++) {
        free(doc->lines[i].text);
        free(doc->lines[i].name);
        free(doc->lines[i].value);
    }
    free(doc->lines);
    free(doc);
}
