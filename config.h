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


#ifndef CONFIG_H
#define CONFIG_H

#include "backends.h"

#include "themes.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * General options
 */
typedef struct {
    /* Backend to use */
    backends_backend_id_t backend;
    /* If true, use animations */
    bool animations;
    /* Timeout (in seconds) - once elapsed, the device will shutdown. 0 (default) to disable */
    uint16_t timeout;
} config_opts_general;

/**
 * Options related to the theme
 */
typedef struct {
    /* Default theme */
    themes_theme_id_t default_id;
    /* Alternate theme */
    themes_theme_id_t alternate_id;
} config_opts_theme;

/**
 * Options related to input devices
 */
typedef struct {
    /* If true and a keyboard device is connected, use it for input */
    bool keyboard;
    /* If true and a pointer device is connected, use it for input */
    bool pointer;
    /* If true and a touchscreen device is connected, use it for input */
    bool touchscreen;
} config_opts_input;

/**
 * Options parsed from config file(s)
 */
typedef struct {
    /* General options */
    config_opts_general general;
    /* Options related to the theme */
    config_opts_theme theme;
    /* Options related to input devices */
    config_opts_input input;
} config_opts;

/**
 * Parse options from one or more configuration files.
 * 
 * @param files paths to configuration files
 * @param num_files number of configuration files
 * @param opts pointer for writing the parsed options into
 */
void config_parse(const char **files, int num_files, config_opts *opts);

#endif /* CONFIG_H */
