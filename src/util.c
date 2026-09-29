#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <getopt.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "launcher.h"
#include <launcher_config.h>
#include "util.h"
#include "library.h"
#include "fileio.h"
#include "settings.h"
#include "debug.h"
#include "platform/platform.h"
#include <ini.h>

static void add_gamepad_control(const char *label, const char *cmd);
static bool parse_mode_setting(ModeSettingType type, const char *value, int *setting);
static Menu *create_menu(const char *menu_name, size_t *num_menus);
static bool gamepad_command_mapped(const char *cmd);
static bool gamepad_control_mapped(const char *label);
static void add_default_controls(const char *cmd, const char *const *labels, size_t count);
static void store_background_setting(SettingId id, const SettingValue *value);

extern Config          config;
extern GamepadControl  *gamepad_controls;
extern Hotkey          *hotkeys;
Menu                   *menu  = NULL;
Entry                  *entry = NULL;
static bool            columns_set = false; // Columns wins over its older name, MaxButtons

static const char *mode_settings[][5] = {
    {"Color", "Image", "Slideshow", "Transparent", NULL}, // Background Mode
    {"Blank", "None", "Quit", NULL, NULL},                // OnLaunch
    {"Truncate", "Shrink", "None", NULL, NULL},           // OversizeMode ("Truncated" is read too)
    {"Left", "Right", NULL, NULL, NULL},                  // Clock Alignment
    {"24hr", "12hr", "Auto", NULL, NULL},                 // Clock Format
    {"Big", "Little", "Auto", NULL, NULL}                 // Date Format
};

// A function to handle the arguments from the command line
void handle_arguments(int argc, char *argv[], char **config_file_path)
{
    // Parse command line arguments
    if (argc > 1) {
        bool version = false;
        bool help = false;
        int rc;
        const char *short_opts = "hvc:d";
        static const struct option long_opts[] = {
            { "help",         no_argument,       NULL, 'h' },
            { "version",      no_argument,       NULL, 'v' },
            { "config",       required_argument, NULL, 'c' },
            { "debug",        no_argument,       NULL, 'd' },
            { 0, 0, 0, 0 }
        };
    
        while ((rc = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
            switch (rc) {
                case 'h':
                    help = true;
                    break;

                case 'v':
                    version = true;
                    break;

                case 'c':
                    if (file_exists(optarg))
                        *config_file_path = strdup(optarg);
                    else
                        log_fatal("Config file '%s' not found", optarg);
                    break;

                case 'd':
                    config.debug = true;
                    break;
            }
        }

        // Check version, help flags
#ifdef __unix__
        if (version) {
            print_version(stdout);
            fputs("\n", stdout);
            print_compiler_info(stdout);
            quit(EXIT_SUCCESS);
        }
        if (help) {
            print_usage();
            quit(EXIT_SUCCESS);
        }
#endif
    }

    // Try to find config file if none is specified on the command line
    if (*config_file_path == NULL) {
#ifdef __unix__
        const char *prefixes[4];
        char home_config_buffer[MAX_PATH_CHARS + 1];
        prefixes[0] = CURRENT_DIRECTORY;
        prefixes[1] = config.exe_path;
        prefixes[2] = join_paths(home_config_buffer, sizeof(home_config_buffer), 3, getenv("HOME"), ".config", EXECUTABLE_TITLE);
        prefixes[3] = PATH_CONFIG_SYSTEM;
        *config_file_path = find_file(FILENAME_DEFAULT_CONFIG, 4, prefixes);
#else
        char *prefixes[2];
        prefixes[0] = CURRENT_DIRECTORY;
        prefixes[1] = config.exe_path;
        *config_file_path = find_file(FILENAME_DEFAULT_CONFIG, 2, prefixes);
#endif

        if (*config_file_path == NULL)
            log_fatal("No config file found");
    }
    log_debug("Config file found: %s", *config_file_path);
}

// A function to parse the config file and store the settings into the config struct
void parse_config_file(const char *config_file_path)
{
    FILE *file = fileio_open(config_file_path, "r");
    if (file == NULL)
        log_fatal("Could not open config file");
    int error = ini_parse_file(file, config_handler, NULL);
    fclose(file);
    
    if (error < 0)
        log_fatal("Could not parse config file");
}

// A function to store a [Background] setting read through the settings table
static void store_background_setting(SettingId id, const SettingValue *value)
{
    switch (id) {
        case SET_ID_BACKGROUND_MODE:
            config.background_mode = (ModeBackground) value->number;
            break;
        case SET_ID_BACKGROUND_COLOR:
            config.background_color.r = value->color.r;
            config.background_color.g = value->color.g;
            config.background_color.b = value->color.b;
            break;
        case SET_ID_BACKGROUND_IMAGE:
            free(config.background_image);
            config.background_image = strdup(value->text);
            break;
        case SET_ID_SLIDESHOW_DIRECTORY:
            free(config.slideshow_directory);
            config.slideshow_directory = strdup(value->text);
            break;
        case SET_ID_SLIDESHOW_DURATION:
            config.slideshow_image_duration = (Uint32) value->number * 1000;
            break;
        case SET_ID_SLIDESHOW_FADE:
            config.slideshow_transition_time = (Uint32) value->number;
            break;
        default:
            break;
    }
}

// A function to handle config file parsing
int config_handler(void *user, const char *section, const char *name, const char *value)
{
    UNUSED(user);

    if (MATCH(section, "General")) {
        if (MATCH(name, SETTING_DEFAULT_MENU))
            config.default_menu = strdup(value);
        else if (MATCH(name, SETTING_VSYNC))
            convert_bool(value, &config.vsync);
        else if(MATCH(name, SETTING_FPS_LIMIT)) {
            int fps = atoi(value);
            if (fps > MIN_FPS_LIMIT)
                config.fps_limit = fps;
        }
        else if (MATCH(name, SETTING_APPLICATION_TIMEOUT)) {
            Uint32 application_timeout = (Uint32) atoi(value);
            if (application_timeout >= MIN_APPLICATION_TIMEOUT
                && application_timeout <= MAX_APPLICATION_TIMEOUT) {
                config.application_timeout = 1000 * application_timeout;
            }
        }
        else if (MATCH(name, SETTING_ON_LAUNCH))
            parse_mode_setting(MODE_SETTING_ON_LAUNCH, value, (int*) &config.on_launch);
        else if (MATCH(name, SETTING_WRAP_ENTRIES))
            convert_bool(value, &config.wrap_entries);
        else if (MATCH(name, SETTING_RESET_ON_BACK))
            convert_bool(value, &config.reset_on_back);
        else if (MATCH(name, SETTING_MOUSE_SELECT))
            convert_bool(value, &config.mouse_select);
        else if (MATCH(name, SETTING_INHIBIT_OS_SCREENSAVER))
            convert_bool(value, &config.inhibit_os_screensaver);
        else if (MATCH(name, SETTING_STARTUP_CMD))
            config.startup_cmd = strdup(value);
        else if (MATCH(name, SETTING_QUIT_CMD))
            config.quit_cmd = strdup(value);
    }

    else if (MATCH(section, "Layout")) {
        SettingValue parsed;
        if (MATCH(name, SETTING_MAX_BUTTONS)) {
            if (!setting_parse(setting_def(SET_ID_LAYOUT_COLUMNS), value, &parsed))
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_MAX_BUTTONS, value);
            else if (!columns_set)
                config.max_buttons = (unsigned int) parsed.number;
        }
        else if (MATCH(name, SETTING_COLUMNS)) {
            if (setting_parse(setting_def(SET_ID_LAYOUT_COLUMNS), value, &parsed)) {
                config.max_buttons = (unsigned int) parsed.number;
                columns_set = true;
            }
            else
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_COLUMNS, value);
        }
        else if (MATCH(name, SETTING_ROWS)) {
            if (setting_parse(setting_def(SET_ID_LAYOUT_ROWS), value, &parsed))
                config.rows = (unsigned int) parsed.number;
            else
                log_error("Invalid %s value '%s' in [Layout], ignoring it", SETTING_ROWS, value);
        }
        else if (MATCH(name, SETTING_ICON_SIZE)) {
            if (setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), value, &parsed))
                config.icon_size = (Uint16) parsed.number;
            else
                log_error("Invalid %s value '%s' in [Layout] (use a whole number from %i to %i), ignoring it",
                    SETTING_ICON_SIZE, value, MIN_ICON_SIZE, MAX_ICON_SIZE);
        }
        else if (MATCH(name, SETTING_ICON_SPACING)) {
            if (is_percent(value))
                copy_string(config.icon_spacing_str, value, sizeof(config.icon_spacing_str));
            else {
                int icon_spacing = atoi(value);
                if (icon_spacing > 0 || MATCH(value, "0"))
                    config.icon_spacing = icon_spacing;
            }
        }
        else if (MATCH(name, SETTING_VCENTER)) {
            if (is_percent(value))
                copy_string(config.vcenter, value, sizeof(config.vcenter));
        }
    }

    else if (MATCH(section, "Background")) {
        SettingId id = SET_ID_COUNT;
        if (MATCH(name, SETTING_BACKGROUND_MODE))
            id = SET_ID_BACKGROUND_MODE;
        else if (MATCH(name, SETTING_BACKGROUND_COLOR))
            id = SET_ID_BACKGROUND_COLOR;
        else if (MATCH(name, SETTING_BACKGROUND_IMAGE))
            id = SET_ID_BACKGROUND_IMAGE;
        else if (MATCH(name, SETTING_SLIDESHOW_DIRECTORY))
            id = SET_ID_SLIDESHOW_DIRECTORY;
        else if (MATCH(name, SETTING_SLIDESHOW_IMAGE_DURATION))
            id = SET_ID_SLIDESHOW_DURATION;
        else if (MATCH(name, SETTING_SLIDESHOW_TRANSITION_TIME))
            id = SET_ID_SLIDESHOW_FADE;
        if (id != SET_ID_COUNT) {
            SettingValue parsed;
            if (setting_parse(setting_def(id), value, &parsed))
                store_background_setting(id, &parsed);
            else
                log_error("Invalid %s value '%s' in [Background], ignoring it", name, value);
        }
        else if (MATCH(name, SETTING_CHROMA_KEY_COLOR))
            hex_to_color(value, &config.chroma_key_color);
        else if (MATCH(name, SETTING_BACKGROUND_OVERLAY))
            convert_bool(value, &config.background_overlay);
        else if (MATCH(name, SETTING_BACKGROUND_OVERLAY_COLOR))
            hex_to_color(value, &config.background_overlay_color);
        else if (MATCH(name, SETTING_BACKGROUND_OVERLAY_OPACITY)) {
            if (is_percent(value))
                copy_string(config.background_overlay_opacity, value, sizeof(config.background_overlay_opacity));
        }
    }

    else if (MATCH(section, "Titles")) {
        if (MATCH(name, SETTING_TITLES_ENABLED))
            convert_bool(value, &config.titles_enabled);
        else if (MATCH(name, SETTING_TITLE_FONT)) {
            config.title_font_path = strdup(value);
            clean_path(config.title_font_path);
        }
        else if (MATCH(name, SETTING_TITLE_FONT_SIZE)) {
            SettingValue parsed;
            if (!setting_parse(setting_def(SET_ID_TITLE_SIZE), value, &parsed))
                log_error("Invalid %s value '%s' in [Titles] (use a percentage of the button such as 14%%, or a size such as 36), ignoring it",
                    SETTING_TITLE_FONT_SIZE, value);
            else if (parsed.percent)
                config.title_font_size_pct = parsed.number;
            else {
                config.title_font_size = (unsigned int) parsed.number;
                config.title_font_size_pct = 0;
            }
        }
        else if (MATCH(name, SETTING_TITLE_FONT_COLOR))
            hex_to_color(value, &config.title_font_color);
        else if (MATCH(name, SETTING_TITLE_OPACITY)) {
            if (is_percent(value))
                copy_string(config.title_opacity, value, sizeof(config.title_opacity));
        }
        else if (MATCH(name, SETTING_TITLE_SHADOWS))
            convert_bool(value, &config.title_shadows);
        else if (MATCH(name, SETTING_TITLE_SHADOW_COLOR))
            hex_to_color(value, &config.title_shadow_color);
        else if (MATCH(name, SETTING_TITLE_OVERSIZE_MODE)) {
            // "Truncated" was the only spelling the parser knew; the docs have always said "Truncate"
            if (MATCH(value, "Truncated"))
                config.title_oversize_mode = OVERSIZE_TRUNCATE;
            else
                parse_mode_setting(MODE_SETTING_OVERSIZE, value, (int*) &config.title_oversize_mode);
        }
        else if (MATCH(name, SETTING_TITLE_PADDING)) {
            int padding;
            bool percent;
            if (!layout_parse_title_padding(value, &padding, &percent))
                log_error("Invalid %s value '%s' in [Titles] (use a percentage of the button such as 8%%, or px such as 20), ignoring it",
                    SETTING_TITLE_PADDING, value);
            else if (percent) {
                config.title_padding_pct = padding;
                config.title_padding = 0;
            }
            else {
                config.title_padding = padding;
                config.title_padding_pct = 0;
            }
        }
    }

    else if (MATCH(section, "Highlight")) {
        if (MATCH(name, SETTING_HIGHLIGHT_ENABLED))
            convert_bool(value, &config.highlight);
        else if (MATCH(name, SETTING_HIGHLIGHT_FILL_COLOR))
            hex_to_color(value, &config.highlight_fill_color);
        else if (MATCH(name, SETTING_HIGHLIGHT_OUTLINE_COLOR))
            hex_to_color(value, &config.highlight_outline_color);
        else if (MATCH(name, SETTING_HIGHLIGHT_OUTLINE_SIZE)) {
            int highlight_outline_size = atoi(value);
            if (highlight_outline_size >= 0)
                config.highlight_outline_size = highlight_outline_size;
        }
        else if (MATCH(name, SETTING_HIGHLIGHT_CORNER_RADIUS)) {
            int rx = atoi(value);
            if (rx >= MIN_RX_SIZE && rx <= MAX_RX_SIZE)
                config.highlight_rx = (Uint16) rx;
        }
        else if (MATCH(name, SETTING_HIGHLIGHT_FILL_OPACITY)) {
            if (is_percent(value))
                copy_string(config.highlight_fill_opacity, value, sizeof(config.highlight_fill_opacity));
        }
        else if (MATCH(name, SETTING_HIGHLIGHT_OUTLINE_OPACITY)) {
            if (is_percent(value))
                copy_string(config.highlight_outline_opacity, value, sizeof(config.highlight_outline_opacity));
        }
        else if (MATCH(name, SETTING_HIGHLIGHT_VPADDING)) {
            int highlight_vpadding = atoi(value);
            if (highlight_vpadding > 0 || MATCH(value,"0"))
                config.highlight_vpadding = highlight_vpadding;
        }
        else if (MATCH(name, SETTING_HIGHLIGHT_HPADDING)) {
            int highlight_hpadding = atoi(value);
            if (highlight_hpadding > 0 || MATCH(value,"0"))
                config.highlight_hpadding = highlight_hpadding;
        }
    }

    else if (MATCH(section, "Scroll Indicators")) {
        if (MATCH(name,SETTING_SCROLL_INDICATORS))
            convert_bool(value, &config.scroll_indicators);
        else if (MATCH(name,SETTING_SCROLL_INDICATOR_FILL_COLOR))
            hex_to_color(value, &config.scroll_indicator_fill_color);
        else if (MATCH(name, SETTING_SCROLL_INDICATOR_OUTLINE_SIZE)) {
            int scroll_indicator_outline_size = atoi(value);
            if (scroll_indicator_outline_size >= 0)
                config.scroll_indicator_outline_size = scroll_indicator_outline_size;
        }
        else if (MATCH(name,SETTING_SCROLL_INDICATOR_OUTLINE_COLOR))
            hex_to_color(value, &config.scroll_indicator_outline_color);
        else if (MATCH(name,SETTING_SCROLL_INDICATOR_OPACITY)) {
            if (is_percent(value))
                copy_string(config.scroll_indicator_opacity, value, sizeof(config.scroll_indicator_opacity));
        }
    }

    else if (MATCH(section, "Clock")) {
        if (MATCH(name, SETTING_CLOCK_ENABLED))
            convert_bool(value, &config.clock_enabled);
        else if (MATCH(name, SETTING_CLOCK_SHOW_DATE))
            convert_bool(value, &config.clock_show_date);
        else if (MATCH(name, SETTING_CLOCK_ALIGNMENT))
            parse_mode_setting(MODE_SETTING_ALIGNMENT, value, (int*) &config.clock_alignment);
        else if (MATCH(name, SETTING_CLOCK_FONT)) {
            config.clock_font_path = strdup(value);
            clean_path(config.clock_font_path);
        }
        else if (MATCH(name, SETTING_CLOCK_MARGIN)) {
            if (is_percent(value))
                copy_string(config.clock_margin_str, value, sizeof(config.clock_margin_str));
            else {
                int clock_margin = atoi(value);
                if (clock_margin > 0 || MATCH(value,"0"))
                    config.clock_margin = clock_margin;
            }
        }
        else if (MATCH(name, SETTING_CLOCK_FONT_COLOR))
            hex_to_color(value, &config.clock_font_color);
        else if (MATCH(name, SETTING_CLOCK_SHADOW_COLOR))
            hex_to_color(value, &config.clock_shadow_color);
        else if (MATCH(name, SETTING_CLOCK_SHADOWS))
            convert_bool(value, &config.clock_shadows);
        else if (MATCH(name, SETTING_CLOCK_OPACITY)) {
            if (is_percent(value))
                copy_string(config.clock_opacity, value, sizeof(config.clock_opacity));
        }
        else if (MATCH(name, SETTING_CLOCK_FONT_SIZE)) {
            unsigned int font_size = (unsigned int) atoi(value);
            if (font_size)
                config.clock_font_size = font_size;
        }
        else if (MATCH(name, SETTING_CLOCK_TIME_FORMAT))
            parse_mode_setting(MODE_SETTING_TIME_FORMAT, value, (int*) &config.clock_time_format);
        else if (MATCH(name, SETTING_CLOCK_DATE_FORMAT))
            parse_mode_setting(MODE_SETTING_DATE_FORMAT, value, (int*) &config.clock_date_format);
        else if (MATCH(name, SETTING_CLOCK_INCLUDE_WEEKDAY))
            convert_bool(value, &config.clock_include_weekday);
    }

    else if (MATCH(section, "Screensaver")) {
        if (MATCH(name, SETTING_SCREENSAVER_ENABLED))
            convert_bool(value, &config.screensaver_enabled);
        else if (MATCH(name, SETTING_SCREENSAVER_IDLE_TIME)) {
            Uint32 screensaver_idle_time = (Uint32) atoi(value);
            if (screensaver_idle_time >= MIN_SCREENSAVER_IDLE_TIME &&
            screensaver_idle_time <= MAX_SCREENSAVER_IDLE_TIME)
                config.screensaver_idle_time = screensaver_idle_time*1000; // Convert to ms
        }
        else if (MATCH(name, SETTING_SCREENSAVER_INTENSITY)) {
            if (is_percent(value))
                copy_string(config.screensaver_intensity_str, value, sizeof(config.screensaver_intensity_str));
        }
        else if (MATCH(name, SETTING_SCREENSAVER_PAUSE_SLIDESHOW))
            convert_bool(value, &config.screensaver_pause_slideshow);
    }
    
    else if (MATCH(section, "Hotkeys")) {
        char *keycode = strtok((char*) value, ";");
        if (keycode != NULL) {
            char *cmd = strtok(NULL, "");
            if (cmd != NULL)
                add_hotkey(keycode, cmd);
        }
    }

    else if (MATCH(section, "Gamepad")) {
        if (MATCH(name, SETTING_GAMEPAD_ENABLED))
            convert_bool(value, &config.gamepad_enabled);
        else if (MATCH(name, SETTING_GAMEPAD_DEVICE))
            config.gamepad_device = atoi(value);
        else if (MATCH(name, SETTING_GAMEPAD_MAPPINGS_FILE)) {
            config.gamepad_mappings_file = strdup(value);
            clean_path(config.gamepad_mappings_file);
        }

        // Parse gamepad controls
        else
            add_gamepad_control(name, value);
    }

    // Parse menus/entries
    else {
        Entry *previous_entry = NULL;

        // Point the menu and entry cursors at this section's menu, adding it to the end of the list
        // when it is new. A section can appear twice with another menu between, so the cursors move
        // back to it, and to its last entry, rather than staying on the last menu read.
        Menu *section_menu = NULL;
        Menu *last_menu = NULL;
        for (Menu *tmp = config.first_menu; tmp != NULL; tmp = tmp->next) {
            if (section_menu == NULL && MATCH(tmp->name, section))
                section_menu = tmp;
            last_menu = tmp;
        }
        if (section_menu == NULL) {
            section_menu = create_menu(section, &config.num_menus);
            if (last_menu == NULL)
                config.first_menu = section_menu;
            else
                last_menu->next = section_menu;
        }
        if (section_menu != menu) {
            menu = section_menu;
            entry = menu->first_entry;
            while (entry != NULL && entry->next != NULL)
                entry = entry->next;
        }

        // Per-menu layout settings. They count only when the value is a number, so an
        // existing entry that happens to be keyed Rows, Columns or IconSize still parses.
        bool layout_key = MATCH(name, SETTING_ROWS) || MATCH(name, SETTING_COLUMNS) ||
                          MATCH(name, SETTING_ICON_SIZE);
        if (layout_key && strchr(value, ';') == NULL) {
            SettingId id = MATCH(name, SETTING_ROWS) ? SET_ID_MENU_ROWS
                         : MATCH(name, SETTING_COLUMNS) ? SET_ID_MENU_COLUMNS : SET_ID_MENU_ICON_SIZE;
            SettingValue parsed;
            if (!setting_parse(setting_def(id), value, &parsed))
                log_error("Invalid %s value '%s' in menu '%s', ignoring it", name, value, section);
            else if (id == SET_ID_MENU_ROWS)
                menu->overrides.rows = parsed.number;
            else if (id == SET_ID_MENU_COLUMNS)
                menu->overrides.columns = parsed.number;
            else
                menu->overrides.icon_cap = parsed.number;
            return 0;
        }
        if (layout_key)
            log_error("Menu '%s': '%s' holds an entry, so it is read as an entry", section, name);

        // Parse entry line for title, icon path, command
        char *string = (char*) value;
        char *token;
        char *delimiter = ";";
        token = strtok(string, delimiter);
        if (token == NULL) {
            log_error("Menu '%s': '%s' is empty, ignoring it", section, name);
            return 0;
        }
        if (token != NULL) {

            // Create first entry in the menu if none exists
            if (menu->first_entry == NULL) {
                menu->first_entry = calloc(1, sizeof(Entry));
                entry = menu->first_entry;
                entry->next = NULL;
            }

            // Add entry to the end of the linked list
            else {
                previous_entry = entry;
                entry = entry->next;
                entry = calloc(1, sizeof(Entry));
                previous_entry->next = entry;
                entry->next = NULL;
            }
            entry->title_offset = 0;
        }

        // Store data in entry struct
        int i;
        for (i = 0;i < 3 && token != NULL; i++) {
            if (i == 0)
                entry->title = strdup(token);
            else if (i == 1) {
                entry->icon_path = strdup(token);
                clean_path(entry->icon_path);
                delimiter = "";
            }
            else if (i == 2)
                entry->cmd = strdup(token);

            token = strtok(NULL, delimiter);
        }

        // Delete entry if parse failed to find 3 valid tokens, or its command is :select
        if (i != 3 || MATCH(":select", entry->cmd)) {
            if (i != 3)
                log_error("Menu '%s': '%s' needs a title, an icon and a command, ignoring it", section, name);
            else
                log_error("Menu '%s': '%s' uses :select, which only a hotkey or gamepad button can, ignoring it",
                    section, name);
            free(entry->title);
            free(entry->icon_path);
            free(entry->cmd);
            if (menu->num_entries == 0) {
                free(menu->first_entry);
                menu->first_entry = NULL;
            }
            else {
                free(entry);
                entry = previous_entry;
                entry->next = NULL;
            }
        }
        else {
            if (menu->num_entries == 0)
                entry->previous = NULL;
            else
                entry->previous = previous_entry;
            menu->num_entries++;
            entry->icon_selected_path = selected_path(entry->icon_path);
        }
    }
    return 0;
}

static bool parse_mode_setting(ModeSettingType type, const char *value, int *setting)
{
    const char **arr = mode_settings[type];
    for (int i = 0; arr[i] != NULL; i++) {
        if (MATCH(arr[i], value)) {
            *setting = i;
            return true;
        }
    }
    return false;
}

const char *get_mode_setting(int type, int value)
{
    return mode_settings[type][value];
}

// A function to determine if a string is a percent value
bool is_percent(const char *string)
{
    size_t length = strlen(string);
    if (length > 0 && length < PERCENT_MAX_CHARS && strchr(string, '%') == string + length - 1)
        return true;
    else
        return false;
}

// A function to remove quotation marks that enclose a path
// because SDL cannot handle them
void clean_path(char *path)
{
    size_t length = strlen(path);
    if (length >= 3 && path[0] == '"' && path[length - 1] == '"') {
        path[length - 1] = '\0';
        for (size_t i = 1; i <= length; i++)
            *(path + i - 1) = *(path + i);
    }    
}

// A function to get the selected path 
char *selected_path(const char *path)
{
    char buffer[MAX_PATH_CHARS + 1];
    size_t length = strlen(path);
    char *out = NULL;

    // Find file extension
    if (length + LEN(SELECTED_SUFFIX) + 1 > sizeof(buffer))
        return out;
    char *p = (char*) path + length - 1;
    while (*p != '.' && p > path)
        p--;
    if (p == path)
        return out;

    // Assemble path with suffix
    strcpy(buffer, path);
    buffer[p - path] = '\0';
    strcat(buffer, SELECTED_SUFFIX);
    strcat(buffer, p);

    if (file_exists(buffer))
        out = strdup(buffer);
    return out;
}

// A function to convert a hex-formatted string into a color struct
bool hex_to_color(const char *string, SDL_Color *color)
{
    if (*string != '#')
        return false;
    char *p = (char*) string + 1;

    // If strtoul returned 0, and the hex string wasn't 000..., then there was an error
    size_t length = strlen(p);
    Uint32 hex = (Uint32) strtoul(p, NULL, 16);
    if ((!hex && strcmp(p,"000000")) || (length != 6))
        return false;

    // Convert int to SDL_Color struct via bitwise logic
    color->r = (Uint8) (hex >> 16);
    color->g = (Uint8) ((hex & 0x0000ff00) >> 8);
    color->b = (Uint8) (hex & 0x000000ff);
    return true;
}

// A function to convert a string into a bool
bool convert_bool(const char *string, bool *setting)
{
    if (MATCH(string, "true") || MATCH(string, "True")) {
        *setting = true;
        return true;
    }
    else if (MATCH(string, "false") || MATCH(string, "False")) {
        *setting = false;
        return true;
    }
    return false;
}

// A function to copy a string into an existing buffer
void copy_string(char *dest, const char *string, size_t size)
{
    strncpy(dest, string, size);
    dest[size - 1] = '\0';
}

// A function to join paths together
char *join_paths(char *buffer, size_t bytes, int num_paths, ...)
{
    va_list list;
    char *arg;
    size_t length;
    va_start(list, num_paths);

    // Add each subdirectory to path
    for (int i = 0; i < num_paths && bytes > 1; i++) {
        arg = va_arg(list, char*);
        length = strlen(arg);
        if (length > bytes - 1)
            length = bytes - 1;
        if (i == 0) {
            copy_string(buffer, arg, bytes);
            bytes -= length;
            if (bytes == 1)
                break;
        }
        else {

            // Don't copy preceding slash if present
            if (*arg == '/' || *arg == '\\') {
                strncat(buffer, arg + 1, bytes - 1);
                bytes -= (length - 1);
            }
            else {
                strncat(buffer, arg, bytes - 1);
                bytes -= length;
            }
        }

        // Add trailing slash if not present, except last argument
        if ((i != num_paths - 1) && bytes > 1 && *(buffer + strlen(buffer) - 1) != '/' &&
        *(buffer + strlen(buffer) - 1) != '\\') {
            strncat(buffer, PATH_SEPARATOR, bytes - 1);
            bytes -= 1;
        }
    }
    va_end(list);
    return buffer;
}

// A function to find a file from a filename and list of path prefixes
char *find_file(const char *file, int num_prefixes, const char **prefixes)
{
    char buffer[MAX_PATH_CHARS + 1];
    for (int i = 0; i < num_prefixes; i++) {
        if (prefixes[i] != NULL) {
            join_paths(buffer, sizeof(buffer), 2, prefixes[i], file);
            if (file_exists(buffer)) {
                char *output;
                output = strdup(buffer);
                return output;
            }
        }
    }
    return NULL;
}

// A function to extract the Unicode code point from the first character in a UTF-8 string
Uint16 get_unicode_code_point(const char *p, int *bytes)
{
    Uint16 result;

    // 1 byte ASCII char
    if ((*p & 0x80) == 0) {
        result = (Uint16) *p;
        *bytes = 1;
    }

    // If byte is 110xxxxx, then it's a 2 byte char
    else if ((*p & 0xE0) == 0xC0) {
        Uint8 byte1 = *p & 0x1F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        result = (Uint16) ((byte1 << 6) + byte2);
        *bytes = 2;
    }

    // If byte is 1110xxxx, then it's a 3 byte char
    else if ((*p & 0xF0) == 0xE0) {
        Uint8 byte1 = *p & 0x0F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        Uint8 byte3 = *(p + 2) & 0x3F;
        result = (Uint16) ((byte1 << 12) + (byte2 << 6) + byte3);
        *bytes = 3;
    }
    else {
        result = 0;
        *bytes = 1;
    }
    return result;
}

// A function to generate an array of random indices
void random_array(int *array, int array_size)
{
    // Fill array with initial indices
    for (int i = 0; i < array_size; i++)
        array[i] = i;

    // Shuffle array indices randomly, see https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
    srand((unsigned int) time(NULL));
    int tmp;
    for (int i = 0; i < array_size - 1; i++) {
        int j = (rand() % (array_size - i)) + i;
        tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

// A function to add a hotkey to the linked list
void add_hotkey(const char *keycode, const char *cmd)
{
    if (keycode[0] != '#')
        return;
    char *p = (char*) keycode + 1;

    // Convert hex string to binary
    static Hotkey *current_hotkey = NULL;
    SDL_Keycode code = (SDL_Keycode) strtol(p, NULL, 16);

    // Check if exit hotkey for Windows
#ifdef _WIN32
    if (MATCH(cmd, SCMD_EXIT)) {
        set_exit_hotkey(code);
        return;
    }
#endif

    // Create first node if not initialized, else add to end of linked list
    if (current_hotkey == NULL) {
        hotkeys = malloc(sizeof(Hotkey));
        current_hotkey = hotkeys;
    }
    else {
        current_hotkey->next = malloc(sizeof(Hotkey));
        current_hotkey = current_hotkey->next;
    }
    current_hotkey->keycode = code;
    current_hotkey->cmd = strdup(cmd);
    current_hotkey->next = NULL;
}

// A function to add a gamepad control to the linked list
static void add_gamepad_control(const char *label, const char *cmd)
{
    if (cmd[0] == '\0')
        return;
    
    // Table of gamepad info
    static const struct gamepad_info info[] = {
        {SETTING_GAMEPAD_LSTICK_XM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_LEFTX},
        {SETTING_GAMEPAD_LSTICK_XP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_LEFTX},
        {SETTING_GAMEPAD_LSTICK_YM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_LEFTY},
        {SETTING_GAMEPAD_LSTICK_YP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_LEFTY},
        {SETTING_GAMEPAD_RSTICK_XM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_RIGHTX},
        {SETTING_GAMEPAD_RSTICK_XP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_RIGHTX},
        {SETTING_GAMEPAD_RSTICK_YM,             TYPE_AXIS_NEG, SDL_CONTROLLER_AXIS_RIGHTY},
        {SETTING_GAMEPAD_RSTICK_YP,             TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_RIGHTY},
        {SETTING_GAMEPAD_LTRIGGER,              TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_TRIGGERLEFT},
        {SETTING_GAMEPAD_RTRIGGER,              TYPE_AXIS_POS, SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
        {SETTING_GAMEPAD_BUTTON_A,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_A},
        {SETTING_GAMEPAD_BUTTON_B,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_B},
        {SETTING_GAMEPAD_BUTTON_X,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_X},
        {SETTING_GAMEPAD_BUTTON_Y,              TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_Y},
        {SETTING_GAMEPAD_BUTTON_BACK,           TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_BACK},
        {SETTING_GAMEPAD_BUTTON_GUIDE,          TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_GUIDE},
        {SETTING_GAMEPAD_BUTTON_START,          TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_START},
        {SETTING_GAMEPAD_BUTTON_LEFT_STICK,     TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_LEFTSTICK},
        {SETTING_GAMEPAD_BUTTON_RIGHT_STICK,    TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_RIGHTSTICK},
        {SETTING_GAMEPAD_BUTTON_LEFT_SHOULDER,  TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
        {SETTING_GAMEPAD_BUTTON_RIGHT_SHOULDER, TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
        {SETTING_GAMEPAD_BUTTON_DPAD_UP,        TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_UP},
        {SETTING_GAMEPAD_BUTTON_DPAD_DOWN,      TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_DOWN},
        {SETTING_GAMEPAD_BUTTON_DPAD_LEFT,      TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_LEFT},
        {SETTING_GAMEPAD_BUTTON_DPAD_RIGHT,     TYPE_BUTTON,   SDL_CONTROLLER_BUTTON_DPAD_RIGHT}
    };

    // Find correct gamepad info for label, return if none found
    size_t i;
    for (i = 0; i < sizeof(info) / sizeof(info[0]); i++) {
        if (MATCH(info[i].label, label))
            break;
    }
    if (i == sizeof(info) / sizeof(info[0]))
        return;

    // Begin the linked list if none exists
    static GamepadControl *current_gamepad_control = NULL;
    if (current_gamepad_control == NULL) {
        gamepad_controls = malloc(sizeof(GamepadControl));
        current_gamepad_control = gamepad_controls;
    }

    // Add another node to the linked list
    else {
        current_gamepad_control->next = malloc(sizeof(GamepadControl));
        current_gamepad_control = current_gamepad_control->next;
    }

    // Copy the parameters in the struct
    *current_gamepad_control = (GamepadControl) { 
        .type     = info[i].type,
        .index    = info[i].index,
        .label    = info[i].label,
        .repeat   = 0,
        .next     = NULL
    };
    current_gamepad_control->cmd = strdup(cmd);
}

// A function to check whether any gamepad control runs a command
static bool gamepad_command_mapped(const char *cmd)
{
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        if (MATCH(i->cmd, cmd))
            return true;
    }
    return false;
}

// A function to check whether a gamepad control is mapped to anything
static bool gamepad_control_mapped(const char *label)
{
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        if (MATCH(i->label, label))
            return true;
    }
    return false;
}

// A function to map a command to each listed control the config leaves free, unless the
// config already maps the command somewhere itself
static void add_default_controls(const char *cmd, const char *const *labels, size_t count)
{
    if (gamepad_command_mapped(cmd))
        return;
    for (size_t i = 0; i < count; i++) {
        if (!gamepad_control_mapped(labels[i]))
            add_gamepad_control(labels[i], cmd);
    }
}

// A function to give Up, Down and :settings default gamepad controls. Configs written before
// grids existed map nothing to :up or :down, and a grid is unusable without them; and any config
// written before the settings screen needs a way to open it.
void add_default_gamepad_controls()
{
    static const char *const up[] = { SETTING_GAMEPAD_BUTTON_DPAD_UP, SETTING_GAMEPAD_LSTICK_YM };
    static const char *const down[] = { SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_LSTICK_YP };
    static const char *const settings[] = { SETTING_GAMEPAD_BUTTON_START };
    add_default_controls(SCMD_UP, up, sizeof(up) / sizeof(up[0]));
    add_default_controls(SCMD_DOWN, down, sizeof(down) / sizeof(down[0]));
    add_default_controls(SCMD_SETTINGS, settings, sizeof(settings) / sizeof(settings[0]));
}

// A function to convert a string percent setting to an int value
void convert_percent_to_int(char *string, int *result, int max_value)
{
    size_t length = strlen(string);
    char tmp[PERCENT_MAX_CHARS];
    copy_string(tmp, string, sizeof(tmp));
    tmp[length - 1] = '\0';
    float percent = (float) atof(tmp);
    if (percent >= 0.0F && percent <= 100.0F)
        *result = (int) ((percent / 100.0F) * (float) max_value);
}

// A function to make sure all settings are in their correct range
void validate_settings(Geometry *geo)
{
    if (!config.titles_enabled) {
        config.title_padding = 0;
        config.title_padding_pct = 0;
    }

    // Convert % opacity settings to 0-255
    if (config.title_opacity[0] != '\0') {
        int title_opacity = INVALID_PERCENT_VALUE;
        convert_percent_to_int(config.title_opacity, &title_opacity, 255);
        if (title_opacity != INVALID_PERCENT_VALUE)
            config.title_font_color.a = (Uint8) title_opacity;
    }
    if (config.background_overlay_opacity[0] != '\0') {
        int background_overlay_opacity = INVALID_PERCENT_VALUE;
        convert_percent_to_int(config.background_overlay_opacity, &background_overlay_opacity, 255);
        if (background_overlay_opacity != INVALID_PERCENT_VALUE)
            config.background_overlay_color.a = (Uint8) background_overlay_opacity;
    }

    if (config.highlight_fill_opacity[0] != '\0') {
        int highlight_fill_opacity = INVALID_PERCENT_VALUE;
        convert_percent_to_int(config.highlight_fill_opacity, &highlight_fill_opacity, 255);
        if (highlight_fill_opacity != INVALID_PERCENT_VALUE)
            config.highlight_fill_color.a = (Uint8) highlight_fill_opacity;
    }
    if (config.highlight_outline_opacity[0] != '\0') {
        int highlight_outline_opacity = INVALID_PERCENT_VALUE;
        convert_percent_to_int(config.highlight_outline_opacity, &highlight_outline_opacity, 255);
        if (highlight_outline_opacity != INVALID_PERCENT_VALUE)
            config.highlight_outline_color.a = (Uint8) highlight_outline_opacity;
    }
    if (config.scroll_indicator_opacity[0] != '\0') {
        int scroll_indicator_opacity = INVALID_PERCENT_VALUE;
        convert_percent_to_int(config.scroll_indicator_opacity, &scroll_indicator_opacity, 255);
        if (scroll_indicator_opacity != INVALID_PERCENT_VALUE) {
            config.scroll_indicator_fill_color.a = (Uint8) scroll_indicator_opacity;
            config.scroll_indicator_outline_color.a = config.scroll_indicator_fill_color.a;
        }
    }
    if (config.clock_opacity[0] != '\0') {
        int clock_opacity = INVALID_PERCENT_VALUE;
        convert_percent_to_int(config.clock_opacity, &clock_opacity, 255);
        if (clock_opacity != INVALID_PERCENT_VALUE)
            config.clock_font_color.a = (Uint8) clock_opacity;
    }

    // Set default IconSpacing if none is in the config file
    if (config.icon_spacing < 0) {
        int icon_spacing = INVALID_PERCENT_VALUE;
        if (config.icon_spacing_str[0] != '\0')
            convert_percent_to_int(config.icon_spacing_str, &icon_spacing, geo->screen_width);
        if (icon_spacing == INVALID_PERCENT_VALUE)
            convert_percent_to_int(DEFAULT_ICON_SPACING, &icon_spacing, geo->screen_width);
        config.icon_spacing = icon_spacing;
    }

    // A gap wider than the screen leaves no room for a single button
    if (config.icon_spacing > geo->screen_width) {
        log_error("%s %i px is wider than the screen, using %i px",
            SETTING_ICON_SPACING, config.icon_spacing, geo->screen_width);
        config.icon_spacing = geo->screen_width;
    }

    // Convert clock margin setting and check limits
    if (config.clock_margin < 0) {
        int clock_margin = INVALID_PERCENT_VALUE;
        if (config.clock_margin_str[0] != '\0')
            convert_percent_to_int(config.clock_margin_str, &clock_margin, geo->screen_height);
        if (clock_margin == INVALID_PERCENT_VALUE)
            convert_percent_to_int(DEFAULT_CLOCK_MARGIN, &clock_margin, geo->screen_height);
        config.clock_margin = clock_margin;
    }
    int clock_margin_limit = (int) ((float) geo->screen_height*MAX_CLOCK_MARGIN);
    if (config.clock_margin > clock_margin_limit)
        config.clock_margin = clock_margin_limit;

    // Reduce highlight hpadding to prevent overlaps
    if (config.highlight_hpadding > (config.icon_spacing / 2))
        config.highlight_hpadding = config.icon_spacing / 2;

    // Convert the vertical centre setting to px and check its limits
    int vcenter = INVALID_PERCENT_VALUE;
    float f_screen_height = (float) geo->screen_height;
    int lower_limit = (int) (MIN_VCENTER*f_screen_height);
    int upper_limit = (int) (MAX_VCENTER*f_screen_height);
    if (config.vcenter[0] != '\0')
        convert_percent_to_int(config.vcenter, &vcenter, geo->screen_height);
    if (vcenter == INVALID_PERCENT_VALUE)
        convert_percent_to_int(DEFAULT_VCENTER, &vcenter, geo->screen_height);
    if (vcenter < lower_limit)
        vcenter = lower_limit;
    else if (vcenter > upper_limit)
        vcenter = upper_limit;
    geo->vcenter = vcenter;

    // Max highlight outline
    int max_highlight_outline_size = (config.highlight_hpadding < config.highlight_vpadding)
                                     ? config.highlight_hpadding : config.highlight_vpadding;
    if (max_highlight_outline_size < 0)
        max_highlight_outline_size = 0;
    if (config.highlight_outline_size > max_highlight_outline_size)
        config.highlight_outline_size = max_highlight_outline_size;

    // Max scroll indicator outline
    int max_scroll_indicator_outline_size = (int) ((float) geo->screen_height * MAX_SCROLL_INDICATOR_OUTLINE);
    if (config.scroll_indicator_outline_size > max_scroll_indicator_outline_size)
        config.scroll_indicator_outline_size = max_scroll_indicator_outline_size;

    // Don't allow rounded rectangle with outline due to Nanosvg bug
    if (config.highlight_rx && config.highlight_outline_size)
        config.highlight_rx = 0;
}

// A function to retreive menu struct from the linked list via the menu name
Menu *get_menu(const char *menu_name)
{
    for (Menu *menu = config.first_menu; menu != NULL; menu = menu->next) {
        if (MATCH(menu_name, menu->name))
            return menu;
    }
    log_error("Menu '%s' not found in config file", menu_name);
    return NULL;
}

// A function to allocate memory to and initialize a menu struct
Menu *create_menu(const char *menu_name, size_t *num_menus)
{
    Menu *menu = malloc(sizeof(Menu));
    *menu = (Menu) {
        .first_entry = NULL,
        .items = NULL,
        .next = NULL,
        .back = NULL,
        .num_entries = 0,
        .overrides = { 0, 0, 0 },
        .position = { 0, 0 },
        .rendered_size = 0
    };
    menu->name = strdup(menu_name);
    (*num_menus)++;

    return menu;
}

// A function to give every menu an array of its entries by index, for the layout maths
void build_menu_items()
{
    for (Menu *m = config.first_menu; m != NULL; m = m->next) {
        if (m->num_entries == 0)
            continue;
        m->items = malloc(m->num_entries * sizeof(Entry*));
        Entry *e = m->first_entry;
        for (unsigned int i = 0; i < m->num_entries; i++) {
            m->items[i] = e;
            e = e->next;
        }
    }
}

// A function to pass the icon library's warnings to the log
static void library_warning(const char *message)
{
    log_error("%s", message);
}

// A function to point every entry that names a library icon at its file. It also rescues a missing
// file that the library can stand in for (see library_rescue): a path to one of the seven icons older
// versions shipped, or a name typed with capitals or stray spaces. Both use the library icon and log a note.
void resolve_library_icons(void)
{
    library_set_warn(library_warning);
    char exe_library[MAX_PATH_CHARS + 1];
    const char *roots[2];
    roots[0] = config.exe_path != NULL
               ? join_paths(exe_library, sizeof(exe_library), 4, config.exe_path, PATH_ASSETS_EXE, PATH_ICONS_EXE, PATH_LIBRARY_EXE)
               : NULL;
#ifdef __unix__
    roots[1] = PATH_LIBRARY_SYSTEM;
#else
    roots[1] = PATH_LIBRARY_RELATIVE;
#endif
    const char *root = NULL;
    char manifest[MAX_PATH_CHARS + 1];
    for (int i = 0; i < 2 && root == NULL; i++) {
        if (roots[i] != NULL && file_exists(join_paths(manifest, sizeof(manifest), 2, roots[i], LIBRARY_MANIFEST)))
            root = roots[i];
    }
    bool loaded = false;
    if (root == NULL)
        log_error("Icon library not found; entries that name a library icon will have no image");
    else {
        int count = library_load(root);
        loaded = count >= 0;
        if (loaded)
            log_debug("Icon library: %s (%i icons)", root, count);
        else
            log_error("Icon library at %s could not be read; entries that name a library icon will have no image", root);
    }

    for (Menu *m = config.first_menu; m != NULL; m = m->next) {
        for (Entry *e = m->first_entry; e != NULL; e = e->next) {
            const char *path = NULL;
            if (e->icon_path == NULL)
                continue;
            if (library_is_name(e->icon_path)) {
                path = library_lookup(e->icon_path);
                // Without a library every name misses; the line above already says why, once
                if (path == NULL && loaded) {
                    log_error("Entry '%s' in menu '%s': no library icon named '%s'", e->title, m->name, e->icon_path);
                    path = library_lookup(LIBRARY_FALLBACK_ICON);
                }
            }
            else if (!file_exists(e->icon_path)) {
                const char *name = NULL;
                path = library_rescue(e->icon_path, &name);
                if (path != NULL)
                    log_error("Entry '%s' in menu '%s': '%s' is not a file; using the library icon '%s' "
                        "(write '%s' in the config to use it directly)", e->title, m->name, e->icon_path, name, name);
            }
            if (path != NULL) {
                free(e->icon_path);
                e->icon_path = strdup(path);
            }
        }
    }
}

// A function to dynamically allocate a buffer for and copy a formatted string
void sprintf_alloc(char **buffer, const char *format, ...)
{
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    
    size_t length = (size_t) vsnprintf(NULL, 0, format, args1);
    if (length) {
        *buffer = malloc(length + 1);
        vsnprintf(*buffer, length + 1, format, args2);
    }
    va_end(args1);
    va_end(args2);
}