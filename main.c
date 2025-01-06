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

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/reboot.h>

/**
 * Static variables
 */

cli_opts cli_options;
config_opts conf_opts;

static lv_color_t *buf = NULL;
static lv_disp_draw_buf_t disp_buf;

bool is_alternate_theme = true;

/* Main page */
lv_obj_t *reboot_btn;
lv_obj_t *shutdown_btn;

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
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        shutdown();
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
    terminal_reset_current_terminal();
    exit(0);
}

static void create_buttons(lv_obj_t *label_container) {
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
}

static void create_ui(uint32_t hor_res, uint32_t ver_res) {
    /* Clear the screen */
    lv_obj_clean(lv_scr_act());

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
