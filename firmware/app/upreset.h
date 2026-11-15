/* SPDX-License-Identifier: GPL-3.0-or-later */
/* M10 (D040, D062): the user presets: 30 slots (U01..U30) of the M7 preset record (vsynth.h struct vs_preset, 64 bytes,
 * versioned), kept in ONE fm1-base store object (OBJ_PRESETS: an A/B sector pair at 0xDE000-0xDFFFF, torn-write safe, CRC,
 * header written last; at most PLAT_STORE_MAX = 3,840 bytes: the object is 1,928).
 *
 * The object is built in RAM (the M5 lesson: never written from a const table) and written only by upreset_save.
 * Start-up (upreset_boot) only READS: no object, a torn write, a bad CRC, junk from another firmware, a wrong length ->
 * all 30 slots EMPTY, no crash, nothing written. The stock presets 1-30 can never be written or replaced.
 *
 * The preset LIST (SYNTH's PRESET knob, BACKTRACK's K4): the 30 stock presets, then the user slots that are not empty, in
 * slot order. */
#pragma once
#include <stdint.h>
#include "vsynth.h"

#define UP_SLOTS 30u
#define UP_REC VS_REC_BYTES                /* 64 */
#define UP_HEAD 8u                         /* u32 magic "UPRS", u8 container version, u8 slots (30), u16 0 */
#define UP_BYTES (UP_HEAD + UP_SLOTS * UP_REC)   /* 1,928 */
#define UP_MAGIC 0x53525055u
#define UP_VERSION 1u

/* the container (pure, host-tested): slots[s][0] = the record's version byte, 0 = EMPTY */
void up_empty(uint8_t slots[UP_SLOTS][UP_REC]);
uint32_t up_pack(uint8_t *dst, const uint8_t slots[UP_SLOTS][UP_REC]);              /* UP_BYTES */
int up_unpack(uint8_t slots[UP_SLOTS][UP_REC], const uint8_t *src, int32_t n);       /* 1: ours; 0: not (all empty) */

/* the device */
void upreset_boot(void);                   /* read the object (never writes): the slots in RAM */
int upreset_save(uint32_t slot, const struct vs_preset *p);   /* the slot's record, then the object (0 ok, else the store's error);
                                                                * the RAM copy changes only when the write succeeded */
#define UP_FLASH_LO 0xDE000u               /* M10.1: the object's two sectors (an A/B pair): 0xDE000, 0xDF000 (app.h APP_STORE_MAP) */
#define UP_FLASH_SECTORS 2u
void upreset_clear_ram(void);              /* M10.1: FACTORY RESET erased the object: all 30 slots empty in RAM (writes nothing) */
int upreset_used(uint32_t slot);           /* 1: the slot holds a preset */
uint32_t upreset_count(void);              /* slots in use */
void upreset_name(uint32_t slot, char *d); /* its name (>= 13 bytes), "" for an empty slot */

/* the list: stock 0..29, then the used user slots in slot order */
uint32_t pl_count(void);
const struct vs_preset *pl_get(uint32_t i);      /* valid until the next call */
int32_t pl_slot(uint32_t i);                     /* -1 stock, else the user slot */
void pl_label(uint32_t i, char *d);              /* "E-PIANO", "U05 NAME" (d >= 20 bytes) */
uint32_t pl_index_of_slot(uint32_t slot);        /* the list index of a used slot (0 if not used) */
uint32_t pl_ref(uint32_t i);                     /* M10.1: a reference to the sound at list index i: its stock position, or 0x100 + its user slot */
uint32_t pl_index_of_ref(uint32_t ref);          /* ... and back to the list index now (a new save or another order cannot shift what it names) */
