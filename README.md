FuriOS Recovery
===============

Recovery project for the initramfs based on [LVGL].

# Usage

```
$ bootman --help
Usage: bootman [OPTION]

Mandatory arguments to long options are mandatory for short options too.
  -c, --config=PATH      Locaton of the main config file. Defaults to
                         /etc/bootman.conf.
  -C, --config-override  Location of the config override file. Values in
                         this file override values for the same keys in the
                         main config file. If specified multiple times, the
                         values from consecutive files will be merged in
                         order.
  -g, --geometry=NxM     Force a display size of N horizontal times M
                         vertical pixels
  -d  --dpi=N            Overrides the DPI
  -h, --help             Print this message and exit
  -v, --verbose          Enable more detailed logging output on STDERR
  -V, --version          Print the bootman version and exit
```

For an example configuration file, see [bootman].

# Development

## Dependencies

- [inih]
- [lvgl] (git submodule / linked statically)
- [lv_drivers] (git submodule / linked statically)
- [squeek2lvgl] (git submodule / linked statically)
- [libinput]
- [libxkbcommon]
- [libdrm] (optional, required for the DRM backend)
- evdev kernel module

## Building & running

Some of FuriOS Recovery's dependencies are included as git submodules in this repository. You can clone the repository and initialise the submodules with

```
$ git clone https://github.com/furilabs/bootman.git
$ cd bootman
$ git submodule init
$ git submodule update
```

When pulling changes from the remote later, either use `git pull --recurse-submodules` or manually run `git submodule update` as needed after pulling.

Once you have the sources, you can build the app and run it in a VT. Unless your user account has special privileges, `sudo` will be needed to access input device files.

```
$ meson _build
$ meson compile -C _build
$ sudo chvt 2
$ sudo ./_build/bootman
```

With meson <0\.55 use `ninja` instead of `meson compile`\.

### Optional features

If [libdrm] is installed, the DRM backend will be compiled automatically. It's possible to
change this behaviour using the `with-drm` meson feature. For example,

```
$ meson _build -Dwith-drm=disabled
```

will forcibly disable the DRM backend regardless if libdrm is installed or not.

## Backends

FuriOS Recovery supports multiple lvgl display drivers, which are herein referred as "backends".

Currently supported backends:

- fbdev
- drm (optional)
- minui (optional)

The backend can be switched at runtime by modifying the `general.backend` configuration.

## Fonts

In order to work with [LVGL], fonts need to be converted to bitmaps, stored as C arrays. FuriOS Recovery currently uses a combination of the [OpenSans] font for text and the [FontAwesome] font for pictograms. For both fonts only limited character ranges are included to reduce the binary size. To (re)generate the C file containing the combined font, run the following command

```
$ ./regenerate-fonts.sh
```

Below is a short explanation of the different unicode ranges used above.

- [OpenSans]
  - Basic Latin (`0x0020-0x007F`)
  - Latin-1 supplement (`0x00A0-0x00FF`)
  - Latin extended A (`0x0100-0x017F`)
  - Greek and Coptic (`0x0370-0x03FF`)
  - General punctuation (`0x2000-0x206F`)
  - Currency symbols (`0x20A0-0x20CF`)
  - Mathematical operators (`0x2200-0x22FF`)
- [FontAwesome]
  - Standard `LV_SYMBOL_*` glyphs (`0xF001,0xF008,0xF00B,0xF00C,0xF00D,0xF011,0xF013,0xF015,0xF019,0xF01C,0xF021,0xF026,0xF027,0xF028,0xF03E,0xF0E0,0xF304,0xF043,0xF048,0xF04B,0xF04C,0xF04D,0xF051,0xF052,0xF053,0xF054,0xF067,0xF068,0xF06E,0xF070,0xF071,0xF074,0xF077,0xF078,0xF079,0xF07B,0xF093,0xF095,0xF0C4,0xF0C5,0xF0C7,0xF0C9,0xF0E7,0xF0EA,0xF0F3,0xF11C,0xF124,0xF158,0xF1EB,0xF240,0xF241,0xF242,0xF243,0xF244,0xF287,0xF293,0xF2ED,0xF55A,0xF7C2,0xF8A2`)
  - [adjust](https://fontawesome.com/v5/icons/adjust) (`0xF042`)
  - [arrow-alt-circle-up](https://fontawesome.com/v5/icons/arrow-alt-circle-up) (`0xF35B`)
  - [chevron-left](https://fontawesome.com/v5/icons/chevron-left) (`0xF053`)

## Keyboard layouts

FuriOS Recovery uses [squeekboard layouts] converted to C via [squeek2lvgl]. To regenerate the layouts, ensure that you have pipenv installed (e.g. via `pip install --user pipenv`) and then run

```
$ ./regenerate-layouts.sh
```

from the root of the repository.

# License

FuriOS Recovery is licensed under the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

The [OpenSans] font is licensed under the Apache License 2.0.

The [FontAwesome] font is licensed under the Open Font License version 1.1.

[Add full keyboard support to libinput/evdev driver]: https://github.com/lvgl/lv_drivers/pull/156
[Add support for keypads to libinput driver]: https://github.com/lvgl/lv_drivers/pull/152
[Add support for pointer devices to libinput driver]: https://github.com/lvgl/lv_drivers/pull/150
[Automatic device discovery via libinput]: https://github.com/lvgl/lv_drivers/pull/157
[FontAwesome]: https://fontawesome.com
[LVGL]: https://lvgl.io
[Make it possible to use multiple devices with the libinput and XKB drivers]: https://github.com/lvgl/lv_drivers/pull/165
[OpenSans]: https://fonts.google.com/specimen/Open+Sans
[Use LV_LOG instead of printf in fbdev driver]: https://github.com/lvgl/lv_drivers/pull/167
[adjust]: https://fontawesome.com/v5.15/icons/adjust?style=solid
[arrow-alt-circle-up]: https://fontawesome.com/v5.15/icons/arrow-alt-circle-up?style=solid
[feat(btnmatrix): add option to show popovers on button press]: https://github.com/lvgl/lvgl/pull/2537
[feat(msgbox): add function to get selected button index]: https://github.com/lvgl/lvgl/pull/2538
[feat(msgbox): omit title label unless needed]: https://github.com/lvgl/lvgl/pull/2539
[fix(btnmatrix): make ORed values work correctly with lv_btnmatrix_has_btn_ctrl]: https://github.com/lvgl/lvgl/pull/2571
[fix(examples) don't compile assets unless needed]: https://github.com/lvgl/lvgl/pull/2523
[inih]: https://github.com/benhoyt/inih
[libinput]: https://gitlab.freedesktop.org/libinput/libinput
[libxkbcommon]: https://github.com/xkbcommon/libxkbcommon
[libdrm]: https://gitlab.freedesktop.org/mesa/drm
[lv_drivers]: https://github.com/lvgl/lv_drivers
[lv_port_linux_frame_buffer]: https://github.com/lvgl/lv_port_linux_frame_buffer
[lv_sim_emscripten]: https://github.com/lvgl/lv_sim_emscripten/blob/master/mouse_cursor_icon.c
[lvgl]: https://github.com/lvgl/lvgl
[online font converter]: https://lvgl.io/tools/fontconverter
[open issues]: https://github.com/furilabs/bootman/-/issues
[osk-sdl]: https://gitlab.com/postmarketOS/osk-sdl
[screenshots]: ./screenshots
[squeek2lvgl]: https://gitlab.com/cherrypicker/squeek2lvgl
[squeekboard layouts]: https://gitlab.gnome.org/World/Phosh/squeekboard/-/tree/master/data/keyboards
[bootman.conf]: ./bootman.conf
