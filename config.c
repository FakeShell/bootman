/**
 * Copyright 2021 Johannes Marbach
 *
 * This file is part of bootman, hereafter referred to as the program.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


#include "config.h"

#include "lvgl/lvgl.h"

#include <ini.h>
#include <stdlib.h>


/**
 * Static prototypes
 */

/**
 * Initialise a config options struct with default values.
 * 
 * @param opts pointer to the options struct
 */
static void init_opts(config_opts *opts);

/**
 * Parse options from a configuration file.
 * 
 * @param path path to configuration file
 * @param opts pointer for writing the parsed options into
 */
static void parse_file(const char *path, config_opts *opts);

/**
 * Handle parsing events from INIH.
 *
 * @param user_data pointer to user data
 * @param section current section name
 * @param key option key
 * @param value option value
 * @return 0 on error, non-0 otherwise
 */
static int parsing_handler(void* user_data, const char* section, const char* key, const char* value);

/**
 * Attempt to parse a boolean value.
 *
 * @param value string to parse
 * @param result pointer to write result into if parsing is successful
 * @return true on success, false otherwise
 */
static bool parse_bool(const char *value, bool *result);


/**
 * Static functions
 */

static void init_opts(config_opts *opts) {
    opts->general.animations = false;
    opts->general.backend = backends_backends[0] == NULL ? BACKENDS_BACKEND_NONE : 0;
    opts->general.timeout = 0;
    opts->theme.default_id = THEMES_THEME_BREEZY_DARK;
    opts->theme.alternate_id = THEMES_THEME_BREEZY_LIGHT;
    opts->input.keyboard = true;
    opts->input.pointer = true;
    opts->input.touchscreen = true;
}

static void parse_file(const char *path, config_opts *opts) {
    if (ini_parse(path, parsing_handler, opts) != 0) {
        printf("Ignoring invalid config file %s\n", path);
    }
}

static int parsing_handler(void* user_data, const char* section, const char* key, const char* value) {
    config_opts *opts = (config_opts *)user_data;

    if (strcmp(section, "general") == 0) {
        if (strcmp(key, "animations") == 0) {
            if (parse_bool(value, &(opts->general.animations))) {
                return 1;
            }
        } else if (strcmp(key, "backend") == 0) {
            backends_backend_id_t id = backends_find_backend_with_name(value);
            if (id != BACKENDS_BACKEND_NONE) {
                opts->general.backend = id;
                return 1;
            }
        } else if (strcmp(key, "timeout") == 0) {
            /* Use a max ceiling of 60 minutes (3600 secs) */
            opts->general.timeout = (uint16_t)LV_MIN(strtoul(value, (char **)NULL, 10), 3600);
            return 1;
        }
    } else if (strcmp(section, "theme") == 0) {
        if (strcmp(key, "default") == 0) {
            themes_theme_id_t id = themes_find_theme_with_name(value);
            if (id != THEMES_THEME_NONE) {
                opts->theme.default_id = id;
                return 1;
            }
        } else if (strcmp(key, "alternate") == 0) {
            themes_theme_id_t id = themes_find_theme_with_name(value);
            if (id != THEMES_THEME_NONE) {
                opts->theme.alternate_id = id;
                return 1;
            }
        }
    } else if (strcmp(section, "input") == 0) {
        if (strcmp(key, "keyboard") == 0) {
            if (parse_bool(value, &(opts->input.keyboard))) {
                return 1;
            }
        } else if (strcmp(key, "pointer") == 0) {
            if (parse_bool(value, &(opts->input.pointer))) {
                return 1;
            }
        } else if (strcmp(key, "touchscreen") == 0) {
            if (parse_bool(value, &(opts->input.touchscreen))) {
                return 1;
            }
        }
    }

    printf("Ignoring invalid config value \"%s\" for key \"%s\" in section \"%s\"\n", value, key, section);
    return 1; /* Return 1 (true) so that we can use the return value of ini_parse exclusively for file-level errors (e.g. file not found) */
}

static bool parse_bool(const char *value, bool *result) {
    if (strcmp(value, "true") == 0) {
        *result = true;
        return true;
    }

    if (strcmp(value, "false") == 0) {
        *result = false;
        return true;
    }

    return false;
}


/**
 * Public functions
 */

void config_parse(const char **files, int num_files, config_opts *opts) {
    init_opts(opts);
    for (int i = 0; i < num_files; ++i) {
        parse_file(files[i], opts);
    }
}
