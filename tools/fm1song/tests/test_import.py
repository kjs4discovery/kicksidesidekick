# The MIDI import tool (M4 part 5): a small made-up file through the whole import.
from fm1song import compile_folder, read_compiled
from fm1song import importer as imp
from fm1song.smf import meta_tempo, meta_timesig, read_midi, write_midi

PPQ = 96                                 # (the file's own resolution, not 480: converted on reading)
CHORDS = [(60, 64, 67), (65, 69, 72), (67, 71, 74), (60, 64, 67)] * 2   # C F G C, twice: 8 bars of 4/4


def _file():
    bar = 4 * PPQ
    tr = lambda chan, notes: [(t, bytes([0x90 | chan, p, 80])) for t, d, p in notes] + \
        [(t + d, bytes([0x80 | chan, p, 0])) for t, d, p in notes]
    mel = [(PPQ * 3, PPQ, 76)] + [(bar * (b + 1) + PPQ * k, PPQ, 76 + (k % 3)) for b in range(8) for k in range(4)]
    harm = [(bar * (b + 1), bar - 10, p) for b, c in enumerate(CHORDS) for p in c]           # held: like level 1
    bass = [(bar * (b + 1) + PPQ * k, PPQ - 10, c[0] - 24) for b, c in enumerate(CHORDS) for k in range(4)]
    drums = [(bar * (b + 1) + PPQ * k, 20, 36 if k % 2 == 0 else 38) for b in range(8) for k in range(4)]
    return write_midi(PPQ, [("conductor", [(0, meta_tempo(600000)), (0, meta_timesig(4, 4))]),
                            ("Voice", tr(0, mel)), ("Piano", tr(1, harm)), ("Bass", tr(2, bass)),
                            ("Drums", tr(9, drums))])


def test_roles_are_guessed():
    infos = imp.analyze(read_midi(_file()))
    roles = imp.guess_roles(infos)
    by = {i.name: roles[i.index] for i in infos}
    assert by == {"Voice": "melody", "Piano": "harmony", "Bass": "bass", "Drums": "drums"}


def test_import_makes_every_level(tmp_path):
    src = tmp_path / "t.mid"
    src.write_bytes(_file())
    out = tmp_path / "song"
    song, rep = imp.import_file(src, out, title="Test Song", category="Tests", rights="ours")
    assert song.bpb == 4 and song.tempo == 100 and song.pickup == 1 and song.key == "C"
    assert [b.strip("| ").strip() for b in song.chart_bars[1:5]] == ["C", "F", "G", "C"]
    assert song.levels["harmony"][1] == "from the file"            # held chords: the level they resemble
    assert song.levels["bass"][2] == "from the file"               # the root on every beat
    assert song.levels["drums"][2] == "from the file" and "generated" in song.levels["harmony"][5]
    c = read_compiled(compile_folder(out)[0])
    assert {k: len(v["variants"]) for k, v in c["layers"].items()} == {"drums": 3, "bass": 4, "harmony": 5,
                                                                        "melody": 1}
    assert c["category"] == "Tests" and (out / "IMPORT-REPORT.txt").is_file()
    mel = [e for e in c["layers"]["melody"]["variants"][1] if e[1] == 144]
    assert len(mel) == 33 and mel[0][0] == 3 * 480                 # every melody note, nothing added; the pickup


def test_recognize_chords():
    hist = [0.0] * 12
    for pc in (2, 5, 9, 0):                                         # D F A C: Dm7
        hist[pc] = 1.0
    assert imp.recognize(hist, 2) == (2, "m7")
    hist = [0.0] * 12
    for pc in (7, 11, 2):
        hist[pc] = 1.0
    assert imp.recognize(hist, 7) == (7, "")
