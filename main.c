/**
 * Copyright 2021 Johannes Marbach
 * Copyright 2024 David Badiei
 * Copyright 2025 Bardia Moshiri
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
#include "command_line.h"
#include "config.h"
#include "indev.h"
#include "bootman.h"
#include "terminal.h"
#include "theme.h"
#include "themes.h"

#include "lv_drv_conf.h"

#if USE_FBDEV
#include "lv_drivers/display/fbdev.h"
#endif /* USE_FBDEV */
#if USE_DRM
#include "lv_drivers/display/drm.h"
#endif /* USE_DRM */
#if USE_MINUI
#include "lv_drivers/display/minui.h"
#endif /* USE_MINUI */

#include "lvgl/lvgl.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>

#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <libinput.h>
#include <linux/input.h>

#define MAX_PARTITIONS 50
#define MAX_LINE_LENGTH 256
#define PERSIST_PARTITION "/dev/disk/by-partlabel/vendor_boot_a"
#define MOUNT_POINT "/furios-persist"
#define PARTITIONS_FILE "/furios-persist/bootman/partitions"
#define DROIDIAN_VG_PATH "/dev/droidian"
#define FURIOS_VG_PATH "/dev/furios"

typedef struct {
    char *name;
    char *label;
    char *vg_path;
} PartitionEntry;

typedef struct {
    PartitionEntry entries[MAX_PARTITIONS];
    size_t count;
} PartitionList;

/**
 * Static variables
 */
cli_opts cli_options;
config_opts conf_opts;
static lv_color_t *buf = NULL;
static lv_disp_draw_buf_t disp_buf;
bool is_alternate_theme = false;
lv_obj_t *reboot_btn;
lv_obj_t *shutdown_btn;

/* Navigation variables */
lv_obj_t **nav_buttons = NULL;
int nav_button_count = 0;
int current_button_index = 0;
pthread_t key_thread;
volatile bool key_thread_running = true;

/**
 * Static prototypes
 */

/**
 * Set the UI theme.
 *
 * @param is_dark true if the dark theme should be applied, false if the light theme should be applied
 */
static void set_theme(bool is_dark);

/**
 * Handle LV_EVENT_CLICKED events from the shutdown button.
 *
 * @param event the event object
 */
static void shutdown_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the shutdown message box.
 *
 * @param event the event object
 */
static void shutdown_mbox_value_changed_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the reboot button.
 *
 * @param event the event object
 */
static void reboot_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the reboot message box.
 *
 * @param event the event object
 */
static void reboot_mbox_value_changed_cb(lv_event_t *event);

/**
 * Handle clicks on partition buttons
 *
 * @param event the event object containing partition data
 */
static void partition_btn_clicked_cb(lv_event_t *event);

/**
 * Check partition, read version, and flash boot images.
 *
 * @param partition_name name of the partition to boot
 * @param vg_path volume group path (DROIDIAN_VG_PATH or FURIOS_VG_PATH) or NULL for external paths
 * @param error_msg buffer to store error message on failure
 * @param error_msg_size size of error message buffer
 * @return 0 on success, -1 on failure with error_msg set
 */
static int check_and_flash_partition(const char *partition_name, const char *vg_path, char *error_msg, size_t error_msg_size);

/**
 * Check if a volume group exists
 *
 * @param vg_path Path to the volume group
 * @return true if the volume group exists, false otherwise
 */
static bool volume_group_exists(const char *vg_path);

/**
 * Show error message dialog.
 *
 * @param message the error message to display
 */
static void show_error_dialog(const char *message);

/**
 * Handle error dialog button events.
 *
 * @param event the event object
 */
static void error_mbox_event_cb(lv_event_t *event);

/**
 * Read and parse partition entries from config file
 * Checks for both FuriOS and Droidian volume groups
 *
 * @return PartitionList* List of parsed partitions or NULL on error
 */
static PartitionList* read_partition_entries(void);

/**
 * Check if system is encrypted by looking for droidian_encrypted or furios_encrypted mapper.
 *
 * @return true if system is encrypted, false otherwise
 */
static bool is_encrypted(void);

/**
 * Free memory allocated for partition list
 *
 * @param list PartitionList to free
 */
static void free_partition_list(PartitionList *list);

/**
 * Create partition buttons based on parsed entries
 *
 * @param label_container Container to place buttons in
 * @param list List of partitions to create buttons for
 */
static void create_partition_buttons(lv_obj_t *label_container, PartitionList *list);

/**
 * Create main UI
 *
 * @param hor_res horizontal resolution
 * @param ver_res vertical resolution
 */
static void create_ui(uint32_t hor_res, uint32_t ver_res);

/**
 * Initialize UI
 */
static void initialize_ui(void);

/**
 * Reboots the device.
 */
static void reboot_device(void);

/**
 * Shuts down the device.
 */
static void shutdown(void);

/**
 * Handle termination signals sent to the process.
 *
 * @param signum the signal's number
 */
static void sigaction_handler(int signum);

/**
 * Initialize button navigation
 *
 * @param total_buttons Total number of buttons for navigation
 */
static void init_button_navigation(int total_buttons);

/**
 * Update button highlighting
 */
static void update_button_highlight(void);

/**
 * Check if a file is an input device
 *
 * @param path Path to the input device
 * @return 1 if it's an input device, 0 otherwise
 */
static int is_input_device(const char *path);

/**
 * Initialize libinput and monitor for key events
 *
 * @param arg *arg is unused
 */
static void* key_input_thread(void *arg);

/**
 * Open callback for libinput
 *
 * @param path Device path to open
 * @param flags Open flags
 * @param user_data User data pointer (user_data is unused)
 * @return File descriptor or negative error code
 */
static int open_restricted(const char *path, int flags, void *user_data);

/**
 * Close callback for libinput
 *
 * @param fd File descriptor to close
 * @param user_data User data pointer (user_data is unused)
 */
static void close_restricted(int fd, void *user_data);

/**
 * Static functions
 */

static void set_theme(bool is_alternate) {
    theme_apply(&(themes_themes[is_alternate ? conf_opts.theme.alternate_id : conf_opts.theme.default_id]));
}

static void shutdown_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    static const char *btns[] = { "Yes", "No", "" };
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, "Shutdown device?", btns, false);
    lv_obj_set_size(mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(mbox, shutdown_mbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

static void shutdown_mbox_value_changed_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(mbox) == 0)
        shutdown();
    lv_msgbox_close(mbox);
}

static void reboot_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    static const char *btns[] = { "Yes", "No", "" };
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, "Reboot device?", btns, false);
    lv_obj_set_size(mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(mbox, reboot_mbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

static void reboot_mbox_value_changed_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(mbox) == 0)
        reboot_device();
    lv_msgbox_close(mbox);
}

static void partition_btn_clicked_cb(lv_event_t *e) {
    PartitionEntry *entry = (PartitionEntry *)lv_event_get_user_data(e);
    if (entry) {
        printf("Preparing to boot partition: %s from VG: %s\n", entry->name,
               entry->vg_path ? entry->vg_path : "external");

        char error_msg[512];
        if (check_and_flash_partition(entry->name, entry->vg_path, error_msg, sizeof(error_msg)) == 0) {
            printf("Successfully prepared boot for partition: %s\n", entry->name);
            reboot_device();
        } else {
            show_error_dialog(error_msg);
        }

        free(entry->name);
        free(entry->label);
        free(entry);
    }
}

static int check_and_flash_partition(const char *partition_name, const char *vg_path, char *error_msg, size_t error_msg_size) {
    char device_path[256];
    char mount_point[] = "/mnt_tmp";
    char version[256];
    struct stat st;
    char cmd[1024];

    /* if this is a direct path then its likely some external device (such as an sdcard) */
    if (partition_name[0] == '/' || vg_path == NULL) {
        strncpy(device_path, partition_name, sizeof(device_path) - 1);
        device_path[sizeof(device_path) - 1] = '\0';
    } else {
        snprintf(device_path, sizeof(device_path), "%s/%s", vg_path, partition_name);
    }

    if (stat(device_path, &st) != 0) {
        snprintf(error_msg, error_msg_size, "Partition %s does not exist", device_path);
        return -1;
    }

    if (stat(mount_point, &st) != 0) {
        if (mkdir(mount_point, 0755) != 0) {
            snprintf(error_msg, error_msg_size, "Failed to create mount point: %s", strerror(errno));
            return -1;
        }
    }

    if (mount(device_path, mount_point, "ext4", 0, NULL) != 0) {
        snprintf(error_msg, error_msg_size, "Failed to mount partition: %s", strerror(errno));
        return -1;
    }

    /* this was added on 13.0.6 and thus the boot manager will only work with versions 13.0.6 or newer */
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/usr/lib/furios/device/flash-bootimage.conf", mount_point);

    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        snprintf(error_msg, error_msg_size, "Config file not found: %s", config_path);
        umount(mount_point);
        return -1;
    }

    char line[256];
    bool version_found = false;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VERSION=", 8) == 0) {
            strncpy(version, line + 8, sizeof(version) - 1);
            version[strcspn(version, "\n")] = 0;
            version_found = true;
            break;
        }
    }

    fclose(fp);

    if (!version_found) {
        snprintf(error_msg, error_msg_size, "VERSION not found in config file");
        umount(mount_point);
        return -1;
    }

    char boot_image_path[512];
    snprintf(boot_image_path, sizeof(boot_image_path), "%s/boot/boot.img-%s", mount_point, version);

    if (stat(boot_image_path, &st) != 0) {
        snprintf(error_msg, error_msg_size, "Boot image not found: %s", boot_image_path);
        umount(mount_point);
        return -1;
    }

    char dtbo_image_path[512];
    snprintf(dtbo_image_path, sizeof(dtbo_image_path), "%s/boot/dtbo.img-%s", mount_point, version);

    if (stat(dtbo_image_path, &st) != 0) {
        snprintf(error_msg, error_msg_size, "DTBO image not found: %s", dtbo_image_path);
        umount(mount_point);
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "dd if='%s' of=/dev/disk/by-partlabel/boot_a bs=4M", boot_image_path);
    printf("Executing: %s\n", cmd);

    if (system(cmd) != 0) {
        snprintf(error_msg, error_msg_size, "Failed to flash boot image");
        umount(mount_point);
        return -1;
    }

    sync();

    snprintf(cmd, sizeof(cmd), "dd if='%s' of=/dev/disk/by-partlabel/dtbo_a bs=4M", dtbo_image_path);
    printf("Executing: %s\n", cmd);
    if (system(cmd) != 0) {
        snprintf(error_msg, error_msg_size, "Failed to flash dtbo image");
        umount(mount_point);
        return -1;
    }

    sync();

    /* Store full path for partitions with volume group */
    char next_boot_value[512];
    if (partition_name[0] == '/' || vg_path == NULL) {
        snprintf(next_boot_value, sizeof(next_boot_value), "%s", partition_name);
    } else {
        snprintf(next_boot_value, sizeof(next_boot_value), "%s/%s", vg_path, partition_name);
    }

    FILE *next_boot = fopen("/furios-persist/bootman/next-boot", "w");
    if (!next_boot) {
        snprintf(error_msg, error_msg_size, "Failed to create next-boot file");
        umount(mount_point);
        return -1;
    }

    fprintf(next_boot, "%s", next_boot_value);
    fclose(next_boot);

    sync();

    umount(mount_point);
    return 0;
}

static bool volume_group_exists(const char *vg_path) {
    struct stat st;
    return (stat(vg_path, &st) == 0);
}

static void show_error_dialog(const char *message) {
    static const char *btns[] = {"OK", ""};
    lv_obj_t *error_mbox = lv_msgbox_create(NULL, "Error", message, btns, false);
    lv_obj_set_size(error_mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(error_mbox, error_mbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(error_mbox);
}

static void error_mbox_event_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    lv_msgbox_close(mbox);
}

static void reboot_device(void) {
    sync();
    reboot(RB_AUTOBOOT);
}

static void shutdown(void) {
    sync();
    reboot(RB_POWER_OFF);
}

static void sigaction_handler(int signum) {
    LV_UNUSED(signum);
    key_thread_running = false;
    pthread_join(key_thread, NULL);
    if (nav_buttons != NULL) {
        free(nav_buttons);
        nav_buttons = NULL;
    }
    terminal_reset_current_terminal();
    exit(0);
}

static bool is_encrypted(void) {
    struct stat st;
    return (stat("/dev/mapper/droidian_encrypted", &st) == 0 ||
            stat("/dev/mapper/furios_encrypted", &st) == 0);
}

static void free_partition_list(PartitionList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->entries[i].name);
        free(list->entries[i].label);
    }
}

static PartitionList* read_partition_entries(void) {
    struct stat st;
    PartitionList *list = calloc(1, sizeof(PartitionList));
    if (!list)
        return NULL;

    printf("Reading partition entries...\n");

    if (stat(PERSIST_PARTITION, &st) != 0) {
        printf("Persist partition not found\n");
        free(list);
        return NULL;
    }

    if (stat(MOUNT_POINT, &st) != 0) {
        if (mkdir(MOUNT_POINT, 0755) != 0) {
            printf("Failed to create mount point: %s\n", strerror(errno));
            free(list);
            return NULL;
        }
    }

    FILE *mtab = fopen("/proc/mounts", "r");
    char line[256];
    int is_mounted = 0;

    while (fgets(line, sizeof(line), mtab)) {
        if (strstr(line, MOUNT_POINT)) {
            is_mounted = 1;
            break;
        }
    }

    fclose(mtab);

    if (!is_mounted) {
        if (mount(PERSIST_PARTITION, MOUNT_POINT, "ext4", 0, NULL) != 0) {
            printf("Failed to mount partition: %s\n", strerror(errno));
            free(list);
            return NULL;
        }
        printf("Mounted %s at %s\n", PERSIST_PARTITION, MOUNT_POINT);
    }

    FILE *fp = fopen(PARTITIONS_FILE, "r");
    if (!fp) {
        printf("Failed to open partitions file: %s\n", strerror(errno));
        if (!is_mounted)
            umount(MOUNT_POINT);
        free(list);
        return NULL;
    }

    /* Check which volume groups exist */
    bool furios_vg_exists = volume_group_exists(FURIOS_VG_PATH);
    bool droidian_vg_exists = volume_group_exists(DROIDIAN_VG_PATH);

    printf("Parsing partition entries:\n");
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), fp) && list->count < MAX_PARTITIONS) {
        if (buffer[0] == '\n' || buffer[0] == '\0')
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        printf("Found entry: %s\n", buffer);

        char *str = buffer;
        char *token;
        char *saveptr;

        token = strtok_r(str, ":", &saveptr);
        if (!token)
            continue;

        if (strcmp(token, "ubuntu-userdata") == 0) {
            printf("Skipping ubuntu-userdata partition\n");
            continue;
        }

        /* If the token is a direct path, use it as-is */
        if (token[0] == '/') {
            list->entries[list->count].name = strdup(token);
            list->entries[list->count].vg_path = NULL; /* Direct path */
        } else {
            /* Add the partition to both VGs if they exist */
            if (furios_vg_exists) {
                list->entries[list->count].name = strdup(token);
                list->entries[list->count].vg_path = FURIOS_VG_PATH;

                /* Get the label */
                token = strtok_r(NULL, "", &saveptr);
                if (!token)
                    list->entries[list->count].label = strdup(list->entries[list->count].name);
                else
                    list->entries[list->count].label = strdup(token);

                printf("Added partition %zu: name='%s', label='%s'\n",
                       list->count,
                       list->entries[list->count].name,
                       list->entries[list->count].label);

                list->count++;
                if (list->count >= MAX_PARTITIONS)
                    break;
            }

            if (droidian_vg_exists) {
                char *original_token = token;
                list->entries[list->count].name = strdup(original_token ? original_token : str);
                list->entries[list->count].vg_path = DROIDIAN_VG_PATH;

                /* Reset the strtok_r for this iteration */
                token = strtok_r(NULL, "", &saveptr);
                if (!token)
                    list->entries[list->count].label = strdup(list->entries[list->count].name);
                else
                    list->entries[list->count].label = strdup(token);

                printf("Added partition %zu: name='%s', label='%s'\n",
                       list->count,
                       list->entries[list->count].name,
                       list->entries[list->count].label);

                list->count++;
            }

            /* Continue to the next line */
            continue;
        }

        token = strtok_r(NULL, "", &saveptr);
        if (!token)
            list->entries[list->count].label = strdup(list->entries[list->count].name);
        else
            list->entries[list->count].label = strdup(token);

        printf("Added external partition %zu: name='%s', label='%s'\n",
               list->count,
               list->entries[list->count].name,
               list->entries[list->count].label);

        list->count++;
    }

    printf("Found total %zu partitions\n", list->count);
    fclose(fp);
    return list;
}

static void init_button_navigation(int total_buttons) {
    if (nav_buttons != NULL)
        free(nav_buttons);

    nav_buttons = calloc(total_buttons, sizeof(lv_obj_t *));
    nav_button_count = total_buttons;
    current_button_index = 0;

    printf("Initialized navigation with %d buttons\n", total_buttons);
}

static void update_button_highlight(void) {
    /* Remove highlight from all buttons first */
    for (int i = 0; i < nav_button_count; i++) {
        lv_obj_clear_state(nav_buttons[i], LV_STATE_FOCUSED);
    }

    /* Add highlight to current button */
    lv_obj_add_state(nav_buttons[current_button_index], LV_STATE_FOCUSED);
    printf("Button %d highlighted\n", current_button_index);
}

static int is_input_device(const char *path) {
    int fd;
    char name[256];

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;

    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}

static int open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void* key_input_thread(void *arg) {
    (void)arg;
    struct libinput *li;
    struct libinput_event *event;
    int rc;

    li = libinput_path_create_context(&interface, NULL);
    if (!li) {
        fprintf(stderr, "Failed to initialize libinput context\n");
        return NULL;
    }

    DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];

    dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "Failed to open /dev/input directory\n");
        libinput_unref(li);
        return NULL;
    }

    int device_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            if (is_input_device(path)) {
                struct libinput_device *device;
                device = libinput_path_add_device(li, path);
                if (!device) {
                    fprintf(stderr, "Failed to add device: %s\n", path);
                } else {
                    printf("Added input device: %s\n", path);
                    device_count++;
                }
            }
        }
    }

    closedir(dir);

    if (device_count == 0) {
        fprintf(stderr, "No input devices were added\n");
        libinput_unref(li);
        return NULL;
    }

    printf("Monitoring %d input devices for key events\n", device_count);

    libinput_dispatch(li);

    while (key_thread_running) {
        fd_set fds;
        int fd = libinput_get_fd(li);

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        rc = select(fd + 1, &fds, NULL, NULL, NULL);
        if (rc < 0 && errno != EINTR) {
            fprintf(stderr, "select() failed: %s\n", strerror(errno));
            break;
        }

        if (rc > 0 && FD_ISSET(fd, &fds)) {
            libinput_dispatch(li);

            while ((event = libinput_get_event(li)) != NULL) {
                if (libinput_event_get_type(event) == LIBINPUT_EVENT_KEYBOARD_KEY) {
                    struct libinput_event_keyboard *key_event;
                    enum libinput_key_state state;
                    uint32_t key;

                    key_event = libinput_event_get_keyboard_event(event);
                    key = libinput_event_keyboard_get_key(key_event);
                    state = libinput_event_keyboard_get_key_state(key_event);

                    struct libinput_device *device = libinput_event_get_device(event);
                    const char *device_name = libinput_device_get_name(device);

                    printf("Key event from '%s': key=%d, state=%d\n",
                           device_name, key, state);
                    if (state == LIBINPUT_KEY_STATE_PRESSED) {
                        switch (key) {
                            case KEY_VOLUMEUP:
                                /* Navigate up */
                                if (current_button_index > 0)
                                    current_button_index--;
                                else
                                    /* Wrap around to bottom */
                                    current_button_index = nav_button_count - 1;
                                update_button_highlight();
                                break;
                            case KEY_VOLUMEDOWN:
                                /* Navigate down */
                                if (current_button_index < nav_button_count - 1)
                                    current_button_index++;
                                else
                                    /* Wrap around to top */
                                    current_button_index = 0;
                                update_button_highlight();
                                break;
                            case KEY_POWER:
                                if (current_button_index >= 0 && current_button_index < nav_button_count)
                                    lv_event_send(nav_buttons[current_button_index], LV_EVENT_CLICKED, NULL);
                                break;
                        }
                    }
                }
                libinput_event_destroy(event);
            }
        }
    }

    libinput_unref(li);
    return NULL;
}

static void create_partition_buttons(lv_obj_t *label_container, PartitionList *list) {
    if (!list)
        return;

    int base_y_offset = 150;
    int button_spacing = 120;

    int total_buttons = list->count + 2;
    init_button_navigation(total_buttons);

    int btn_index = 0;

    for (size_t i = 0; i < list->count; i++) {
        lv_obj_t *btn = lv_btn_create(label_container);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_height(btn, 100);

        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, list->entries[i].label);

        char *partition_name = strdup(list->entries[i].name);
        lv_obj_add_event_cb(btn, partition_btn_clicked_cb, LV_EVENT_CLICKED, partition_name);

        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, base_y_offset + (i * button_spacing));
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        nav_buttons[btn_index++] = btn;
    }

    reboot_btn = lv_btn_create(label_container);
    lv_obj_set_width(reboot_btn, LV_PCT(100));
    lv_obj_set_height(reboot_btn, 100);
    lv_obj_t *reboot_label = lv_label_create(reboot_btn);
    lv_label_set_text(reboot_label, "Reboot");
    lv_obj_add_event_cb(reboot_btn, reboot_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(reboot_btn, LV_ALIGN_TOP_MID, 0, base_y_offset + (list->count * button_spacing));
    lv_obj_set_flex_flow(reboot_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(reboot_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    nav_buttons[btn_index++] = reboot_btn;

    shutdown_btn = lv_btn_create(label_container);
    lv_obj_set_width(shutdown_btn, LV_PCT(100));
    lv_obj_set_height(shutdown_btn, 100);
    lv_obj_t *shutdown_label = lv_label_create(shutdown_btn);
    lv_label_set_text(shutdown_label, "Shutdown");
    lv_obj_add_event_cb(shutdown_btn, shutdown_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(shutdown_btn, LV_ALIGN_TOP_MID, 0, base_y_offset + ((list->count + 1) * button_spacing));
    lv_obj_set_flex_flow(shutdown_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(shutdown_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    nav_buttons[btn_index++] = shutdown_btn;

    update_button_highlight();
}

static void create_ui(uint32_t hor_res, uint32_t ver_res) {
    /* Clear the screen */
    lv_obj_clean(lv_scr_act());

    /* Check for encryption */
    if (is_encrypted()) {
        printf("System is encrypted, cannot proceed\n");
        exit(1);
    }

    /* Figure out a few numbers for sizing and positioning */
    const int keyboard_height = ver_res > hor_res ? ver_res / 3 : ver_res / 2;
    const int padding = keyboard_height / 8;
    const int label_width = hor_res - 2 * padding;

    /* Main flexbox */
    lv_obj_t *container = lv_obj_create(lv_scr_act());
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(container, LV_PCT(100), ver_res - keyboard_height);
    lv_obj_set_pos(container, 0, 0);
    lv_obj_set_style_pad_row(container, padding, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(container, padding, LV_PART_MAIN);

    /* Label container */
    lv_obj_t *label_container = lv_obj_create(container);
    lv_obj_set_size(label_container, label_width, LV_PCT(100));
    lv_obj_set_flex_grow(label_container, 1);

    /* FuriOS label container */
    lv_obj_t *furios_label_container = lv_obj_create(lv_scr_act());
    lv_obj_set_width(furios_label_container, LV_PCT(100));
    lv_obj_set_height(furios_label_container, LV_SIZE_CONTENT);
    lv_obj_set_align(furios_label_container, LV_ALIGN_BOTTOM_MID);

    /* FuriOS label text */
    lv_obj_t *furios_label = lv_label_create(furios_label_container);
    lv_label_set_text(furios_label, "FuriOS Boot Manager");
    lv_obj_align(furios_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Create partition buttons */
    PartitionList *list = read_partition_entries();
    create_partition_buttons(label_container, list);
    if (list) {
        free_partition_list(list);
        free(list);
    } else {
        printf("No partitions found in persist\n");
        exit(1);
    }
}

static void initialize_ui(void) {
    /* Initialise LVGL and set up logging callback */
    lv_init();

    /* Initialise display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /* Initialise framebuffer driver and query display size */
    uint32_t hor_res = 0;
    uint32_t ver_res = 0;
    uint32_t dpi = 0;

    switch (conf_opts.general.backend) {
#if USE_FBDEV
    case BACKENDS_BACKEND_FBDEV:
        fbdev_init();
        fbdev_get_sizes(&hor_res, &ver_res, &dpi);
        disp_drv.flush_cb = fbdev_flush;
        break;
#endif /* USE_FBDEV */
#if USE_DRM
    case BACKENDS_BACKEND_DRM:
        drm_init();
        drm_get_sizes((lv_coord_t *)&hor_res, (lv_coord_t *)&ver_res, &dpi);
        disp_drv.flush_cb = drm_flush;
        break;
#endif /* USE_DRM */
#if USE_MINUI
    case BACKENDS_BACKEND_MINUI:
        minui_init();
        minui_get_sizes(&hor_res, &ver_res, &dpi);
        disp_drv.flush_cb = minui_flush;
        break;
#endif /* USE_MINUI */
    default:
        printf("Unable to find suitable backend\n");
        exit(EXIT_FAILURE);
    }

    /* Override display parameters with command line options if necessary */
    if (cli_options.hor_res > 0)
        hor_res = cli_options.hor_res;
    if (cli_options.ver_res > 0)
        ver_res = cli_options.ver_res;
    if (cli_options.dpi > 0)
        dpi = cli_options.dpi;

    /* Prepare display buffer */
    const size_t buf_size = hor_res * ver_res / 10; /* At least 1/10 of the display size is recommended */
    if (buf == NULL)
        buf = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf, NULL, buf_size);

    /* Register display driver */
    disp_drv.draw_buf = &disp_buf;
    disp_drv.hor_res = hor_res;
    disp_drv.ver_res = ver_res;
    disp_drv.offset_x = cli_options.x_offset;
    disp_drv.offset_y = cli_options.y_offset;
    disp_drv.dpi = dpi;
    lv_disp_drv_register(&disp_drv);

    printf("Display resolution: %dx%d, DPI: %d, Offset: (%d, %d)\n",
           hor_res, ver_res, dpi, cli_options.x_offset, cli_options.y_offset);

    /* Connect input devices */
    indev_auto_connect(conf_opts.input.keyboard, conf_opts.input.pointer, conf_opts.input.touchscreen);
    indev_set_up_mouse_cursor();

    /* Initialise theme */
    set_theme(is_alternate_theme);

    /* Create UI elements */
    create_ui(hor_res, ver_res);

    /* Add custom focus style for button navigation */
    static lv_style_t style_focus;
    lv_style_init(&style_focus);
    lv_style_set_border_width(&style_focus, 3);
    lv_style_set_border_color(&style_focus, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_border_opa(&style_focus, LV_OPA_COVER);

    /* Apply the focus style to all buttons */
    for (int i = 0; i < nav_button_count; i++) {
        if (nav_buttons[i] != NULL)
            lv_obj_add_style(nav_buttons[i], &style_focus, LV_STATE_FOCUSED);
    }
}

int main(int argc, char *argv[]) {
    /* Parse command line options */
    cli_parse_opts(argc, argv, &cli_options);

    /* Parse config files */
    config_parse(cli_options.config_files, cli_options.num_config_files, &conf_opts);

    /* Prepare current TTY and clean up on termination */
    terminal_prepare_current_terminal();
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigaction_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    initialize_ui();

    /* Start key input thread for hardware button navigation */
    key_thread_running = true;
    if (pthread_create(&key_thread, NULL, key_input_thread, NULL) != 0)
        printf("Failed to create key input thread: %s\n", strerror(errno));
    else
        printf("Key input thread started successfully\n");

    /* Run lvgl in "tickless" mode */
    while (1) {
        lv_task_handler();
        usleep(5000);
    }

    return 0;
}

/**
 * Generate tick for LVGL.
 *
 * @return tick in ms
 */
uint32_t get_tick(void) {
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
