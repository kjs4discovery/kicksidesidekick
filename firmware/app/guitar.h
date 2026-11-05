/* Licence: TBD (see D017) */
/* Guitar chord shapes (M3 part 6): our own table of standard open shapes, and barre shapes (E and A forms) made from
 * templates for any root. Strings low E to high e; frets as characters: 'x' muted, '0' open, '1'.. fretted (in a
 * template: frets above the barre, '0' the barre itself); fingers the same way ('0' none). Included by ui.c. */
#pragma once
#include <stdint.h>

struct gshape {
    const char *name;                    /* root and quality as in a chord symbol: "C", "Am7", "D/F#" */
    const char *frets, *fingers;         /* 6 characters each */
};

/* the open shapes first: they win over a barre for the same chord */
static const struct gshape GUITAR_OPEN[] = {
    {"C", "x32010", "032010"},     {"C7", "x32310", "032410"},    {"Cmaj7", "x32000", "032000"},
    {"Cadd9", "x32030", "021030"}, {"Csus4", "x33011", "034011"}, {"Csus2", "x30013", "030014"},
    {"C/E", "032010", "032010"},   {"C/G", "332010", "342010"},   {"C/B", "x22010", "023010"},
    {"D", "xx0232", "000132"},     {"D7", "xx0212", "000213"},    {"Dm", "xx0231", "000231"},
    {"Dm7", "xx0211", "000211"},   {"Dmaj7", "xx0222", "000111"}, {"Dsus4", "xx0233", "000134"},
    {"Dsus2", "xx0230", "000130"}, {"D/F#", "200232", "100243"},  {"D7/F#", "200212", "100213"},
    {"D/A", "x00232", "000132"},
    {"E", "022100", "023100"},     {"E7", "020100", "020100"},    {"Em", "022000", "023000"},
    {"Em7", "020000", "020000"},   {"Emaj7", "021100", "031200"}, {"Esus4", "022200", "023400"},
    {"F", "133211", "134211"},     {"F7", "131211", "131211"},    {"Fmaj7", "xx3210", "003210"},
    {"Fm", "133111", "134111"},    {"F/C", "x33211", "034211"},   {"F/A", "x03211", "003211"},
    {"G", "320003", "210004"},     {"G7", "320001", "320001"},    {"Gmaj7", "320002", "320001"},
    {"G/B", "x20003", "010004"},   {"G/D", "xx0003", "000004"},   {"G/F", "120003", "120004"},
    {"Gsus4", "330013", "230014"}, {"G6", "320000", "210000"},
    {"A", "x02220", "001230"},     {"A7", "x02020", "001030"},    {"Am", "x02210", "002310"},
    {"Am7", "x02010", "002010"},   {"Amaj7", "x02120", "002130"}, {"Asus4", "x02230", "001240"},
    {"Asus2", "x02200", "001200"}, {"Am/G", "302210", "302410"},  {"A/E", "002220", "001230"},
    {"B7", "x21202", "021304"},    {"Bm", "x24432", "013421"},    {"Bm7", "x20202", "020304"},
    {"E/G#", "422100", "423100"},  {"Am/C", "x32210", "032410"},
};

/* barre templates: the root on the 6th string (E form) or the 5th (A form); quality "" = major */
struct gbarre {
    const char *quality;
    uint8_t string;                      /* 6 or 5: where the root is */
    const char *frets, *fingers;         /* relative to the barre fret ('0' = the barre) */
};
static const struct gbarre GUITAR_BARRE[] = {
    {"", 6, "022100", "134211"},   {"m", 6, "022000", "134111"},  {"7", 6, "020100", "131211"},
    {"m7", 6, "020000", "131111"}, {"maj7", 6, "0x110x", "103420"}, {"sus4", 6, "022200", "134411"},
    {"", 5, "x02220", "013331"},   {"m", 5, "x02210", "013421"},  {"7", 5, "x02020", "013141"},
    {"m7", 5, "x02010", "013121"}, {"maj7", 5, "x02120", "013241"}, {"sus4", 5, "x02230", "013341"},
};
