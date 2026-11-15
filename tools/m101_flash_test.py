#!/usr/bin/env python3
# m101_flash_test.py -- M10.1 Part 3: FACTORY RESET on the TIMED emulator (tools/m51_test.py's runner: erases 8 ms, page programs 2 ms,
# a read of the XIP window while a flash command has it off stalls until the watchdog resets, as on the chip). Laptop and emulator
# only: never touches the FM-1. Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m101_flash_test.py [--pkg PKG] [--out DIR] [--jobs 3]     -> <out>/report.md
#
# A power cut at each flash operation of the reset (the song store's erases, the preset object's, the settings' save), then a boot:
# it always boots (no crash, no watchdog reset), and the song store, the presets and the settings are each either old or empty.
import argparse
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m51_test as m51                   # noqa: E402  (sets FM1_FLASH_TIMED=1)
import m5_test as m5                     # noqa: E402
import m101_test as m101                 # noqa: E402  (its stats parser and prep image)

timed, healthy = m51.timed, m51.healthy
V_LIST = 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m101flash"
    if a.pkg:
        m5.PKG = Path(a.pkg).resolve()
    m5.OUT.mkdir(parents=True, exist_ok=True)
    m5.prepare_flash()
    lines, st, check = m5.checker("M10.1 Part 3: FACTORY RESET on the timed emulator")
    before = m101.prep_images("f")
    host = f"{m5.SEND} factory-reset --emu-pipe --yes"
    default = timed("m101f-default", 5200, [], flash="empty")["stats"]
    b0 = timed("m101f-before", 5200, [], flash=str(before))["stats"]
    b0["preset_used"] = timed("m101f-before-sy", 5200, m101.SYNTH_PRE, flash=str(before), mode=None)["stats"]["preset_used"]
    full = timed("m101f-full", 40000, [], flash=str(before), host=host)
    total = full["ops"]
    check(healthy(full) and "factory reset done" in full["out"] and total >= 4,
          f"the whole reset on the timed flash: {total} flash operations, watchdog resets {full['wdt']}, XIP reads with a flash command "
          f"running {full['xip']}, longest unfed stretch {full['gap']} ms (limit {m51.GAP_LIMIT_MS})")
    with ThreadPoolExecutor(a.jobs) as ex:
        cuts = [(k, ex.submit(lambda k=k: timed(f"m101f-cut-{k}", 40000, [], flash=str(before), host=host, cut=k)))
                for k in range(1, total + 1)]
        cuts = [(k, f.result()) for k, f in cuts]
        boots = [(k, ex.submit(lambda k=k, c=c: timed(f"m101f-boot-{k}", 5600, [], flash=str(c["flash"])))) for k, c in cuts]
        boots = [(k, f.result()) for k, f in boots]
        sboots = [(k, ex.submit(lambda k=k, c=c: timed(f"m101f-sboot-{k}", 5600, m101.SYNTH_PRE, flash=str(c["flash"]), mode=None)))
                  for k, c in cuts]
        sboots = [(k, f.result()) for k, f in sboots]
    rows, bad = [], []
    for (k, c), (_, b), (_, sb) in zip(cuts, boots, sboots):
        s = b["stats"]
        s["preset_used"] = sb["stats"]["preset_used"]       # (the saved presets are counted in SYNTH)
        row = (k, s["list_n"], s["preset_used"], (s["settings"] >> 16) & 15)
        rows.append(row)
        ok = (healthy(c) and healthy(b) and healthy(sb) and not b["halt"] and not sb["halt"] and s["ui_view"] == V_LIST and s["list_n"] in (0, 1, 2) and
              s["preset_used"] in (0, 2) and ((s["settings"] >> 16) & 15 in (1, (default["settings"] >> 16) & 15)))
        if not ok:
            bad.append((k, row, c["wdt"], c["xip"], c["gap"], b["wdt"], b["halt"]))
    check(not bad,
          f"a power cut at each of the {total} flash operations, then a boot: always boots (the song list, no crash, no watchdog reset, no "
          f"XIP read with a flash command running); songs / presets / ZOOM after each cut (k, songs, presets, zoom): {rows}; before: "
          f"{b0['list_n']} / {b0['preset_used']} / {(b0['settings'] >> 16) & 15}, defaults: 0 / 0 / {(default['settings'] >> 16) & 15} {bad[:3] or ''}")
    # no cut: the reset complete: everything empty after a boot; the cuts in order never bring anything back
    fb = timed("m101f-full-boot", 5600, [], flash=str(full["flash"]))["stats"]
    fb["preset_used"] = timed("m101f-full-sboot", 5600, m101.SYNTH_PRE, flash=str(full["flash"]), mode=None)["stats"]["preset_used"]
    dz = (default["settings"] >> 16) & 15
    check((fb["list_n"], fb["preset_used"], (fb["settings"] >> 16) & 15) == (0, 0, dz) and
          all(a[1] >= b[1] and a[2] >= b[2] for a, b in zip(rows, rows[1:])),
          f"the complete reset leaves no songs, no presets and the default settings after a boot ({fb['list_n']} / {fb['preset_used']} / "
          f"{(fb['settings'] >> 16) & 15}); a later cut never has more songs or presets than an earlier one")
    sha = m5.subprocess.run(["sha256sum", str(m5.PKG)], capture_output=True, text=True).stdout.split()[0]
    text = [f"# M10.1 FACTORY RESET on the timed emulator\n\nPackage: {m5.PKG}, sha256 {sha}\n"] + lines
    text.append(f"\n**{'ALL PASS' if st['ok'] else 'FAILURES'}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if st["ok"] else 1)


if __name__ == "__main__":
    main()
