/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The firmware has no C library headers: what EAS calls, as felucca-bsp's chip/libc.c defines it (sizes as
 * unsigned) plus strcmp (eas/eas_host_fw.c). For the firmware build only (fm1.toml device_include); the host
 * simulator uses the system's. */
#ifndef FW_STRING_H
#define FW_STRING_H
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);
int strcmp(const char *a, const char *b);
#endif
