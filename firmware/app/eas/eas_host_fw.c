/* Licence: TBD (see D017) */
/* The host layer Sonivox EAS asks of its platform (host_src/eas_host.h), for the FM-1: memory from a fixed pool,
 * files that are buffers (a MIDI file in the flash, read through its EAS_FILE locator), no debug output. Written for
 * this project; upstream's host_src/eas_hostmm.c (a desktop host with malloc and stdio) was the reference for what
 * each call must do. */
#include <stdarg.h>
#include "eas.h"
#include "eas_host.h"
#include "eas_report.h"

#ifndef FW_EAS_POOL_BYTES
#define FW_EAS_POOL_BYTES (40u * 1024u)    /* EAS takes what it needs once, at EAS_Init and when a stream opens */
#endif

static uint32_t pool[FW_EAS_POOL_BYTES / 4u];
static uint32_t pool_used;              /* bytes handed out */

uint32_t fw_eas_pool_used(void) { return pool_used; }
uint32_t fw_eas_pool_size(void) { return FW_EAS_POOL_BYTES; }

EAS_RESULT EAS_HWInit(EAS_HW_DATA_HANDLE *hwInstData)
{
    *hwInstData = 0;                    /* one instance: no per-instance data */
    return EAS_SUCCESS;
}

EAS_RESULT EAS_HWShutdown(EAS_HW_DATA_HANDLE hwInstData)
{
    (void)hwInstData;
    pool_used = 0;                      /* everything goes at once; nothing is freed on its own */
    return EAS_SUCCESS;
}

void *EAS_HWMemSet(void *s, int c, EAS_I32 n)
{
    uint8_t *p = (uint8_t *)s;
    while (n-- > 0)
        *p++ = (uint8_t)c;
    return s;
}

void *EAS_HWMemCpy(void *s1, const void *s2, EAS_I32 n)
{
    uint8_t *d = (uint8_t *)s1;
    const uint8_t *s = (const uint8_t *)s2;
    while (n-- > 0)
        *d++ = *s++;
    return s1;
}

EAS_I32 EAS_HWMemCmp(const void *s1, const void *s2, EAS_I32 n)
{
    const uint8_t *a = (const uint8_t *)s1, *b = (const uint8_t *)s2;
    for (; n > 0; n--, a++, b++)
        if (*a != *b)
            return (EAS_I32)*a - (EAS_I32)*b;
    return 0;
}

/* a bump allocator, 8-byte aligned: EAS allocates its structures at start-up and keeps them */
void *EAS_HWMalloc(EAS_HW_DATA_HANDLE hwInstData, EAS_I32 size)
{
    uint32_t n = ((uint32_t)size + 7u) & ~7u;
    void *p;
    (void)hwInstData;
    if (size <= 0 || n > FW_EAS_POOL_BYTES - pool_used)
        return 0;
    p = (uint8_t *)pool + pool_used;
    pool_used += n;
    return p;
}

void EAS_HWFree(EAS_HW_DATA_HANDLE hwInstData, void *p)
{
    (void)hwInstData;
    (void)p;                            /* freed together, at EAS_HWShutdown */
}

/* EAS looks its sound library up by name (eas_sndlibmgt.c); the firmware's C library has no strcmp. Not built for
 * the host simulator, whose C library has one. */
#ifndef PLAT_HOST
int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
#endif

/* files: whatever an EAS_FILE locator reads (eas_types.h: its readAt and size, e.g. a MIDI file in the flash; there
 * is no file system). The SMF parser takes one handle for the file and one more for each track (EAS_HWDupHandle). */
#ifndef FW_EAS_FILES
#define FW_EAS_FILES 34                 /* a file of up to 32 tracks, opened once, and a spare */
#endif

struct eas_hw_file_tag {
    const EAS_FILE *loc;                /* 0 = free */
    EAS_I32 pos;
};
static struct eas_hw_file_tag files[FW_EAS_FILES];

static EAS_FILE_HANDLE file_new(const EAS_FILE *loc, EAS_I32 pos)
{
    uint32_t i;
    for (i = 0; i < FW_EAS_FILES; i++)
        if (!files[i].loc) {
            files[i].loc = loc;
            files[i].pos = pos;
            return &files[i];
        }
    return 0;
}

EAS_RESULT EAS_HWOpenFile(EAS_HW_DATA_HANDLE h, EAS_FILE_LOCATOR locator, EAS_FILE_HANDLE *pFile, EAS_FILE_MODE mode)
{
    (void)h;
    *pFile = 0;
    if (mode != EAS_FILE_READ)
        return EAS_ERROR_INVALID_FILE_MODE;
    if (!locator || !locator->readAt || !locator->size)
        return EAS_ERROR_INVALID_PARAMETER;
    *pFile = file_new(locator, 0);
    return *pFile ? EAS_SUCCESS : EAS_ERROR_MAX_FILES_OPEN;
}

EAS_RESULT EAS_HWReadFile(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, void *b, EAS_I32 n, EAS_I32 *got)
{
    EAS_I32 left;
    (void)h;
    *got = 0;
    if (!f || !f->loc)
        return EAS_ERROR_INVALID_HANDLE;
    left = f->loc->size(f->loc->handle) - f->pos;
    if (n < 0 || left <= 0)
        return EAS_EOF;
    if (n > left)
        n = left;
    *got = f->loc->readAt(f->loc->handle, b, (int)f->pos, (int)n);
    if (*got <= 0) {
        *got = 0;
        return EAS_EOF;
    }
    f->pos += *got;
    return EAS_SUCCESS;
}

EAS_RESULT EAS_HWGetByte(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, void *p)
{
    EAS_I32 got;
    EAS_RESULT r = EAS_HWReadFile(h, f, p, 1, &got);
    return r != EAS_SUCCESS ? r : got == 1 ? EAS_SUCCESS : EAS_EOF;
}

static EAS_RESULT get_n(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, uint32_t *v, uint32_t n, EAS_BOOL msbFirst)
{
    EAS_U8 c[4];
    uint32_t i, x = 0;
    EAS_RESULT r;
    for (i = 0; i < n; i++)
        if ((r = EAS_HWGetByte(h, f, &c[i])) != EAS_SUCCESS)
            return r;
    for (i = 0; i < n; i++)
        x |= (uint32_t)c[msbFirst ? i : n - 1u - i] << (8u * (n - 1u - i));
    *v = x;
    return EAS_SUCCESS;
}

EAS_RESULT EAS_HWGetWord(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, void *p, EAS_BOOL msbFirst)
{
    uint32_t v;
    EAS_RESULT r = get_n(h, f, &v, 2, msbFirst);
    if (r == EAS_SUCCESS)
        *(EAS_U16 *)p = (EAS_U16)v;
    return r;
}

EAS_RESULT EAS_HWGetDWord(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, void *p, EAS_BOOL msbFirst)
{
    uint32_t v;
    EAS_RESULT r = get_n(h, f, &v, 4, msbFirst);
    if (r == EAS_SUCCESS)
        *(EAS_U32 *)p = v;
    return r;
}

EAS_RESULT EAS_HWFilePos(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, EAS_I32 *pos)
{
    (void)h;
    if (!f || !f->loc)
        return EAS_ERROR_INVALID_HANDLE;
    *pos = f->pos;
    return EAS_SUCCESS;
}

EAS_RESULT EAS_HWFileSeek(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, EAS_I32 pos)
{
    (void)h;
    if (!f || !f->loc)
        return EAS_ERROR_INVALID_HANDLE;
    if (pos < 0 || pos > f->loc->size(f->loc->handle))
        return EAS_ERROR_FILE_SEEK;
    f->pos = pos;
    return EAS_SUCCESS;
}

EAS_RESULT EAS_HWFileSeekOfs(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, EAS_I32 pos)
{
    if (!f || !f->loc)
        return EAS_ERROR_INVALID_HANDLE;
    return EAS_HWFileSeek(h, f, f->pos + pos);
}

EAS_RESULT EAS_HWFileLength(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, EAS_I32 *len)
{
    (void)h;
    if (!f || !f->loc)
        return EAS_ERROR_INVALID_HANDLE;
    *len = f->loc->size(f->loc->handle);
    return EAS_SUCCESS;
}

EAS_RESULT EAS_HWDupHandle(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f, EAS_FILE_HANDLE *pFile)
{
    (void)h;
    *pFile = 0;
    if (!f || !f->loc)
        return EAS_ERROR_INVALID_HANDLE;
    *pFile = file_new(f->loc, f->pos);
    return *pFile ? EAS_SUCCESS : EAS_ERROR_MAX_FILES_OPEN;
}

EAS_RESULT EAS_HWCloseFile(EAS_HW_DATA_HANDLE h, EAS_FILE_HANDLE f)
{
    (void)h;
    if (f)
        f->loc = 0;
    return EAS_SUCCESS;
}

/* no vibrator, LED or backlight for EAS to drive; nothing to yield to */
EAS_RESULT EAS_HWVibrate(EAS_HW_DATA_HANDLE h, EAS_BOOL state) { (void)h; (void)state; return EAS_SUCCESS; }
EAS_RESULT EAS_HWLED(EAS_HW_DATA_HANDLE h, EAS_BOOL state) { (void)h; (void)state; return EAS_SUCCESS; }
EAS_RESULT EAS_HWBackLight(EAS_HW_DATA_HANDLE h, EAS_BOOL state) { (void)h; (void)state; return EAS_SUCCESS; }
EAS_BOOL EAS_HWYield(EAS_HW_DATA_HANDLE h) { (void)h; return EAS_FALSE; }
void *EAS_HWRegisterSignalHandler(void) { return 0; }
EAS_RESULT EAS_HWUnRegisterSignalHandler(void *cookie) { (void)cookie; return EAS_SUCCESS; }

/* debug messages: dropped */
void EAS_ReportEx(int severity, unsigned long hashCode, int serialNum, ...) { (void)severity; (void)hashCode; (void)serialNum; }
void EAS_Report(int severity, const char *fmt, ...) { (void)severity; (void)fmt; }
void EAS_ReportX(int severity, const char *fmt, ...) { (void)severity; (void)fmt; }
void EAS_SetDebugLevel(int severity) { (void)severity; }
void EAS_SetDebugFile(void *file, int flushAfterWrite) { (void)file; (void)flushAfterWrite; }
