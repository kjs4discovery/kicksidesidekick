/* Licence: TBD (see D017) */
/* fm1-accompaniment: the app's half of the contract with fm1-base (vendor/fm1-base/device/unity.c,
 * host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "Notami - SongSidekick"   /* M8 (D050): the splash, the help, fm1-base's crash / safe-mode
                                                 * screens; NOT the USB / MIDI names or the identity (FM-1_4600xxxx) */
#define FW_MILESTONE "m8"                /* M8: the splash shows only this under the name */
#define FW_VERSION "0.8.0-m8"             /* ours (M5 part builds 0.5.N-m5pN; M5.1 0.5.4-m5p4; M6 0.6.0-m6; M7 0.7.0-m7; M8) */
#define APP_SPLASH_RGB RGB(64, 160, 220)
#define APP_SPLASH                       /* our splash: the logo (ui.c app_splash; tools/logo_to_c.py) */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz; one EAS buffer (synth.h) */

/* the store: our settings (none yet) and the panel calibration (the platform's), and their flash offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

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
