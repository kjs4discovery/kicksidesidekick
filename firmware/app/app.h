/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * KickSide is free software (GPL-3.0-or-later). If you are working on a new version of this tool, or building
 * something on it, I would love to hear what you are doing with it. Contact: websiteInfo@KickSideSideKick.com,
 * or through the KickSide website, https://kicksidesidekick.com
 */
/* fm1-accompaniment: the app's half of the contract with fm1-base (vendor/fm1-base/device/unity.c,
 * host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "KickSide - Your Musical SideKick"   /* M8 (D050; M10, D063; M10.1: renamed): the splash, the help, fm1-base's crash / safe-mode
                                                 * screens; NOT the USB / MIDI names or the identity (FM-1_58xxxxxx) */
#define FW_MILESTONE "m10.4"              /* M8, M9, M10: the splash shows only this under the name */
#define FW_VERSION "0.10.6-m10.4"           /* ours (M5 part builds 0.5.N-m5pN; M5.1 0.5.4-m5p4; M6 0.6.0-m6; M7 0.7.0-m7; M8 0.8.0-m8; M9 0.9.0-m9; M10) */
#define HINT_PAGE_MS 6000u               /* M10.1: the bottom instruction row's pages alternate, each shown this long (was 3 s) */
#define APP_SPLASH_RGB RGB(64, 160, 220)
#define APP_SPLASH                       /* our splash: the logo (ui.c app_splash; tools/logo_to_c.py) */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz; one EAS buffer (synth.h) */

/* the store: our settings (none yet) and the panel calibration (the platform's), and their flash offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_PRESETS, OBJ_NOBJ };   /* M10 (D040): OBJ_PRESETS, the user presets (upreset.c) */
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}   /* (0xDE000..0xDFFFF: nothing else uses it; the update staging area
                                                          * starts at 0xE0000; the song store ends at 0xDBFFF) */

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the synth */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */
void app_splash(void);                   /* the first screen at power-on (fm1-base APP_SPLASH) */
#ifndef PLAT_HOST
#define APP_USB_CMD                      /* the laptop's song commands (library.c: fm1song send / list / delete) */
void app_usb_cmd(const uint8_t *d, uint32_t n);
#endif
