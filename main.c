/**
 * Copyright 2021 Johannes Marbach
 * Copyright 2024 Bardia Moshiri
 * Copyright 2024 David Badiei
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
#include "lvm.h"

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

#include "squeek2lvgl/sq2lv.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define NUM_IMAGES 1
#define MIN_BRIGHTNESS 5
#define BRIGHTNESS_PATH "/sys/class/leds/lcd-backlight/brightness"
#define MAX_BRIGHTNESS_PATH "/sys/class/leds/lcd-backlight/max_brightness"

/**
 * Static variables
 */

cli_opts cli_options;
config_opts conf_opts;

static lv_color_t *buf = NULL;
static lv_disp_draw_buf_t disp_buf;

bool is_alternate_theme = true;
bool is_password_obscured = true;
bool is_keyboard_hidden = true;

bool enabling_ssh = false;

/* Main page */
lv_obj_t *keyboard = NULL;
lv_obj_t *ip_label_container = NULL;
lv_obj_t *ip_label = NULL;
lv_obj_t *reboot_btn;
lv_obj_t *shutdown_btn;
lv_obj_t *factory_reset_btn;
lv_obj_t *theme_btn;
lv_obj_t *ssh_btn;
lv_obj_t *ssh_btn_label;
lv_obj_t *terminal_btn;
lv_obj_t *brightness_slider;

/* Decryption page */
lv_obj_t *decrypt_container = NULL;
lv_obj_t *label_container = NULL;
lv_obj_t *spangroup = NULL;
lv_obj_t *textarea_container = NULL;
lv_obj_t *textarea = NULL;
lv_obj_t *toggle_pw_btn = NULL;
lv_obj_t *toggle_kb_btn = NULL;

LV_IMG_DECLARE(furilabs_white)
LV_IMG_DECLARE(furilabs_black)

const void *darkmode_imgs[] = {&furilabs_white};
const void *lightmode_imgs[] = {&furilabs_black};

/*
   0: FuriLabs logo
*/
lv_obj_t* images[1];

/**
 * Static prototypes
 */

/**
 * Handle LV_EVENT_CLICKED events from the theme toggle button.
 *
 * @param event the event object
 */
static void toggle_theme_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the ssh toggle button.
 *
 * @param event the event object
 */
static void toggle_ssh_btn_clicked_cb(lv_event_t *event);

/**
 * Enable or disable SSH access
 */
static void enable_ssh(void);

/**
 * Toggle between the light and dark theme.
 */
static void toggle_theme(void);

/**
 * Set the UI theme.
 *
 * @param is_dark true if the dark theme should be applied, false if the light theme should be applied
 */
static void set_theme(bool is_dark);

/**
 * Set the image mode
 *
 * @param is_dark true if the dark theme should be applied, false if the light theme should be applied
 */
static void update_image_mode(bool is_dark);

/**
 * Handle LV_EVENT_CLICKED events from the show/hide password toggle button.
 *
 * @param event the event object
 */
static void toggle_pw_btn_clicked_cb(lv_event_t *event);

/**
 * Toggle between showing and hiding the password.
 */
static void toggle_password_obscured(void);

/**
 * Show / hide the password.
 *
 * @param is_hidden true if the password should be hidden, false if it should be shown
 */
static void set_password_obscured(bool is_obscured);

/**
 * Handle LV_EVENT_CLICKED events from the show/hide keyboard toggle button.
 *
 * @param event the event object
 */
static void toggle_kb_btn_clicked_cb(lv_event_t *event);

/**
 * Toggle between showing and hiding the keyboard.
 */
static void toggle_keyboard_hidden(void);

/**
 * Show / hide the keyboard
 *
 * @param is_hidden true if the keyboard should be hidden, false if it should be shown
 */
static void set_keyboard_hidden(bool is_hidden);

/**
 * Callback for the keyboard's vertical slide in / out animation.
 *
 * @param obj keyboard widget
 * @param value y position
 */
static void keyboard_anim_y_cb(void *obj, int32_t value);

/**
 * Callback for the brightness slider
 *
 * @param event the event object
 */
static void brightness_slider_changed_cb(lv_event_t *event);

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
 * Handle LV_EVENT_CLICKED events from the terminal button.
 *
 * @param event the event object
 */
static void terminal_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the terminal message box.
 *
 * @param event the event object
 */
static void terminal_mbox_value_changed_cb(lv_event_t *event);

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
 * Handle LV_EVENT_CLICKED events from the factory reset button.
 */
static void factory_reset_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the factory reset confirmation button.
 */
static void perform_factory_reset(lv_timer_t *timer);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the factory reset message box.
 *
 * @param event the event object
 */
static void factory_reset_mbox_value_changed_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the factory reset failed messsage box
 */
static void close_mbox_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the keyboard widget.
 *
 * @param event the event object
 */
static void keyboard_value_changed_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_READY events from the textarea widget.
 *
 * @param event the event object
 */
static void textarea_ready_cb(lv_event_t *event);

/**
 * Check password against LVM for SSH access
 *
 * @param textarea the textarea widget
 */
static void check_password_enable_ssh(lv_obj_t *textarea);

/**
 * Check password against LVM for factory reset
 *
 * @param textarea the textarea widget
 */
static void check_password_factory_reset(lv_obj_t *textarea);

/**
 * Handle LV_EVENT_CLICKED events from the password check
 */
static void factory_reset_password(lv_timer_t *timer);

/**
 * Returns current slot suffix from cmdline
 */
static char* get_slot_suffix(void);

/**
 * Drop all caches on device
 */
static int drop_caches(void);

/**
 * Factory resets the device
 */
static int factory_reset(void);

/**
 * Restores the screen from the decryption page
 */
static void restore_main_screen(void);

/**
 * Decrypts the device
 */
static void decrypt(void);

/**
 * Reboots the device.
 */
static void reboot_device(void);

/**
 * Shuts down the device.
 */
static void shutdown(void);

/**
 * Launch another instance of recovery before exiting in open_terminal
 *
 * @param unused
 */
static void* run_recovery(void* arg);

/**
 * Opens furios-terminal
 */
static void open_terminal(void);

/**
 * Handle termination signals sent to the process.
 *
 * @param signum the signal's number
 */
static void sigaction_handler(int signum);

/**
 * Read a value from given path and return the value
 *
 * @paran path is the path of the file
 * @param default_value is the default value if there was an error
 */
static int read_int_from_file(const char *path, int default_value);

/**
 * Write a value to a given path
 *
 * @paran path is the path of the file
 * @param value is the value requeted for writing
 */
static int write_int_to_file(const char *path, int value);

/**
 * Create all buttons in the label container
 *
 * @param label container to create buttons in
 */
static void create_buttons(lv_obj_t *label_container);

/**
 * Create main UI
 *
 * @param horizantal resolution
 * @param vertical resolution
 */
static void create_ui(uint32_t hor_res, uint32_t ver_res);

/**
 * Initialize recovery UI
 */
static void initialize_recovery_ui(void);

/**
 * Static functions
 */

static void toggle_theme_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    toggle_theme();
}

static void toggle_theme(void) {
    is_alternate_theme = !is_alternate_theme;

    update_image_mode(is_alternate_theme);
    set_theme(is_alternate_theme);
}

static void update_image_mode(bool is_alternate) {
    for (int i = 0; i < NUM_IMAGES; i++)
        lv_img_set_src(images[i], is_alternate ? lightmode_imgs[i] : darkmode_imgs[i]);
}

static void set_theme(bool is_alternate) {
    theme_apply(&(themes_themes[is_alternate ? conf_opts.theme.alternate_id : conf_opts.theme.default_id]));
}

static void toggle_pw_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    toggle_password_obscured();
}

static void toggle_password_obscured(void) {
    is_password_obscured = !is_password_obscured;
    set_password_obscured(is_password_obscured);
}

static void set_password_obscured(bool is_obscured) {
    lv_obj_t *textarea = lv_keyboard_get_textarea(keyboard);
    lv_textarea_set_password_mode(textarea, is_obscured);
}

static void toggle_kb_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    toggle_keyboard_hidden();
}

static void toggle_keyboard_hidden(void) {
    is_keyboard_hidden = !is_keyboard_hidden;
    set_keyboard_hidden(is_keyboard_hidden);
}

static void set_keyboard_hidden(bool is_hidden) {
    if (!conf_opts.general.animations) {
        lv_obj_set_y(keyboard, is_hidden ? lv_obj_get_height(keyboard) : 0);
        return;
    }

    lv_anim_t keyboard_anim;
    lv_anim_init(&keyboard_anim);
    lv_anim_set_var(&keyboard_anim, keyboard);
    lv_anim_set_values(&keyboard_anim, is_hidden ? 0 : lv_obj_get_height(keyboard), is_hidden ? lv_obj_get_y(keyboard) : 0);
    lv_anim_set_path_cb(&keyboard_anim, lv_anim_path_ease_out);
    lv_anim_set_time(&keyboard_anim, 500);
    lv_anim_set_exec_cb(&keyboard_anim, keyboard_anim_y_cb);
    lv_anim_start(&keyboard_anim);
}

static void keyboard_anim_y_cb(void *obj, int32_t value) {
    lv_obj_set_y(obj, value);
}

static void brightness_slider_changed_cb(lv_event_t *event) {
    lv_obj_t *slider = lv_event_get_target(event);
    int32_t value = lv_slider_get_value(slider);

    if (value < MIN_BRIGHTNESS) {
        value = MIN_BRIGHTNESS;
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
    }

    write_int_to_file(BRIGHTNESS_PATH, value);
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
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        shutdown();
    }
    lv_msgbox_close(mbox);
}

static void terminal_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    static const char *btns[] = { "Yes", "No", "" };
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, "Open terminal?", btns, false);
    lv_obj_set_size(mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(mbox, terminal_mbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

static void terminal_mbox_value_changed_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        open_terminal();
    }
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
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        reboot_device();
    }
    lv_msgbox_close(mbox);
}

static void factory_reset_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    static const char *btns[] = {"Yes", "No", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, "Factory reset device?", btns, false);
    lv_obj_set_size(mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(mbox, factory_reset_mbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

static void factory_reset_mbox_value_changed_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        lv_msgbox_close(mbox);

        lv_obj_t *resetting_mbox = lv_msgbox_create(NULL, NULL, "Resetting device...", NULL, false);
        lv_obj_set_size(resetting_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_center(resetting_mbox);

        lv_timer_t *timer = lv_timer_create(perform_factory_reset, 500, resetting_mbox);
        lv_timer_set_repeat_count(timer, 1);
    } else {
        lv_msgbox_close(mbox);
    }
}

static void perform_factory_reset(lv_timer_t *timer) {
    lv_obj_t *resetting_mbox = (lv_obj_t *)timer->user_data;
    const char *lvm_device_path = "/dev/droidian/droidian-reserved";
    size_t print_bytes = 64;
    int result = is_lv_encrypted_with_luks(lvm_device_path, print_bytes);

    if (result == -1) {
        // rootfs.img in data? well we can't reset that for now
        lv_msgbox_close(resetting_mbox);
        static const char *btns[] = {"OK", ""};
        lv_obj_t *fail_mbox = lv_msgbox_create(NULL, NULL, "Failed to factory reset", btns, false);
        lv_obj_set_size(fail_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(fail_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(fail_mbox);
    } else {
        if (result == 1) {
            enabling_ssh = false;
            decrypt(); // Decrypt LVM if necessary
            lv_msgbox_close(resetting_mbox);
            return;
        }

        // LVM is not encrypted or unlocked, we can continue
        int factory_reset_result = factory_reset();

        lv_msgbox_close(resetting_mbox);

        if (factory_reset_result == 0) {
            static const char *btns[] = {"OK", ""};
            lv_obj_t *success_mbox = lv_msgbox_create(NULL, NULL, "Successfully reset to factory settings", btns, false);
            lv_obj_set_size(success_mbox, 400, LV_SIZE_CONTENT);
            lv_obj_add_event_cb(success_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(success_mbox);
        } else {
            static const char *btns[] = {"OK", ""};
            lv_obj_t *error_mbox = lv_msgbox_create(NULL, NULL, "Failed to factory reset", btns, false);
            lv_obj_set_size(error_mbox, 400, LV_SIZE_CONTENT);
            lv_obj_add_event_cb(error_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(error_mbox);
        }
    }
}

static void close_mbox_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);

    // maybe do something instead of sleep?
    sleep(3);
    reboot_device();
    lv_msgbox_close(mbox);
}

static void keyboard_value_changed_cb(lv_event_t *event) {
    lv_obj_t *kb = lv_event_get_target(event);

    uint16_t btn_id = lv_btnmatrix_get_selected_btn(kb);
    if (btn_id == LV_BTNMATRIX_BTN_NONE) {
        return;
    }

    if (sq2lv_is_layer_switcher(kb, btn_id)) {
        sq2lv_switch_layer(kb, btn_id);
        return;
    }

    lv_keyboard_def_event_cb(event);
}

static void textarea_ready_cb(lv_event_t *event) {
    if (enabling_ssh) {
        check_password_enable_ssh(lv_event_get_target(event));
    } else {
        check_password_factory_reset(lv_event_get_target(event));
    }
}

static void check_password_enable_ssh(lv_obj_t *textarea) {
    const char *password = lv_textarea_get_text(textarea);
    static int attempt_count = 0;
    int result = mount_luks_lvm_droidian_helper(password);

    if (result == EXIT_SUCCESS) {
        enable_ssh();
        restore_main_screen();
    } else if (result == 2) {
        attempt_count++;
        if (attempt_count >= 3) {
            lv_obj_t *error_mbox = lv_msgbox_create(NULL, NULL, "Maximum password attempt reached.", NULL, false);
            lv_obj_set_size(error_mbox, 400, LV_SIZE_CONTENT);
            lv_obj_center(error_mbox);
        }
    }
}

static void check_password_factory_reset(lv_obj_t *textarea) {
    const char *password = lv_textarea_get_text(textarea);
    static int attempt_count = 0;
    int result = mount_luks_lvm_droidian_helper(password);

    if (result == EXIT_SUCCESS) {
        lv_obj_t *resetting_mbox = lv_msgbox_create(NULL, NULL, "Resetting device...", NULL, false);
        lv_obj_set_size(resetting_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_center(resetting_mbox);
        lv_timer_t *timer = lv_timer_create(factory_reset_password, 500, resetting_mbox);
        lv_timer_set_repeat_count(timer, 1);
    } else if (result == 2) {
        attempt_count++;
        if (attempt_count >= 3) {
            lv_obj_t *resetting_mbox = lv_msgbox_create(NULL, NULL, "Maximum password attempt reached.", NULL, false);
            lv_obj_set_size(resetting_mbox, 400, LV_SIZE_CONTENT);
            lv_obj_center(resetting_mbox);
        }
    }
}

static void factory_reset_password(lv_timer_t *timer) {
    lv_obj_t *resetting_mbox = (lv_obj_t *)timer->user_data;

    int factory_reset_result = factory_reset();

    lv_msgbox_close(resetting_mbox);

    if (factory_reset_result == 0) {
        static const char *btns[] = {"OK", ""};
        lv_obj_t *success_mbox = lv_msgbox_create(NULL, NULL, "Successfully reset to factory settings", btns, false);
        lv_obj_set_size(success_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(success_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(success_mbox);
    } else {
        static const char *btns[] = {"OK", ""};
        lv_obj_t *error_mbox = lv_msgbox_create(NULL, NULL, "Failed to factory reset", btns, false);
        lv_obj_set_size(error_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(error_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(error_mbox);
    }
}

static char* get_slot_suffix() {
    FILE* cmdline = fopen("/proc/cmdline", "r");
    if (cmdline == NULL) {
        perror("Error opening /proc/cmdline");
        return NULL;
    }

    char buffer[1024];
    char* result = NULL;
    if (fgets(buffer, sizeof(buffer), cmdline) != NULL) {
        char* token = strstr(buffer, "androidboot.slot_suffix=");
        if (token != NULL) {
            token += strlen("androidboot.slot_suffix=");
            result = malloc(3 * sizeof(char));
            if (result != NULL) {
                strncpy(result, token, 2);
                result[2] = '\0';
            }
        }
    }

    fclose(cmdline);
    return result;
}

static int drop_caches() {
    int fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (fd == -1) {
        perror("Failed to open /proc/sys/vm/drop_caches");
        return -1;
    }

    if (write(fd, "1", 1) != 1) {
        perror("Failed to write to /proc/sys/vm/drop_caches");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int factory_reset(void) {
    // the reason most things here are system calls is because our ramdisk must be small and more libraries we link against the bigger the binary will get
    // here, we're using pre existing binaries in the ramdisk to not take too much storage in the ramdisk
    struct stat buffer;
    int result;
    char cmd[1024];
    char bootimg_file[256] = "";
    char dtboimg_file[256] = "";
    char* slot_suffix = get_slot_suffix();

    // If no slot suffix is found, default to an empty string so that single slot devices can work
    if (slot_suffix == NULL) {
        slot_suffix = strdup("");
    }

    drop_caches(); // tar will fill up cache, has to be cleared before writing

    if (stat("/dev/disk/by-partlabel/super", &buffer) == 0) {
        // if system_a doesn't exist
        if (stat("/dev/mapper/dynpart-system_a", &buffer) != 0) {
            // if system_b doesn't exist
            if (stat("/dev/mapper/dynpart-system_b", &buffer) != 0) {
                snprintf(cmd, sizeof(cmd), "dmsetup create --concise \"$(parse-android-dynparts /dev/disk/by-partlabel/super)\"");
                system(cmd);
            }
        }
    }

    mkdir("/system_mnt", 0755);
    if (stat("/dev/mapper/dynpart-system_a", &buffer) == 0) {
        result = mount("/dev/mapper/dynpart-system_a", "/system_mnt", "ext4", 0, NULL);
        if (result != 0) {
            printf("Failed to mount dynpart-system_a\n");
            free(slot_suffix);
            return -1;
        }
    } else if (stat("/dev/mapper/dynpart-system_b", &buffer) == 0) {
        result = mount("/dev/mapper/dynpart-system_b", "/system_mnt", "ext4", 0, NULL);
        if (result != 0) {
            printf("Failed to mount dynpart-system_b\n");
            free(slot_suffix);
            return -1;
        }
    } else {
        printf("Failed to mount dynpart-system, block device doesn't not exist\n");
        free(slot_suffix);
        return -1;
    }

    if (stat("/system_mnt/userdata.img.tar.gz", &buffer) == 0) {
        snprintf(cmd, sizeof(cmd), "tar -xzOf /system_mnt/userdata.img.tar.gz | dd of=/dev/disk/by-partlabel/userdata bs=4M");
    } else if (stat("/system_mnt/userdata-raw.img.tar.gz", &buffer) == 0) {
        snprintf(cmd, sizeof(cmd), "tar -xzOf /system_mnt/userdata-raw.img.tar.gz | dd of=/dev/disk/by-partlabel/userdata bs=4M");
    } else {
        printf("Failed to find userdata archive\n");
        umount("/system_mnt");
        free(slot_suffix);
        return -1;
    }

    result = system(cmd);
    if (result != 0) {
        printf("Failed to extract and write userdata\n");
        umount("/system_mnt");
        free(slot_suffix);
        return -1;
    }

    if (stat("/system_mnt/boot.img", &buffer) == 0) {
        snprintf(cmd, sizeof(cmd),
                 "dd if=/system_mnt/boot.img of=/dev/disk/by-partlabel/boot%s bs=4M",
                 slot_suffix);
        result = system(cmd);
        if (result != 0) {
            printf("Failed to flash boot image%s%s\n",
                   *slot_suffix ? " to slot suffix " : "",
                   *slot_suffix ? slot_suffix : "");
        } else {
            printf("Flashed boot.img from /system_mnt\n");
        }
    } else {
        printf("No /system_mnt/boot.img found.\n");
    }

    if (stat("/system_mnt/dtbo.img", &buffer) == 0) {
        snprintf(cmd, sizeof(cmd),
                 "dd if=/system_mnt/dtbo.img of=/dev/disk/by-partlabel/dtbo%s bs=4M",
                 slot_suffix);
        result = system(cmd);
        if (result != 0) {
            printf("Failed to flash dtbo image%s%s\n",
                   *slot_suffix ? " to slot suffix " : "",
                   *slot_suffix ? slot_suffix : "");
        } else {
            printf("Flashed dtbo.img from /system_mnt\n");
        }
    } else {
        printf("No /system_mnt/dtbo.img found.\n");
    }

    if (stat("/system_mnt/boot.img", &buffer) != 0 ||
        stat("/system_mnt/dtbo.img", &buffer) != 0) {
        if (stat("/dev/mapper/droidian-droidian--rootfs", &buffer) == 0) {
            mkdir("/rootfs_mnt", 0755);

            result = mount("/dev/mapper/droidian-droidian--rootfs", "/rootfs_mnt", "ext4",0, NULL);
            if (result != 0) {
                printf("Failed to mount droidian-droidian--rootfs\n");
                return -1;
            }

            DIR *dir = opendir("/rootfs_mnt/boot");
            if (dir == NULL) {
                printf("Failed to opendir /rootfs_mnt/boot\n");
                umount("/rootfs_mnt");
                return -1;
            }

            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strncmp(entry->d_name, "boot.img", strlen("boot.img")) == 0) {
                    strncpy(bootimg_file, entry->d_name, sizeof(bootimg_file) - 1);
                    bootimg_file[sizeof(bootimg_file) - 1] = '\0';
                    break;
                }
            }

            rewinddir(dir);
            while ((entry = readdir(dir)) != NULL) {
                if (strncmp(entry->d_name, "dtbo.img", strlen("dtbo.img")) == 0) {
                    strncpy(dtboimg_file, entry->d_name, sizeof(dtboimg_file) - 1);
                    dtboimg_file[sizeof(dtboimg_file) - 1] = '\0';
                    break;
                }
            }

            closedir(dir);

            if (bootimg_file[0] != '\0') {
                char boot_path[512];
                snprintf(boot_path, sizeof(boot_path), "/rootfs_mnt/boot/%s", bootimg_file);

                snprintf(cmd, sizeof(cmd),
                         "dd if=\"%s\" of=\"/dev/disk/by-partlabel/boot%s\" bs=4M",
                         boot_path, slot_suffix);

                result = system(cmd);
                if (result != 0) {
                    printf("Failed to flash boot image%s%s\n",
                           *slot_suffix ? " to slot suffix " : "",
                           *slot_suffix ? slot_suffix : "");
                } else {
                    printf("Flashed boot.img from /rootfs_mnt\n");
                }
            } else {
                printf("Failed to find boot image in the rootfs\n");
            }

            if (dtboimg_file[0] != '\0') {
                char dtbo_path[512];
                snprintf(dtbo_path, sizeof(dtbo_path), "/rootfs_mnt/boot/%s", dtboimg_file);

                snprintf(cmd, sizeof(cmd),
                         "dd if=\"%s\" of=\"/dev/disk/by-partlabel/dtbo%s\" bs=4M",
                         dtbo_path, slot_suffix);

                result = system(cmd);
                if (result != 0) {
                    printf("Failed to flash dtbo image%s%s\n",
                           *slot_suffix ? " to slot suffix " : "",
                           *slot_suffix ? slot_suffix : "");
                } else {
                    printf("Flashed dtbo.img from /rootfs_mnt\n");
                }
            } else {
                printf("Failed to find dtbo image in the rootfs\n");
            }

            umount("/rootfs_mnt");
        } else {
            printf("No /system_mnt images found and /dev/mapper/droidian-droidian--rootfs not available.\n");
        }
    }

    umount("/system_mnt");
    drop_caches();
    free(slot_suffix);
    return 0;
}

static void restore_main_screen(void) {
    /* Show all main window widgets */
    lv_obj_clear_flag(reboot_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(shutdown_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(factory_reset_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(theme_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(terminal_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ssh_btn, LV_OBJ_FLAG_HIDDEN);

    /* Delete all decrypt screen elements */
    if (decrypt_container != NULL) {
        lv_obj_del(decrypt_container);
        decrypt_container = NULL;
        label_container = NULL;
        spangroup = NULL;
        textarea_container = NULL;
        textarea = NULL;
        toggle_pw_btn = NULL;
        toggle_kb_btn = NULL;
    }

    if (keyboard != NULL) {
        lv_obj_del(keyboard);
        keyboard = NULL;
    }

    /* Re-enable scrolling on main screen */
    lv_obj_add_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
}

static void decrypt(void) {
    uint32_t hor_res = 0;
    uint32_t ver_res = 0;
    uint32_t dpi = 0;

    switch (conf_opts.general.backend) {
#if USE_FBDEV
    case BACKENDS_BACKEND_FBDEV:
        fbdev_get_sizes(&hor_res, &ver_res, &dpi);
        break;
#endif /* USE_FBDEV */
#if USE_DRM
    case BACKENDS_BACKEND_DRM:
        drm_get_sizes((lv_coord_t *)&hor_res, (lv_coord_t *)&ver_res, &dpi);
        break;
#endif /* USE_DRM */
#if USE_MINUI
    case BACKENDS_BACKEND_MINUI:
        minui_get_sizes(&hor_res, &ver_res, &dpi);
        break;
#endif /* USE_MINUI */
    default:
        printf("Unable to find suitable backend\n");
        exit(EXIT_FAILURE);
    }

    is_keyboard_hidden = false;

    /* Prevent scrolling when keyboard is off-screen */
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    /* Figure out a few numbers for sizing and positioning */
    const int keyboard_height = ver_res > hor_res ? ver_res / 3 : ver_res / 2;
    const int padding = keyboard_height / 8;
    const int label_width = hor_res - 2 * padding;
    const int textarea_container_max_width = LV_MIN(hor_res, ver_res);

    /* Hide everything from the main window */
    lv_obj_add_flag(reboot_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shutdown_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(factory_reset_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(theme_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(terminal_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ssh_btn, LV_OBJ_FLAG_HIDDEN);

    /* Main flexbox */
    decrypt_container = lv_obj_create(lv_scr_act());
    lv_obj_set_flex_flow(decrypt_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(decrypt_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(decrypt_container, LV_PCT(100), ver_res - keyboard_height);
    lv_obj_set_pos(decrypt_container, 0, 0);
    lv_obj_set_style_pad_row(decrypt_container, padding, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(decrypt_container, padding, LV_PART_MAIN);

    /* Label container */
    label_container = lv_obj_create(decrypt_container);
    lv_obj_set_size(label_container, label_width, LV_PCT(100));
    lv_obj_set_flex_grow(label_container, 1);

    /* Label */
    spangroup = lv_spangroup_create(label_container);
    lv_spangroup_set_align(spangroup, LV_TEXT_ALIGN_CENTER);
    lv_spangroup_set_mode(spangroup, LV_SPAN_MODE_BREAK);
    lv_spangroup_set_overflow(spangroup, LV_SPAN_OVERFLOW_ELLIPSIS);
    lv_span_t *span1 = lv_spangroup_new_span(spangroup);

    /* Label text */
    const char *label_text = enabling_ssh ?
        "Password required for SSH access" :
        "Password required for factory reset";
    lv_span_set_text(span1, label_text);

    /* Size label to content */
    const lv_coord_t label_height = lv_spangroup_get_expand_height(spangroup, label_width);
    lv_obj_set_style_max_height(spangroup, LV_PCT(100), LV_PART_MAIN);
    lv_obj_set_size(spangroup, label_width, label_height);
    lv_obj_set_align(spangroup, LV_ALIGN_BOTTOM_MID);

    /* Textarea flexbox */
    textarea_container = lv_obj_create(decrypt_container);
    lv_obj_set_size(textarea_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(textarea_container, textarea_container_max_width, LV_PART_MAIN);
    lv_obj_set_flex_flow(textarea_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(textarea_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(textarea_container, padding, LV_PART_MAIN);
    lv_obj_set_style_pad_right(textarea_container, padding, LV_PART_MAIN);

    /* Textarea */
    textarea = lv_textarea_create(textarea_container);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_password_mode(textarea, true);
    lv_textarea_set_password_bullet(textarea, conf_opts.textarea.bullet);
    lv_textarea_set_placeholder_text(textarea, "Enter password...");
    lv_obj_add_event_cb(textarea, textarea_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_set_flex_grow(textarea, 1);
    lv_obj_add_state(textarea, LV_STATE_FOCUSED);

    /* Route physical keyboard input into textarea */
    indev_set_up_textarea_for_keyboard_input(textarea);

    /* Reveal / obscure password button */
    toggle_pw_btn = lv_btn_create(textarea_container);
    const int textarea_height = lv_obj_get_height(textarea);
    lv_obj_set_size(toggle_pw_btn, textarea_height, textarea_height);
    lv_obj_t *toggle_pw_btn_label = lv_label_create(toggle_pw_btn);
    lv_obj_center(toggle_pw_btn_label);
    lv_label_set_text(toggle_pw_btn_label, LV_SYMBOL_EYE_OPEN);
    lv_obj_add_event_cb(toggle_pw_btn, toggle_pw_btn_clicked_cb, LV_EVENT_CLICKED, NULL);

    /* Show / hide keyboard button */
    toggle_kb_btn = lv_btn_create(textarea_container);
    lv_obj_set_size(toggle_kb_btn, textarea_height, textarea_height);
    lv_obj_add_event_cb(toggle_kb_btn, toggle_kb_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *toggle_kb_btn_label = lv_label_create(toggle_kb_btn);
    lv_label_set_text(toggle_kb_btn_label, LV_SYMBOL_KEYBOARD);
    lv_obj_center(toggle_kb_btn_label);

    /* Hide label if it clips vertically */
    if (label_height > lv_obj_get_height(label_container)) {
        lv_obj_set_height(spangroup, 0);
    }

    /* Keyboard (after textarea / label so that key popovers are not drawn over) */
    keyboard = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(keyboard, textarea);
    lv_obj_remove_event_cb(keyboard, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(keyboard, keyboard_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_pos(keyboard, 0, is_keyboard_hidden ? keyboard_height : 0);
    lv_obj_set_size(keyboard, hor_res, keyboard_height);
    theme_prepare_keyboard(keyboard);

    /* Apply textarea options */
    set_password_obscured(conf_opts.textarea.obscured);
}

static void reboot_device(void) {
    sync();
    reboot(RB_AUTOBOOT);
}

static void shutdown(void) {
    sync();
    reboot(RB_POWER_OFF);
}

static void* run_recovery(void* arg) {
    LV_UNUSED(arg);

    char *recovery_args[] = {"/usr/bin/bootman", NULL};
    execv(recovery_args[0], recovery_args);
    perror("execv");
    return NULL;
}

static void open_terminal(void) {
    lv_obj_clean(lv_scr_act());
    lv_deinit();
    if (buf)
        free(buf);

    switch (conf_opts.general.backend) {
#if USE_FBDEV
    case BACKENDS_BACKEND_FBDEV:
        fbdev_exit();
        break;
#endif /* USE_FBDEV */
#if USE_DRM
    case BACKENDS_BACKEND_DRM:
        drm_exit();
        break;
#endif /* USE_DRM */
#if USE_MINUI
    case BACKENDS_BACKEND_MINUI:
        minui_exit();
        break;
#endif /* USE_MINUI */
    }

    terminal_reset_current_terminal();

    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {"/usr/bin/furios-terminal", NULL};
        execv(args[0], args);
    } else {
        waitpid(pid, NULL, 0);
        printf("Terminal exited, reinitializing recovery\n");

        pthread_t recovery_thread;
        int thread_result = pthread_create(&recovery_thread, NULL, run_recovery, NULL);
        if (thread_result != 0) {
            perror("pthread_create");
            exit(1);
        }

        thread_result = pthread_detach(recovery_thread);
        if (thread_result != 0) {
            perror("pthread_detach");
            exit(1);
        }

        sleep(1); // wait for the other instance to start
        exit(0);
    }
}

static void sigaction_handler(int signum) {
    LV_UNUSED(signum);
    terminal_reset_current_terminal();
    exit(0);
}

static void enable_ssh() {
    struct stat buffer;

    if (stat("/tmp/dropbear-enabled", &buffer) == 0) {
        if (stat("/scripts/enable-ssh.sh", &buffer) == 0) {
            system("/scripts/enable-ssh.sh 0");
            if (ip_label_container != NULL) {
                lv_obj_add_flag(ip_label_container, LV_OBJ_FLAG_HIDDEN);
            }
            lv_label_set_text(ssh_btn_label, "Enable SSH");
        }
    } else {
        if (stat("/scripts/enable-ssh.sh", &buffer) == 0) {
            system("/scripts/enable-ssh.sh 1");

            if (ip_label_container == NULL) {
                /* IP Address label container */
                ip_label_container = lv_obj_create(lv_scr_act());
                lv_obj_set_width(ip_label_container, LV_PCT(100));
                lv_obj_set_height(ip_label_container, LV_SIZE_CONTENT);
                lv_obj_align(ip_label_container, LV_ALIGN_BOTTOM_MID, 0, -50);

                /* IP Address label text */
                ip_label = lv_label_create(ip_label_container);
                lv_label_set_text(ip_label, "IP Address: 192.168.2.15");
                lv_obj_align(ip_label, LV_ALIGN_BOTTOM_MID, 0, 0);
            } else {
                lv_obj_clear_flag(ip_label_container, LV_OBJ_FLAG_HIDDEN);
            }
            lv_label_set_text(ssh_btn_label, "Disable SSH");
        }
    }
}

static void toggle_ssh_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);

    const char *lvm_device_path = "/dev/droidian/droidian-reserved";
    size_t print_bytes = 64;
    int result = is_lv_encrypted_with_luks(lvm_device_path, print_bytes);
    if (result == 1) {
        enabling_ssh = true;
        decrypt();
    } else {
        enable_ssh();
    }
}

static int read_int_from_file(const char *path, int default_value) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("File not found: %s\n", path);
        return default_value;
    }

    char buffer[20];
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        fclose(file);
        buffer[strcspn(buffer, "\n")] = 0;

        char *endptr;
        long value = strtol(buffer, &endptr, 10);
        if (*endptr == '\0' && value >= 0) {
            return (int)value;
        }
    }

    fclose(file);
    return default_value;
}

static int write_int_to_file(const char *path, int value) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        printf("Failed to open file for writing: %s (Error: %s)\n", path, strerror(errno));
        return -1;
    }

    int result = fprintf(file, "%d", value);
    fclose(file);

    if (result < 0) {
        printf("Failed to write to file: %s (Error: %s)\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

static void create_buttons(lv_obj_t *label_container) {
    /* Brightness slider */
    brightness_slider = lv_slider_create(label_container);
    lv_obj_set_width(brightness_slider, LV_PCT(100));
    lv_obj_set_height(brightness_slider, 20);

    int max_brightness = read_int_from_file(MAX_BRIGHTNESS_PATH, 255);
    lv_slider_set_range(brightness_slider, 0, max_brightness);

    int current_brightness = read_int_from_file(BRIGHTNESS_PATH, max_brightness);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);

    lv_obj_add_event_cb(brightness_slider, brightness_slider_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 520);

    static lv_style_t style_slider;
    lv_style_init(&style_slider);
    lv_style_set_bg_color(&style_slider, lv_color_hex(0x888888));
    lv_style_set_bg_opa(&style_slider, LV_OPA_100);
    lv_obj_add_style(brightness_slider, &style_slider, LV_PART_MAIN);

    static lv_style_t style_indicator;
    lv_style_init(&style_indicator);
    lv_style_set_bg_color(&style_indicator, lv_color_hex(0x00ff00));
    lv_style_set_bg_opa(&style_indicator, LV_OPA_100);
    lv_obj_add_style(brightness_slider, &style_indicator, LV_PART_INDICATOR);

    static lv_style_t style_knob;
    lv_style_init(&style_knob);
    lv_style_set_bg_color(&style_knob, lv_color_hex(0xffffff));
    lv_style_set_bg_opa(&style_knob, LV_OPA_100);
    lv_style_set_border_color(&style_knob, lv_color_hex(0x000000));
    lv_style_set_border_width(&style_knob, 2);
    lv_style_set_radius(&style_knob, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&style_knob, 5);
    lv_obj_add_style(brightness_slider, &style_knob, LV_PART_KNOB);

    /* Brightness label */
    lv_obj_t *brightness_label = lv_label_create(label_container);
    lv_label_set_text(brightness_label, "Brightness control");
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    /* Reboot button */
    reboot_btn = lv_btn_create(label_container);
    lv_obj_set_width(reboot_btn, LV_PCT(100));
    lv_obj_set_height(reboot_btn, 100);
    lv_obj_t *reboot_btn_label = lv_label_create(reboot_btn);
    lv_label_set_text(reboot_btn_label, "Reboot");
    lv_obj_add_event_cb(reboot_btn, reboot_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(reboot_btn, LV_ALIGN_TOP_MID, 0, 600);
    lv_obj_set_flex_flow(reboot_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(reboot_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Shutdown button */
    shutdown_btn = lv_btn_create(label_container);
    lv_obj_set_width(shutdown_btn, LV_PCT(100));
    lv_obj_set_height(shutdown_btn, 100);
    lv_obj_t *shutdown_btn_label = lv_label_create(shutdown_btn);
    lv_label_set_text(shutdown_btn_label, "Shutdown");
    lv_obj_add_event_cb(shutdown_btn, shutdown_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(shutdown_btn, LV_ALIGN_TOP_MID, 0, 700);
    lv_obj_set_flex_flow(shutdown_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(shutdown_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Factory reset button */
    factory_reset_btn = lv_btn_create(label_container);
    lv_obj_set_width(factory_reset_btn, LV_PCT(100));
    lv_obj_set_height(factory_reset_btn, 100);
    lv_obj_t *factory_reset_btn_label = lv_label_create(factory_reset_btn);
    lv_label_set_text(factory_reset_btn_label, "Factory Reset");
    lv_obj_add_event_cb(factory_reset_btn, factory_reset_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(factory_reset_btn, LV_ALIGN_TOP_MID, 0, 800);
    lv_obj_set_flex_flow(factory_reset_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(factory_reset_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Theme button */
    theme_btn = lv_btn_create(label_container);
    lv_obj_set_width(theme_btn, LV_PCT(100));
    lv_obj_set_height(theme_btn, 100);
    lv_obj_t *theme_btn_label = lv_label_create(theme_btn);
    lv_label_set_text(theme_btn_label, "Toggle Theme");
    lv_obj_add_event_cb(theme_btn, toggle_theme_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(theme_btn, LV_ALIGN_TOP_MID, 0, 900);
    lv_obj_set_flex_flow(theme_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(theme_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Terminal button */
    terminal_btn = lv_btn_create(label_container);
    lv_obj_set_width(terminal_btn, LV_PCT(100));
    lv_obj_set_height(terminal_btn, 100);
    lv_obj_t *terminal_btn_label = lv_label_create(terminal_btn);
    lv_label_set_text(terminal_btn_label, "Terminal");
    lv_obj_add_event_cb(terminal_btn, terminal_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(terminal_btn, LV_ALIGN_TOP_MID, 0, 1000);
    lv_obj_set_flex_flow(terminal_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(terminal_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* SSH button */
    ssh_btn = lv_btn_create(label_container);
    lv_obj_set_width(ssh_btn, LV_PCT(100));
    lv_obj_set_height(ssh_btn, 100);
    ssh_btn_label = lv_label_create(ssh_btn);

    struct stat buffer;
    if (stat("/tmp/dropbear-enabled", &buffer) == 0)
        lv_label_set_text(ssh_btn_label, "Disable SSH");
    else
        lv_label_set_text(ssh_btn_label, "Enable SSH");

    lv_obj_add_event_cb(ssh_btn, toggle_ssh_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(ssh_btn, LV_ALIGN_TOP_MID, 0, 1100);
    lv_obj_set_flex_flow(ssh_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ssh_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

static void create_ui(uint32_t hor_res, uint32_t ver_res) {
    /* Clear the screen */
    lv_obj_clean(lv_scr_act());

    /* Prevent scrolling when keyboard is off-screen */
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

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
    lv_label_set_text(furios_label, "FuriOS Recovery");
    lv_obj_align(furios_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Initialize images */
    for (int i = 0; i < NUM_IMAGES; i++)
        images[i] = lv_img_create(lv_scr_act());

    /* Furi Labs logo */
    lv_obj_align(images[0], LV_ALIGN_TOP_MID, 0, 100);

    /* Set image mode */
    update_image_mode(is_alternate_theme);

    /* Create buttons */
    create_buttons(label_container);
}

static void initialize_recovery_ui(void) {
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
}

/**
 * Main
 */

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

    initialize_recovery_ui();

    /* Run lvgl in "tickless" mode */
    while(1) {
        lv_task_handler();
        usleep(5000);
    }

    return 0;
}


/**
 * Tick generation
 */

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
