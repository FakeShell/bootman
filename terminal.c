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


#include "terminal.h"

#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>

#include <linux/kd.h>

#include <sys/ioctl.h>


/**
 * Static variables
 */

static int current_fd = -1;

static int original_mode = KD_TEXT;
static int original_kb_mode = K_UNICODE;


/**
 * Static prototypes
 */

/**
 * Close the current file descriptor and reopen /dev/tty0.
 * 
 * @return true if opening was successful, false otherwise
 */
static bool reopen_current_terminal(void);

/**
 * Close the current file descriptor.
 */
static void close_current_terminal(void);


/**
 * Static functions
 */

static bool reopen_current_terminal(void) {
    close_current_terminal();

    current_fd = open("/dev/tty0", O_RDWR);
    if (current_fd < 0) {
        printf("Could not open /dev/tty0\n");
        return false;
    }

    return true;
}

static void close_current_terminal(void) {
    if (current_fd < 0) {
        return;
    }

    close(current_fd);
    current_fd = -1;
}


/**
 * Public functions
 */

void terminal_prepare_current_terminal(void) {
    reopen_current_terminal();

    if (current_fd < 0) {
        printf("Could not prepare current terminal\n");
        return;
    }

    // NB: The order of calls appears to matter for some devices. See
    // https://gitlab.com/cherrypicker/unl0kr/-/issues/34 for further info.

    if (ioctl(current_fd, KDGKBMODE, &original_kb_mode) != 0) {
        printf("Could not get terminal keyboard mode\n");
    }

    if (ioctl(current_fd, KDSKBMODE, K_OFF) != 0) {
        printf("Could not set terminal keyboard mode to off\n");
    }

    if (ioctl(current_fd, KDGETMODE, &original_mode) != 0) {
        printf("Could not get terminal mode\n");
    }

    if (ioctl(current_fd, KDSETMODE, KD_GRAPHICS) != 0) {
        printf("Could not set terminal mode to graphics\n");
    }
}

void terminal_reset_current_terminal(void) {
    if (current_fd < 0) {
        printf("Could not reset current terminal\n");
        return;
    }

    // NB: The order of calls appears to matter for some devices. See
    // https://gitlab.com/cherrypicker/unl0kr/-/issues/34 for further info.

    if (ioctl(current_fd, KDSETMODE, original_mode) != 0) {
        printf("Could not reset terminal mode\n");
    }

    if (ioctl(current_fd, KDSKBMODE, original_kb_mode) != 0) {
        printf("Could not reset terminal keyboard mode\n");
    }

    close_current_terminal();
}
