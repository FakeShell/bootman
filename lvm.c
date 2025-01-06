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

#include "lvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <libcryptsetup.h>

#define LUKS_MAGIC "LUKS\xba\xbe"
#define LUKS_MAGIC_LEN 6
#define DEVICE "/dev/droidian/droidian-rootfs"
#define HEADER "/dev/droidian/droidian-reserved"
#define DECRYPTED "/dev/mapper/droidian_encrypted"
#define NAME "droidian_encrypted"
#define PASSPHRASE_MAX 256

int is_lv_encrypted_with_luks(const char *device_path, size_t print_bytes) {
    int fd, result;
    unsigned char *buffer;
    ssize_t read_bytes;

    struct stat st;

    if (stat(DECRYPTED, &st) == 0) {
        // DECRYPTED device exists, return as unencrypted
        return 0;
    }

    fd = open(device_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening device");
        return -1;
    }

    buffer = malloc(print_bytes);
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        close(fd);
        return -1;
    }

    read_bytes = read(fd, buffer, print_bytes);
    if (read_bytes == -1) {
        perror("Error reading device");
        close(fd);
        free(buffer);
        return -1;
    }

/*    printf("Beginning of the LV (%zd bytes):\n", read_bytes);
    for (size_t i = 0; i < read_bytes; ++i) {
        printf("%02x ", buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");*/

    result = (memcmp(buffer, LUKS_MAGIC, LUKS_MAGIC_LEN - 1) == 0) ? 1 : 0;

    close(fd);
    free(buffer);
//    printf("Encryption check result: %d\n", result);
    return result;
}

int mount_luks_lvm(const char *passphrase) {
    struct crypt_device *cd = NULL;
    int result;

    result = crypt_init(&cd, DEVICE);
    if (result < 0) {
        fprintf(stderr, "crypt_init() failed: %s\n", strerror(-result));
        return EXIT_FAILURE;
    }

    // Load the LUKS header from the given header device.
    if (result < 0) {
        fprintf(stderr, "crypt_load() failed: %s\n", strerror(-result));
        crypt_free(cd);
        return EXIT_FAILURE;
    }

    result = crypt_activate_by_passphrase(cd, NAME, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0);
    if (result < 0) {
        fprintf(stderr, "Activation failed: Incorrect passphrase or other error.\n");
        crypt_free(cd);
        return 2;
    }

    printf("LUKS device %s activated successfully.\n", NAME);

    crypt_free(cd);

    return EXIT_SUCCESS;
}

int mount_luks_lvm_droidian_helper(const char *passphrase) {
    if (passphrase == NULL || strlen(passphrase) >= PASSPHRASE_MAX) {
        return EXIT_FAILURE;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            close(pipefd[0]);
            return EXIT_FAILURE;
        }
        close(pipefd[0]);

        execlp("droidian-encryption-helper", "droidian-encryption-helper",
               "--device", DEVICE,
               "--header", HEADER,
               "--name", NAME,
               "--strip-newlines",
               (char *)NULL);

        perror("execlp");
        return EXIT_FAILURE;
    } else {
        close(pipefd[0]);
        write(pipefd[1], passphrase, strlen(passphrase));
        close(pipefd[1]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 2) {
                return 2;
            }
            return exit_code == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        } else {
            return EXIT_FAILURE;
        }
    }
}
