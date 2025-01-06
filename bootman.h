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


#ifndef FURIOS_RECOVERY_H
#define FURIOS_RECOVERY_H

#include "lvgl/lvgl.h"
#include <sys/stat.h>
#include <sys/mount.h>

#ifndef VERSION
#define VERSION "?" /* Just to silence IDE warning. Real version injected by meson during build. */
#endif

/**
 * Fonts
 */

#define SYMBOL_ADJUST "\xef\x81\x82" // 0xF042 (https://fontawesome.com/v5.15/icons/adjust?style=solid)
#define SYMBOL_CHEVRON_LEFT "\xef\x81\x93" // 0xF053 (https://fontawesome.com/v5/icons/chevron-left?style=solid&s=solid)

#endif /* BOOTMAN_H */
