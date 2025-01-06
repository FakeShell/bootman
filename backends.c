/**
 * Copyright 2022 Eugenio Paolantonio (g7)
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


#include "backends.h"

#include <string.h>
#include <stdio.h>

/**
 * Public interface
 */

const char *backends_backends[] = {
#if USE_MINUI
    "minui",
#endif /* USE_MINUI */
#if USE_FBDEV
    "fbdev",
#endif /* USE_FBDEV */
#if USE_DRM
    "drm",
#endif /* USE_DRM */
    NULL
};

backends_backend_id_t backends_find_backend_with_name(const char *name) {
    for (int i = 0; backends_backends[i] != NULL; ++i) {
        if (strcmp(backends_backends[i], name) == 0) {
            printf("Found backend: %s\n", name);
            return i;
        }
    }
    printf("Backend %s not found\n", name);
    return BACKENDS_BACKEND_NONE;
}
