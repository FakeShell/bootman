/**
 * Copyright 2024 Bardia Moshiri
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

#ifndef LVM_H
#define LVM_H

#include <stdlib.h>

int is_lv_encrypted_with_luks(const char *device_path, size_t print_bytes);
int mount_luks_lvm(const char *passphrase);
int mount_luks_lvm_droidian_helper(const char *passphrase);

#endif // LVM_H
