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


#ifndef BACKENDS_H
#define BACKENDS_H

#include "lv_drv_conf.h"

/* NOTE: Only BACKENDS_BACKEND_NONE is ought to have an explicit value assigned */
typedef enum {
    BACKENDS_BACKEND_NONE = -1,
#if USE_MINUI
    BACKENDS_BACKEND_MINUI,
#endif /* USE_MINUI */
#if USE_FBDEV
    BACKENDS_BACKEND_FBDEV,
#endif /* USE_FBDEV */
#if USE_DRM
    BACKENDS_BACKEND_DRM,
#endif /* USE_DRM */
} backends_backend_id_t;

/* Backends */
extern const char *backends_backends[];

/**
 * Find the first backend with a given name.
 *
 * @param name backend name
 * @return ID of the first matching backend or BACKENDS_BACKEND_NONE if no backend matched
 */
backends_backend_id_t backends_find_backend_with_name(const char *name);

#endif /* BACKENDS_H */
