/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ukulele chord shapes (M6 part 2): our own table, standard tuning G4 C4 E4 A4 (re-entrant: the G string is the
 * high one). Strings G C E A, left to right as a chord box shows them; frets and fingers as guitar.h ('0' open /
 * none). A uke has no real bass note, so a slash chord shows its chord's shape (ui.c says which bass to leave out).
 * Checked on the laptop: tools/guitar_check.py --uke (every shape's notes are its chord's, the root and the third
 * there; every chord of every song in all 12 keys finds a shape). Included by ui.c. */
#pragma once
#include <stdint.h>

/* the open shapes first (matched by pitch class, so C#/Db alike) */
static const struct gshape UKE_OPEN[] = {
    {"C", "0003", "0003"},     {"C7", "0001", "0001"},    {"Cmaj7", "0002", "0002"},  {"Cm", "0333", "0111"},
    {"Cm7", "3333", "1111"},   {"Csus4", "0013", "0013"}, {"Csus2", "0233", "0123"},  {"Cadd9", "0203", "0102"},
    {"C6", "0000", "0000"},
    {"D", "2220", "1230"},     {"D7", "2223", "1112"},    {"Dm", "2210", "2310"},     {"Dm7", "2213", "2314"},
    {"Dmaj7", "2224", "1113"}, {"Dsus4", "0230", "0120"}, {"Dsus2", "2200", "1200"},
    {"E", "4442", "2341"},     {"E7", "1202", "1203"},    {"Em", "0432", "0321"},     {"Em7", "0202", "0102"},
    {"Esus4", "4400", "1200"},
    {"F", "2010", "2010"},     {"F7", "2313", "1312"},    {"Fm", "1013", "1013"},     {"Fmaj7", "2410", "1310"},
    {"G", "0232", "0132"},     {"G7", "0212", "0213"},    {"Gm", "0231", "0231"},     {"Gm7", "0211", "0211"},
    {"Gmaj7", "0222", "0111"}, {"Gsus4", "0233", "0123"}, {"G6", "0202", "0102"},     {"Gsus2", "0230", "0120"},
    {"A", "2100", "2100"},     {"A7", "0100", "0100"},    {"Am", "2000", "2000"},     {"Am7", "0000", "0000"},
    {"Amaj7", "1100", "1200"}, {"Asus4", "2200", "1200"},
    {"Bb", "3211", "3211"},    {"B7", "2322", "1211"},    {"Bm", "4222", "3111"},     {"Bm7", "2222", "1111"},
};

/* movable shapes for any root: frets relative to the base fret b (the shape's root is ROOT + b), '0' = b itself; the
 * lowest b >= 1 wins (a shape at b = 0 is an open one: the table above has those) */
struct ushape {
    const char *quality;
    uint8_t root;                        /* the pitch class at b = 0 */
    const char *frets, *fingers;
};
static const struct ushape UKE_MOVE[] = {
    {"", 0, "0003", "1114"},     {"", 9, "2100", "3211"},     {"", 7, "0232", "1243"},
    {"m", 9, "2000", "3111"},    {"m", 0, "0333", "1333"},    {"m", 7, "0231", "1342"},
    {"7", 9, "0100", "1211"},    {"7", 0, "0001", "1112"},    {"7", 7, "0212", "1324"},
    {"m7", 9, "0000", "1111"},   {"m7", 2, "2213", "2314"},
    {"maj7", 0, "0002", "1113"}, {"maj7", 9, "1100", "2311"},
    {"sus4", 0, "0013", "1124"}, {"sus4", 9, "2200", "2311"},
    {"sus2", 0, "0233", "1344"}, {"sus2", 7, "0230", "1341"},
    {"6", 0, "0000", "1111"},    {"add9", 0, "0203", "1314"},
};
