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


#ifndef TERMINAL_H
#define TERMINAL_H

/**
 * Prepare the current TTY for graphics output.
 */
void terminal_prepare_current_terminal(void);

/**
 * Reset the current TTY to text output.
 */
void terminal_reset_current_terminal(void);

#endif /* TERMINAL_H */
