/* Licence: TBD (see D017) */
/* The song store (M2 part 1): compiled songs (.fm1s) in the flash data area, sent from the laptop
 * (tools/fm1song send). docs/song-format.md section 4 is the layout.
 *
 * The area is 0x97000..0xDBFFF (69 sectors of 4 KiB): fm1-base's data area below the panel calibration (app.h
 * APP_STORE_MAP 0xDC000). M-VAVE's stock firmware keeps nothing there (our unit's dump: 0x97000..0xDFFFF all 0xFF;
 * its own data is at 0x93000 and 0xE9000..0xFBFFF). Each song is one record on whole sectors: a 32-byte head (magic
 * "FM1R", a sequence number, the file's length and CRC-32, the head's own CRC-32) and the file right after it.
 *
 * Safe against a power cut at any moment: a record counts only once its head is written, and the head is written
 * last; a newer copy of a song (same slug) is written before the older one is erased, and a record is erased head
 * first. So a cut leaves every other song as it was, and the song being sent either as it was, new, or (only when
 * the area had no room for both copies) gone. Reading never writes: the scan at the song list writes nothing, and
 * every record is checked again in full (song_open) when it is first used.
 *
 * Standalone C like song.c: the flash comes from three functions the includer gives (store_map, store_erase,
 * store_prog), so tools/store_test.c runs it on the laptop with simulated power cuts. Every erase and write goes
 * through store_in_area first: nothing outside the area is ever touched (store_refused counts the attempts: 0). */
#pragma once
#include <stdint.h>

#define STORE_LO 0x97000u
#define STORE_HI 0xDC000u
#define STORE_SECTOR 4096u
#define STORE_SECTORS ((STORE_HI - STORE_LO) / STORE_SECTOR)   /* 69 */
#define STORE_HEAD 32u
#define STORE_FILE_MAX (64u * 1024u)       /* a song file's size, at most (the receive buffer) */
#define STORE_MAX_SONGS 64u

enum {
    STORE_OK = 0,
    STORE_E_FULL = -1,                    /* no room, even after the older copy of the song went */
    STORE_E_SIZE = -2,                    /* longer than STORE_FILE_MAX */
    STORE_E_ERASE = -3,                   /* an erase failed (or did not leave 0xFF) */
    STORE_E_WRITE = -4,                   /* a write failed (or did not read back) */
    STORE_E_NOT_FOUND = -5,
    STORE_E_RANGE = -6,                   /* outside the area: refused (a bug if ever seen) */
    STORE_E_NO_FLASH = -7,                /* the platform refused the flash chip: RAM only */
};

struct store_entry {
    uint32_t off;                         /* the record's flash offset (its head) */
    uint32_t len, crc, seq;               /* the file's */
    uint32_t sectors;
    char slug[24];                        /* from the file's META ("" if the file does not even peek) */
};

/* the includer's flash (device: plat_data*; laptop test: an array). Offsets are flash offsets. */
const uint8_t *store_map(uint32_t off);                 /* read in place; 0: no flash */
int store_erase(uint32_t off);                          /* one 4 KiB sector; 0 = done */
int store_prog(uint32_t off, const void *src, uint32_t n);   /* into erased flash; 0 = done */

int store_scan(void);                     /* rebuild the table: the number of songs, < 0 = STORE_E_NO_FLASH */
uint32_t store_count(void);
const struct store_entry *store_get(uint32_t i);   /* 0 .. count-1, in flash order */
const uint8_t *store_file(const struct store_entry *e);   /* the file, read in place */
int store_add(const uint8_t *file, uint32_t len, const char *slug);   /* file in RAM, checked by the caller */
int store_delete(const char *slug);
uint32_t store_free_sectors(void);
uint32_t store_largest_free(void);        /* the longest run of free sectors */
const char *store_error_name(int err);
extern uint32_t store_refused;            /* erases / writes refused for being outside the area (0) */
