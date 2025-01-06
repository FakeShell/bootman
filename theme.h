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


#ifndef THEME_H
#define THEME_H

#include "lvgl/lvgl.h"

#include <stdbool.h>
#include <stdint.h>

#define WIDGET_HEADER LV_OBJ_FLAG_USER_1

/**
 * Theming structs
 */

/* Window theme */
typedef struct {
    uint32_t bg_color;
} theme_window;

/* Header theme */
typedef struct {
    uint32_t bg_color;
    lv_coord_t border_width;
    uint32_t border_color;
    lv_coord_t pad;
    lv_coord_t gap;
} theme_header;

/* Key theme for one specific key type and state */
typedef struct {
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t border_color;
} theme_key_state;

/* Key theme for one specific key type and all states */
typedef struct {
    theme_key_state normal;
    theme_key_state pressed;
} theme_key;

/* Key theme */
typedef struct {
    lv_coord_t border_width;
    lv_coord_t corner_radius;
    theme_key key_char;
    theme_key key_non_char;
    theme_key key_mod_act;
    theme_key key_mod_inact;
} theme_keys;

/* Button theme for one specific button state */
typedef struct {
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t border_color;
} theme_button_state;

/* Button theme */
typedef struct {
    lv_coord_t border_width;
    lv_coord_t corner_radius;
    lv_coord_t pad;
    theme_button_state normal;
    theme_button_state pressed;
} theme_button;

/* Text area cursor theme */
typedef struct {
    lv_coord_t width;
    uint32_t color;
    int period;
} theme_textarea_cursor;

/* Text area theme */
typedef struct {
    uint32_t fg_color;
    uint32_t bg_color;
    lv_coord_t border_width;
    uint32_t border_color;
    lv_coord_t corner_radius;
    lv_coord_t pad;
    uint32_t placeholder_color;
    theme_textarea_cursor cursor;
} theme_textarea;

/* Dropdown list theme */
typedef struct {
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t selection_fg_color;
    uint32_t selection_bg_color;
    lv_coord_t border_width;
    uint32_t border_color;
    lv_coord_t corner_radius;
    lv_coord_t pad;
} theme_dropdown_list;

/* Dropdown theme */
typedef struct {
    theme_button button;
    theme_dropdown_list list;
} theme_dropdown;

/* Label */
typedef struct {
    uint32_t fg_color;
} theme_label;

/* Message box buttons theme */
typedef struct {
    lv_coord_t gap;
} theme_msgbox_buttons;

/* Message box dimming theme */
typedef struct {
    uint32_t color;
    short opacity;
} theme_msgbox_dimming;

/* Message box theme */
typedef struct {
    uint32_t fg_color;
    uint32_t bg_color;
    lv_coord_t border_width;
    uint32_t border_color;
    lv_coord_t corner_radius;
    lv_coord_t pad;
    lv_coord_t gap;
    theme_msgbox_buttons buttons;
    theme_msgbox_dimming dimming;
} theme_msgbox;

/* Progress bar indicator theme */
typedef struct {
    uint32_t bg_color;
} theme_bar_indicator;

/* Progress bar theme */
typedef struct {
    lv_coord_t border_width;
    uint32_t border_color;
    lv_coord_t corner_radius;
    theme_bar_indicator indicator;
} theme_bar;

/* Full theme */
typedef struct {
    char *name;
    theme_window window;
    theme_header header;
    theme_button button;
    theme_textarea textarea;
    theme_dropdown dropdown;
    theme_label label;
    theme_msgbox msgbox;
    theme_bar bar;
} theme;

/**
 * Apply a UI theme.
 *
 * @param theme the theme to apply
 */
void theme_apply(const theme *theme);

#endif /* THEME_H */
