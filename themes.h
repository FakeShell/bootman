/**
 * Copyright 2021 Johannes Marbach
 *
 * This file is part of furios-recovery, hereafter referred to as the program.
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


#ifndef THEMES_H
#define THEMES_H

#include "theme.h"

/* Theme IDs, values can be used as indexes into the themes_themes array */
typedef enum {
    THEMES_THEME_NONE = -1,
    THEMES_THEME_BREEZY_LIGHT = 0,
    THEMES_THEME_BREEZY_DARK = 1,
    THEMES_THEME_PMOS_LIGHT = 2,
    THEMES_THEME_PMOS_DARK = 3
} themes_theme_id_t;

/* Themes */
extern const int themes_num_themes;
extern const theme themes_themes[];

/**
 * Find the first theme with a given name.
 *
 * @param name theme name
 * @return ID of the first matching theme or THEMES_THEME_NONE if no theme matched
 */
themes_theme_id_t themes_find_theme_with_name(const char *name);

#endif /* THEMES_H */
