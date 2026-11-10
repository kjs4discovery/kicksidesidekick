/* Licence: TBD (see D017) */
/* vsynth_test: SYNTH mode's sound (firmware/app/vsynth.c) on the laptop under ASan/UBSan (tools/vsynth_test.sh):
 * every preset plays a chord (C E G B, 1.5 s held, 1.5 s released), then the arpeggiator runs over it (each mode,
 * each rate); prints per preset the peak and RMS (24-bit), the voices at most; fails on silence, clipping, a
 * voice that never ends, or an arpeggio with no steps. Optional: WAV=dir writes each preset's render. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../firmware/app/vsynth.h"

#define N 256
static int32_t buf[2 * N];

static double run(double secs, double *rms_out, uint32_t *peak_out, FILE *wav)
{
    uint32_t blocks = (uint32_t)(secs * 44100 / N), b, i, peak = 0;
    double sum = 0;
    for (b = 0; b < blocks; b++) {
        vsynth_render(buf, N, 4096);
        for (i = 0; i < 2 * N; i++) {
            uint32_t a = (uint32_t)(buf[i] < 0 ? -buf[i] : buf[i]);
            if (a > peak)
                peak = a;
            sum += (double)buf[i] * buf[i];
            if (wav) {
                int16_t s = (int16_t)(buf[i] >> 8);
                fwrite(&s, 2, 1, wav);
            }
        }
    }
    *rms_out = sqrt(sum / (blocks * 2.0 * N));
    *peak_out = peak;
    return 0;
}

int main(void)
{
    static uint8_t mem[VS_MEM_BYTES];
    uint32_t p, k, fails = 0;
    const char *dir = getenv("WAV");
    vsynth_start(mem, sizeof mem);
    for (p = 0; p < vsynth_presets(); p++) {
        double r1, r2, r3, r4;
        uint32_t k1, k2, k3, k4, steps0, vmax;
        FILE *wav = 0;
        if (dir) {
            char f[256];
            snprintf(f, sizeof f, "%s/%02u.raw", dir, p);
            wav = fopen(f, "wb");
        }
        vsynth_preset(p);
        vsynth_arp(ARP_OFF, RATE_8, 120);
        static const uint8_t CH[4] = {60, 64, 67, 71};
        for (k = 0; k < 4; k++)
            vsynth_key(CH[k], 1);
        run(1.5, &r1, &k1, wav);
        for (k = 0; k < 4; k++)
            vsynth_key(CH[k], 0);
        run(7.0, &r2, &k2, wav);                       /* released: the tail, then silence */
        run(0.5, &r3, &k3, wav);
        vmax = vsynth_stats()->voices;
        steps0 = vsynth_stats()->arp_steps;
        for (k = 0; k < 4; k++)
            vsynth_key(CH[k], 1);
        vsynth_arp(ARP_UP + p % 3u, p % 3u, 100 + p * 5);
        run(2.0, &r4, &k4, wav);
        for (k = 0; k < 4; k++)
            vsynth_key(CH[k], 0);
        vsynth_arp(ARP_OFF, RATE_8, 120);
        run(7.5, &r3, &k3, wav);
        run(0.5, &r3, &k3, wav);                       /* the last half second: silent */
        printf("%2u %-17s held rms %8.0f peak %8u  tail-end voices %u  arp steps %u rms %8.0f  end peak %u\n", p,
               vsynth_preset_name(p), r1, k1, vmax, vsynth_stats()->arp_steps - steps0, r4, k3);
        if (r1 < 20000 || k1 >= 8388607 || vmax || vsynth_stats()->arp_steps - steps0 < 4 || r4 < 1000 || k3 > 2000)
            fails++, printf("   ^ FAIL\n");
        if (wav)
            fclose(wav);
    }
    {                                     /* M6: the arp's rates (1/8 triplets keep time), RANDOM, reverb, crush */
        static const uint32_t RATE_STEPS[RATES] = {8, 16, 24, 32};   /* in 4 s at 120 BPM */
        double r, rd, rr, rc;
        uint32_t pk, st0, rt, notes[128] = {0}, distinct = 0;
        vsynth_preset(9);                 /* CHIP BLIP: short */
        for (rt = 0; rt < RATES; rt++) {
            vsynth_key(60, 1);
            vsynth_key(64, 1);
            vsynth_arp(ARP_UP, rt, 120);
            st0 = vsynth_stats()->arp_steps;
            run(4.0, &r, &pk, 0);
            printf("arp rate %u at 120 BPM: %u steps in 4 s (want %u)\n", rt, vsynth_stats()->arp_steps - st0,
                   RATE_STEPS[rt]);
            if (vsynth_stats()->arp_steps - st0 + 1u < RATE_STEPS[rt] || vsynth_stats()->arp_steps - st0 > RATE_STEPS[rt] + 1u)
                fails++, printf("   ^ FAIL\n");
            vsynth_arp(ARP_OFF, RATE_8, 120);
            vsynth_key(60, 0);
            vsynth_key(64, 0);
            run(1.0, &r, &pk, 0);
        }
        (void)notes;
        for (rt = 0; rt < 5; rt++)
            vsynth_key(60 + 2 * rt, 1);
        vsynth_arp(ARP_RANDOM, RATE_16, 120);
        st0 = vsynth_stats()->arp_steps;
        run(4.0, &r, &pk, 0);
        distinct = vsynth_stats()->arp_steps - st0;
        printf("arp RANDOM over 5 keys: %u steps in 4 s\n", distinct);
        if (distinct < 30)
            fails++, printf("   ^ FAIL\n");
        vsynth_arp(ARP_OFF, RATE_8, 120);
        for (rt = 0; rt < 5; rt++)
            vsynth_key(60 + 2 * rt, 0);
        run(8.0, &r, &pk, 0);
        vsynth_preset(19);                /* DEEP SUB: dry, no delay */
        vsynth_set_knob(VS_REVERB, 0);
        vsynth_set_knob(VS_DELAY, 0);
        vsynth_key(60, 1);
        run(0.5, &r, &pk, 0);
        vsynth_key(60, 0);
        run(0.4, &r, &pk, 0);
        run(0.5, &rd, &pk, 0);            /* the dry tail, 0.4..0.9 s after */
        run(6.0, &r, &pk, 0);
        vsynth_set_knob(VS_REVERB, 127);
        vsynth_key(60, 1);
        run(0.5, &r, &pk, 0);
        vsynth_key(60, 0);
        run(0.4, &r, &pk, 0);
        run(0.5, &rr, &pk, 0);
        run(7.5, &r, &pk, 0);
        run(0.5, &r, &pk, 0);             /* 8.5..9 s after: silent */
        printf("reverb: the tail 0.4..0.9 s after release, rms %.0f dry, %.0f at 127; silent 8 s later (peak %u)\n",
               rd, rr, pk);
        if (rr < 4 * rd + 1000 || pk > 2000)
            fails++, printf("   ^ FAIL\n");
        vsynth_set_knob(VS_REVERB, 0);
        vsynth_key(60, 1);
        run(0.5, &r, &pk, 0);
        vsynth_set_knob(VS_CRUSH, 127);
        run(0.5, &rc, &pk, 0);
        vsynth_key(60, 0);
        vsynth_set_knob(VS_CRUSH, 0);
        run(3.0, &rd, &pk, 0);
        run(0.5, &rd, &pk, 0);
        printf("bit-crush 127: rms %.0f (clean %.0f), silent after (peak %u)\n", rc, r, pk);
        if (rc < r / 4 || pk > 2000)
            fails++, printf("   ^ FAIL\n");
    }
    printf("voices at most %u, notes %u: %s\n", vsynth_stats()->voices_max, vsynth_stats()->notes,
           fails ? "FAILURES" : "ALL PASS");
    return fails != 0;
}
