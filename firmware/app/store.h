/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The song store (M2 part 1; M5 part 2: blocks): compiled songs (.fm1s) in the flash data area, sent from the laptop
 * (tools/fm1song send). docs/song-format.md section 4 is the layout.
 *
 * The area is 0x97000..0xDBFFF (69 sectors of 4 KiB): fm1-base's data area below the panel calibration (app.h
 * APP_STORE_MAP 0xDC000). M-VAVE's stock firmware keeps nothing there (our unit's dump: 0x97000..0xDFFFF all 0xFF;
 * its own data is at 0x93000 and 0xE9000..0xFBFFF).
 *
 * M5 (D029 B): the area is 552 blocks of 512 bytes. Each song is one record: a 32-byte head (magic "FM2R", a sequence
 * number, the file's length and CRC-32, the head's own CRC-32 over all but the magic) and the file right after it,
 * on whole blocks. A record of up to 8 blocks stays inside one sector, so several songs share a sector (a 2.0 song
 * is about 1-3 blocks); a longer one starts on a sector and has its sectors to itself. A record is deleted by
 * writing its magic to zero (NOR: bits only go from 1 to 0), so a delete erases nothing; the room is taken back by
 * compacting when a song needs it: a sector with no live song is erased, and the songs of a sector in the way are
 * first copied elsewhere (the copy whole, then the original deleted). M4's records (magic "FM1R", whole sectors)
 * are still read, so an M4 unit's songs play until they are sent again.
 *
 * Safe against a power cut at any moment: a record counts only once its head is written, and the head is written
 * last; a newer copy of a song (same slug, a higher sequence number) is written before the older one is deleted. So
 * a cut leaves every other song as it was, and the song being sent either as it was, new, or (only when the area had
 * no room for both copies) gone. A copy made while compacting is the same song: either copy is fine. Reading never
 * writes: the scan at the song list writes nothing, and every record is checked again in full (song_open) when it is
 * first used.
 *
 * A song is received straight into the flash (M5 D029 A: no RAM copy, so no 64 KiB limit): store_begin finds and
 * prepares its room, store_write puts the file there piece by piece, store_commit writes the head once the file has
 * been checked in place. Before the commit the record does not count: a cut or a refused file leaves only blocks that
 * the next compaction takes back.
 *
 * Standalone C like song.c: the flash comes from three functions the includer gives (store_map, store_erase,
 * store_prog), so tools/store_test.c runs it on the laptop with simulated power cuts. Every erase and write goes
 * through the area check first: nothing outside the area is ever touched (store_refused counts the attempts: 0). */
#pragma once
#include <stdint.h>

#define STORE_LO 0x97000u
#define STORE_HI 0xDC000u
#define STORE_SECTOR 4096u
#define STORE_SECTORS ((STORE_HI - STORE_LO) / STORE_SECTOR)   /* 69 */
#define STORE_BLOCK 512u
#define STORE_SECTOR_BLOCKS (STORE_SECTOR / STORE_BLOCK)       /* 8 */
#define STORE_BLOCKS (STORE_SECTORS * STORE_SECTOR_BLOCKS)     /* 552 */
#define STORE_HEAD 32u
#define STORE_FILE_MAX (256u * 1024u)      /* a song file's size, at most (song.h SONG_MAX_SIZE; M4: 64 KiB in RAM) */
#define STORE_MAX_SONGS 200u               /* songs listed, at most (M4: 64; the 69 sectors were the real cap) */
#define STORE_MAX_RECORDS STORE_BLOCKS     /* records (stale copies too): every one fits in the table */

enum {
    STORE_BUSY = 1,                       /* M5.1: room is being made (store_step): ask again when it is done */
    STORE_OK = 0,
    STORE_E_FULL = -1,                    /* no room, even after compacting and after the older copy of the song went */
    STORE_E_SIZE = -2,                    /* longer than STORE_FILE_MAX */
    STORE_E_ERASE = -3,                   /* an erase failed (or did not leave 0xFF) */
    STORE_E_WRITE = -4,                   /* a write failed (or did not read back) */
    STORE_E_NOT_FOUND = -5,
    STORE_E_RANGE = -6,                   /* outside the area or the room given: refused (a bug if ever seen) */
    STORE_E_NO_FLASH = -7,                /* the platform refused the flash chip: RAM only */
    STORE_E_STATE = -8,                   /* store_write / store_commit without store_begin */
};

struct store_entry {
    uint32_t off;                         /* the record's flash offset (its head) */
    uint32_t len, crc, seq;               /* the file's */
    uint32_t blocks;                      /* the blocks it takes */
    uint32_t sectors;                     /* (the same, in sectors, rounded up: for the list) */
    const char *slug;                     /* in the file's META, in the flash ("" if the file does not even peek);
                                           * valid until the store next changes */
};

/* the includer's flash (device: plat_data*; laptop test: an array). Offsets are flash offsets. */
const uint8_t *store_map(uint32_t off);                 /* read in place; 0: no flash */
int store_erase(uint32_t off);                          /* one 4 KiB sector; 0 = done */
int store_prog(uint32_t off, const void *src, uint32_t n);   /* src in RAM; bits only cleared; 0 = done */
void store_yield(void);                                 /* M5.1: called in every longer loop (the device: the watchdog
                                                         * fed); nothing in the store runs long without it */

int store_scan(void);                     /* rebuild the table: the number of songs, < 0 = STORE_E_NO_FLASH */
int store_open(void);                     /* M5.1: the same, but scanned only if not yet (every change keeps the table
                                           * up to date: no rescans) */
uint32_t store_count(void);
const struct store_entry *store_get(uint32_t i);   /* 0 .. count-1, in flash order */
const uint8_t *store_file(const struct store_entry *e);   /* the file, read in place */
/* receiving: room for a file of len bytes (slug: its song, whose older copy may make way; "" none) -> STORE_OK and
 * *file the flash offset the file goes to; then store_write the whole file (off from 0), check it in place
 * (store_map(*file)), and store_commit, or store_abort if it is refused */
int store_begin(uint32_t len, const char *slug, uint32_t *file);   /* M5.1: or STORE_BUSY: room being made */
int store_step(void);                     /* M5.1: one step of making room: STORE_BUSY (more to do), STORE_OK (done) */
int store_busy(uint32_t *done, uint32_t *total);   /* M5.1: 1 while room is being made; its steps done / about all */
void store_stop(void);                    /* M5.1: stop making room (what is done stays done; safe at any step) */
int store_wipe_sector(uint32_t sec);      /* M5.1: song store sector sec (0..STORE_SECTORS-1) erased: WIPE SONG STORE */
int store_write(uint32_t off, const void *src, uint32_t n);   /* src in RAM */
int store_commit(void);
void store_abort(void);
int store_add(const uint8_t *file, uint32_t len, const char *slug);   /* all of it at once: the file in RAM */
int store_delete(const char *slug);
uint32_t store_free_blocks(void);         /* free now, or taken back by compacting (deleted songs, leftovers) */
uint32_t store_free_sectors(void);        /* (the same in sectors, rounded down: M4's LIST field) */
uint32_t store_largest_free(void);        /* the longest run of sectors with no song in them */
const char *store_error_name(int err);
extern uint32_t store_refused;            /* erases / writes refused for being outside the area (0) */
extern uint32_t store_compactions;        /* sectors erased by compacting, songs moved (for the tests) */
extern uint32_t store_moves;
