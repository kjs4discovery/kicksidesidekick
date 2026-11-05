# fm1song: audio previews of a compiled song (numpy; lameenc for mp3). Licence: TBD (see D017)
#
# A rough laptop-side synth in the spirit of songs/amazing-grace/make_amazing_grace.py's previews: it plays
# the compiled events (so a preview checks the compiled file, not the source), one simple timbre per GM
# program family, plus an optional count-in (one bar and the pickup's empty beats, as the device does).
import wave

GM_KIT = {"kick": (35, 36), "snare": (38, 40), "stick": (37,), "clap": (39,), "hat": (42, 44, 46),
          "ride": (51, 53, 59), "crash": (49, 52, 55, 57), "tom": (41, 43, 45, 47, 48, 50)}


def _timbre(program):
    if program is None:
        return "epiano"
    if program < 8:
        return "epiano"
    if 24 <= program < 32:
        return "pluck"
    if 32 <= program < 40:
        return "bass"
    if 16 <= program < 24 or 40 <= program < 56 or program >= 80:
        return "organ"
    return "flute"


def render(compiled, layers, count_in=True, rate=22050, tempo_pct=100):
    """compiled: read_compiled() output; layers: {name: variant} of the layers to hear -> float samples"""
    import numpy as np
    ppq, bpb = compiled["ppq"], int(compiled["time_signature"].split("/")[0])
    sec_per_tick = compiled["tempo_us"] / 1e6 / ppq * 100 / tempo_pct
    pickup = compiled["pickup_beats"]
    start = (bpb - pickup) * ppq if pickup else 0
    lead = (bpb * ppq + start if count_in else 0) - start      # ticks of clicks before the first note
    total = (compiled["end_tick"] - start + lead) * sec_per_tick + 2.5
    out = np.zeros(int(total * rate))
    rng = np.random.default_rng(1)

    def place(sig, tick):
        i = int((tick - start + lead) * sec_per_tick * rate)
        j = min(len(out), i + len(sig))
        if i < len(out):
            out[i:j] += sig[:j - i]

    def tone(freq, dur, kind):
        t = np.arange(int((dur + 0.6) * rate)) / rate
        rel = lambda r: np.where(t < dur, 1, np.exp(-(t - dur) / r))
        if kind == "flute":
            vib = 1 + 0.004 * np.sin(2 * np.pi * 5 * t)
            s = np.sin(2 * np.pi * freq * vib * t) + 0.25 * np.sin(4 * np.pi * freq * t)
            return 0.28 * s * np.minimum(t / 0.06, 1) * rel(0.08)
        if kind == "epiano":
            s = sum(a * np.sin(2 * np.pi * freq * k * t) * np.exp(-t * k * 1.2) for k, a in ((1, 1), (2, .4), (3, .15)))
            return 0.16 * s * np.minimum(t / 0.005, 1) * rel(0.1)
        if kind == "bass":
            s = np.sin(2 * np.pi * freq * t) + 0.5 * np.sin(4 * np.pi * freq * t)
            return 0.4 * s * np.minimum(t / 0.01, 1) * np.exp(-t * 1.5) * rel(0.06)
        if kind == "pluck":
            s = sum(a * np.sin(2 * np.pi * freq * k * t) for k, a in ((1, 1), (2, .5), (3, .3), (4, .2)))
            return 0.14 * s * np.exp(-t * 3) * rel(0.05)
        s = sum(a * np.sin(2 * np.pi * freq * k * t) for k, a in ((1, 1), (2, .5), (4, .25)))
        return 0.1 * s * np.minimum(t / 0.03, 1) * rel(0.12)

    def drum(n):
        t = np.arange(int(1.3 * rate)) / rate
        noise = rng.standard_normal(len(t))
        hp = np.diff(noise, prepend=0)
        if n in GM_KIT["kick"]:
            f = 50 + 70 * np.exp(-t / 0.04)
            return 0.8 * np.sin(2 * np.pi * np.cumsum(f) / rate) * np.exp(-t / 0.18)
        if n in GM_KIT["snare"] or n in GM_KIT["clap"]:
            return (0.12 * noise + 0.2 * np.sin(2 * np.pi * 185 * t)) * np.exp(-t / 0.09)
        if n in GM_KIT["stick"]:
            return 0.12 * hp * np.exp(-t / 0.015)
        if n in GM_KIT["hat"]:
            return 0.04 * hp * np.exp(-t / (0.25 if n == 46 else 0.05))
        if n in GM_KIT["ride"]:
            return 0.05 * hp * np.exp(-t / 0.35)
        if n in GM_KIT["crash"]:
            return 0.08 * hp * np.exp(-t / 1.0)
        if n in GM_KIT["tom"]:
            f = 90 + 4 * (n - 41) + 40 * np.exp(-t / 0.05)
            return 0.4 * np.sin(2 * np.pi * np.cumsum(f) / rate) * np.exp(-t / 0.2)
        return 0.03 * hp * np.exp(-t / 0.05)

    freq = lambda p: 440 * 2 ** ((p - 69) / 12)
    if count_in:
        t = np.arange(int(0.05 * rate)) / rate
        tick, beat = start - lead, 0
        while tick < start:
            f = 1500 if beat % bpb == 0 else 1000
            place(0.3 * np.sin(2 * np.pi * f * t) * np.exp(-t / 0.015), tick)
            tick, beat = tick + ppq, beat + 1
    for name, variant in layers.items():
        ly = compiled["layers"].get(name)
        if not ly:
            continue
        evs = ly["variants"].get(variant) or ly["variants"][max(ly["variants"])]
        kind, gain = _timbre(ly["program"]), ly["volume"] / 100
        on = {}
        for tick, st, a, b in evs:
            if st == 0x90:
                if name == "drums":
                    place(drum(a) * b / 80 * gain, tick)
                else:
                    on[a] = (tick, b)
            elif st == 0x80 and a in on:
                t0, vel = on.pop(a)
                place(tone(freq(a), (tick - t0) * sec_per_tick, kind) * vel / 80 * gain, t0)
    peak = np.abs(out).max()
    return out / (peak / 0.9) if peak > 0 else out


def write_audio(samples, path, rate=22050, kbps=64):
    """path ending .mp3 (needs lameenc) or .wav"""
    import numpy as np
    pcm = (np.clip(samples, -1, 1) * 32767).astype("<i2").tobytes()
    if str(path).endswith(".mp3"):
        import lameenc
        enc = lameenc.Encoder()
        enc.set_bit_rate(kbps)
        enc.set_in_sample_rate(rate)
        enc.set_channels(1)
        enc.set_quality(2)
        with open(path, "wb") as f:
            f.write(enc.encode(pcm) + enc.flush())
    else:
        with wave.open(str(path), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(rate)
            w.writeframes(pcm)
