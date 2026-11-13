/* Licence: TBD (see D017) */
/* vsynth_test: SYNTH mode's sound (firmware/app/vsynth.c) on the laptop under ASan/UBSan (tools/vsynth_test.sh).
 * Every preset plays a chord (C E G B, 1.5 s held, then released until silent), then its arpeggio over it; prints per
 * preset the peak and RMS (24-bit), the voices at most; fails on silence, clipping, a voice that never ends, or an
 * arpeggio with no steps. Then (M6) the arp's rates and RANDOM, and (M7) each effect alone and all together: the sound
 * changes, the effect's work is skipped at 0, and it is silent after the release. Optional: WAV=dir writes the
 * presets' renders. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../firmware/app/vsynth.h"

#define N 256
static int32_t buf[2 * N];
static uint32_t fx_seen;                  /* the FXB_* bits seen during a run */
#define CAP 44032u
static int32_t cap[CAP], dry_cap[CAP];    /* a run's left channel (the effects: compared with the dry sound's) */
static uint32_t ncap;

static void run(double secs, double *rms_out, uint32_t *peak_out, FILE *wav)
{
    uint32_t blocks = (uint32_t)(secs * 44100 / N), b, i, peak = 0;
    double sum = 0;
    fx_seen = 0;
    ncap = 0;
    for (b = 0; b < blocks; b++) {
        vsynth_render(buf, N, 4096);
        fx_seen |= vsynth_stats()->fx;
        for (i = 0; i < 2 * N; i++) {
            uint32_t a = (uint32_t)(buf[i] < 0 ? -buf[i] : buf[i]);
            if (a > peak)
                peak = a;
            sum += (double)buf[i] * buf[i];
            if (!(i & 1u) && ncap < CAP)
                cap[ncap++] = buf[i];
            if (wav) {
                int16_t s = (int16_t)(buf[i] >> 8);
                fwrite(&s, 2, 1, wav);
            }
        }
    }
    *rms_out = sqrt(sum / (blocks * 2.0 * N));
    *peak_out = peak;
}

static const uint8_t CH[4] = {60, 64, 67, 71};
static void chord(int on)
{
    uint32_t k;
    for (k = 0; k < 4; k++)
        vsynth_key(CH[k], on);
}

static uint32_t fails;
static void check(int ok, const char *what)
{
    if (!ok) {
        fails++;
        printf("   ^ FAIL: %s\n", what);
    }
}

/* the effect test's dry sound: NEON PAD (M8: preset 4, D050; M7: 0) with every effect at 0 */
static void dry(void)
{
    static const uint8_t FX0[] = {VP_CHO_MIX, VP_TREM_DEPTH, VP_PHA_MIX, VP_CRUSH, VP_DLY_MIX, VP_FUZZ, VP_REV_MIX};
    uint32_t k;
    vsynth_load(vsynth_stock(3));
    vsynth_set(VP_ARP_ON, 0);
    for (k = 0; k < sizeof FX0; k++)
        vsynth_set(FX0[k], 0);
    vsynth_set(VP_ATK, 0);
    vsynth_set(VP_REL, 30);
}

/* one chord through the sound now: its rms while held (the 2nd second), its tail's end */
static void effect_run(const char *name, uint32_t bit, double dry_rms, double settle)
{
    double r, rt, diff;
    uint32_t pk, pk2, seen;
    chord(1);
    run(1.0, &r, &pk, 0);
    run(1.0, &r, &pk, 0);
    seen = fx_seen;
    {                                     /* how far from the dry sound: the difference's rms over the dry's */
        double dd = 0, d0 = 0;
        uint32_t i;
        for (i = 0; i < CAP; i++)
            dd += ((double)cap[i] - dry_cap[i]) * ((double)cap[i] - dry_cap[i]), d0 += (double)dry_cap[i] * dry_cap[i];
        diff = sqrt(dd / (d0 + 1));
    }
    chord(0);
    run(settle, &rt, &pk2, 0);
    run(0.5, &rt, &pk2, 0);
    printf("%-26s held rms %8.0f (dry %8.0f, difference %.2f of it) peak %8u  ran %02x  end peak %u\n", name, r,
           dry_rms, diff, pk, seen, pk2);
    check(bit == 0xFF ? (seen & 0x7Fu) == 0x7Fu : (seen >> bit & 1u) != 0, "the effect did not run");
    check(diff > 0.2, "the sound did not change");
    check(pk < 8388607, "clipped");
    check(pk2 <= 2000, "not silent after the release");
}

/* a preset's loudness: a chord (C E G B; with its arpeggio if it has one) for 2 s, the loudest 100 ms (rms, left) */
static double loudness(uint32_t p)
{
    double r, best = 0;
    uint32_t pk, k, w;
    vsynth_load(vsynth_stock(p));
    chord(1);
    for (k = 0; k < 20; k++) {
        run(0.1, &r, &pk, 0);
        for (w = 0, r = 0; w < ncap; w++)
            r += (double)cap[w] * cap[w];
        r = sqrt(r / (ncap ? ncap : 1));
        best = r > best ? r : best;
    }
    chord(0);
    vsynth_set(VP_ARP_ON, 0);
    run(9.0, &r, &pk, 0);
    return best;
}

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

int main(void)
{
    static uint8_t mem[VS_MEM_BYTES];
    uint32_t p;
    const char *dir = getenv("WAV");
    vsynth_start(mem, sizeof mem);
    vsynth_tempo(120);
    {                                     /* M7: the presets' loudness (LEVELS=1: and each one's LEVEL to match) */
        static double ld[64], sorted[64];
        uint32_t n = vsynth_presets();
        double med;
        for (p = 0; p < n; p++)
            ld[p] = sorted[p] = loudness(p);
        qsort(sorted, n, sizeof sorted[0], cmp_d);
        med = sorted[n / 2];
        for (p = 0; p < n; p++) {
            char name[VS_NAME + 1];
            uint32_t lv = vsynth_stock(p)->v[VP_LEVEL];
            memcpy(name, vsynth_stock(p)->name, VS_NAME);
            name[VS_NAME] = 0;
            if (getenv("LEVELS"))
                printf("level %2u %-12s loudest 100 ms %8.0f (median %8.0f) LEVEL %3u -> %3.0f\n", p, name, ld[p], med,
                       lv, lv + 12.0 * log2(med / ld[p]));
            check(ld[p] < med * 1.41 && ld[p] > med / 1.41, "a preset's level is more than 3 dB from the median");
        }
        printf("presets' loudness: %.0f .. %.0f, median %.0f (within 3 dB of it: %s)\n", sorted[0], sorted[n - 1], med,
               sorted[0] > med / 1.41 && sorted[n - 1] < med * 1.41 ? "yes" : "NO");
    }
    for (p = 0; p < vsynth_presets(); p++) {
        double r1, r2, r3, r4;
        uint32_t k1, k2, k3, k4, steps0, vmax;
        FILE *wav = 0;
        const struct vs_preset *pr = vsynth_stock(p);
        char name[VS_NAME + 1];
        memcpy(name, pr->name, VS_NAME);
        name[VS_NAME] = 0;
        if (dir) {
            char f[256];
            snprintf(f, sizeof f, "%s/%02u.raw", dir, p);
            wav = fopen(f, "wb");
        }
        vsynth_load(pr);
        vsynth_set(VP_ARP_ON, 0);
        chord(1);
        run(1.5, &r1, &k1, wav);
        chord(0);
        run(7.0, &r2, &k2, wav);                       /* released: the tail, then silence */
        run(0.5, &r3, &k3, wav);
        vmax = vsynth_stats()->voices;
        steps0 = vsynth_stats()->arp_steps;
        vsynth_set(VP_ARP_ON, 1);
        vsynth_set(VP_ARP_PAT, (int32_t)(p % 4u));
        vsynth_tempo(100 + p * 3);
        chord(1);
        run(2.0, &r4, &k4, wav);
        chord(0);
        vsynth_set(VP_ARP_ON, 0);
        vsynth_tempo(120);
        run(9.5, &r3, &k3, wav);
        run(0.5, &r3, &k3, wav);                       /* the last half second: silent */
        printf("%2u %-12s %-6s held rms %8.0f peak %8u  tail-end voices %u  arp steps %u rms %8.0f  end peak %u\n",
               p, name, vsynth_engine_name(pr->engine), r1, k1, vmax, vsynth_stats()->arp_steps - steps0, r4, k3);
        check(r1 >= 20000 || (p == 8 && r1 >= 3000), "too quiet");     /* (RISER: a 6 s attack) */
        check(k1 < 8388607, "clipped");
        check(!vmax, "a voice never ended");
        check(vsynth_stats()->arp_steps - steps0 >= 4 && r4 >= 1000, "the arpeggio");
        check(k3 <= 2000, "not silent at the end");
        if (wav)
            fclose(wav);
    }
    {                                     /* the arp's rates (1/8 triplets keep time), RANDOM, OCTAVES */
        static const uint32_t RATE_STEPS[RATES] = {8, 16, 24, 32};   /* in 4 s at 120 BPM */
        double r;
        uint32_t pk, st0, rt, n;
        vsynth_load(vsynth_stock(12));    /* CHIP BLIP: short (M8: 13th) */
        for (rt = 0; rt < RATES; rt++) {
            vsynth_key(60, 1);
            vsynth_key(64, 1);
            vsynth_set(VP_ARP_ON, 1);
            vsynth_set(VP_ARP_PAT, ARP_UP);
            vsynth_set(VP_ARP_RATE, (int32_t)rt);
            st0 = vsynth_stats()->arp_steps;
            run(4.0, &r, &pk, 0);
            n = vsynth_stats()->arp_steps - st0;
            printf("arp rate %u at 120 BPM: %u steps in 4 s (want %u)\n", rt, n, RATE_STEPS[rt]);
            check(n + 1u >= RATE_STEPS[rt] && n <= RATE_STEPS[rt] + 1u, "arp rate");
            vsynth_set(VP_ARP_ON, 0);
            vsynth_key(60, 0);
            vsynth_key(64, 0);
            run(1.0, &r, &pk, 0);
        }
        for (rt = 0; rt < 5; rt++)
            vsynth_key(60 + 2 * rt, 1);
        vsynth_set(VP_ARP_ON, 1);
        vsynth_set(VP_ARP_PAT, ARP_RANDOM);
        vsynth_set(VP_ARP_RATE, RATE_16);
        vsynth_set(VP_ARP_OCT, 2);
        st0 = vsynth_stats()->arp_steps;
        run(4.0, &r, &pk, 0);
        n = vsynth_stats()->arp_steps - st0;
        printf("arp RANDOM over 5 keys x 3 octaves: %u steps in 4 s\n", n);
        check(n >= 30, "RANDOM");
        vsynth_set(VP_ARP_ON, 0);
        vsynth_set(VP_ARP_OCT, 0);
        for (rt = 0; rt < 5; rt++)
            vsynth_key(60 + 2 * rt, 0);
        run(8.0, &r, &pk, 0);
    }
    {                                     /* M7: the effects, each alone, then all together */
        double rd, r;
        uint32_t pk;
        dry();
        run(0.5, &r, &pk, 0);             /* (the lines found silent: skipped from then on) */
        chord(1);
        run(1.0, &r, &pk, 0);
        run(1.0, &rd, &pk, 0);
        memcpy(dry_cap, cap, sizeof cap);
        check((fx_seen & 0x7Fu) == 0, "an effect at 0 ran (dry)");
        printf("%-26s held rms %8.0f  ran %02x (none: every effect at 0 skipped)\n", "dry", rd, fx_seen);
        chord(0);
        run(3.0, &r, &pk, 0);
        dry(), vsynth_set(VP_FUZZ, 127), effect_run("fuzz 127", FXB_FUZZ, rd, 3.0);
        dry(), vsynth_set(VP_FUZZ, 30), effect_run("fuzz 30", FXB_FUZZ, rd, 3.0);
        dry(), vsynth_set(VP_CRUSH, 127), effect_run("crush 127", FXB_CRUSH, rd, 3.0);
        dry(), vsynth_set(VP_TREM_DEPTH, 127), vsynth_set(VP_TREM_RATE, 90), effect_run("tremolo 127", FXB_TREM, rd, 3.0);
        dry(), vsynth_set(VP_PHA_MIX, 127), vsynth_set(VP_PHA_DEPTH, 127), vsynth_set(VP_PHA_RATE, 60),
            effect_run("phaser 127", FXB_PHASER, rd, 3.0);
        dry(), vsynth_set(VP_CHO_MIX, 127), vsynth_set(VP_CHO_DEPTH, 127), effect_run("chorus 127", FXB_CHORUS, rd, 3.0);
        dry(), vsynth_set(VP_DLY_MIX, 127), effect_run("delay 127 (feedback 64)", FXB_DELAY, rd, 8.0);
        dry(), vsynth_set(VP_DLY_MIX, 127), vsynth_set(VP_DLY_FB, 127), vsynth_set(VP_DLY_TIME, 0),
            effect_run("delay 127, feedback 127", FXB_DELAY, rd, 30.0);
        dry(), vsynth_set(VP_REV_MIX, 127), vsynth_set(VP_REV_SIZE, 127), effect_run("reverb 127 (size 127)", FXB_REVERB,
                                                                                     rd, 14.0);
        dry();
        vsynth_set(VP_FUZZ, 127), vsynth_set(VP_CRUSH, 127), vsynth_set(VP_TREM_DEPTH, 127);
        vsynth_set(VP_PHA_MIX, 127), vsynth_set(VP_PHA_DEPTH, 127), vsynth_set(VP_CHO_MIX, 127);
        vsynth_set(VP_DLY_MIX, 127), vsynth_set(VP_REV_MIX, 127);
        effect_run("all at 127", 0xFF, rd, 14.0);
        {                                 /* and nothing runs once silent with every mix back at 0 */
            double rr;
            dry();
            run(0.5, &rr, &pk, 0);
            run(1.0, &rr, &pk, 0);
            printf("after: every effect at 0 again (0.5 s on): ran %02x\n", fx_seen);
            check((fx_seen & 0x7Fu) == 0, "an effect at 0 still ran");
        }
    }
    {                                     /* M7: the drum kits' voices: every sound of 808 and CHIP, alone, then all */
        static const char *const IN[DR_N] = {"kick", "snare", "closed hat", "open hat", "clap", "tom low", "tom high",
                                             "rim"};
        uint32_t kit, i, pk, pk2;
        double r, rt;
        vsynth_load(vsynth_stock(0));
        for (kit = KIT_808; kit < KITS; kit++)
            for (i = 0; i < DR_N + 1u; i++) {
                uint32_t k;
                for (k = 0; k < DR_N; k++)
                    if (k == i || i == DR_N)
                        vsynth_drum(k % 8u, kit, k, 3, 127);
                run(0.3, &r, &pk, 0);
                run(2.0, &rt, &pk2, 0);
                run(0.5, &rt, &pk2, 0);               /* (2.3 .. 2.8 s after the hit: silent) */
                printf("drum %-4s %-10s rms %8.0f peak %8u  voices %u  then peak %u\n", kit == KIT_808 ? "808" : "CHIP",
                       i == DR_N ? "all eight" : IN[i], r, pk, vsynth_stats()->drum_voices_max, pk2);
                check(r > 20000 && pk < 8388607, "a drum sound");
                check(pk2 <= 2000 && vsynth_stats()->drum_voices == 0, "a drum sound that does not end");
            }
        check(vsynth_stats()->drum_voices_max <= VS_DRUM_VOICES, "more drum voices than its own");
    }
    printf("voices at most %u, notes %u: %s\n", vsynth_stats()->voices_max, vsynth_stats()->notes,
           fails ? "FAILURES" : "ALL PASS");
    return fails != 0;
}
