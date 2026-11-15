# SPDX-License-Identifier: GPL-3.0-or-later
# Everything here runs on the laptop and in the emulator only; nothing writes to the FM-1.
.PHONY: firmware emu shot patches m2-test store-test fuzz hello songs song-test m51-test upreset-test

firmware:            ## build firmware/app/build/fm1acc.fwsc
	firmware/build.sh

emu:                 ## build, then play it in the fm1-emu window
	tools/emu.sh

shot:                ## build, run 2.5 s headless with C4 held: build/emu/panel.png, build/emu/audio.raw
	tools/emu.sh --shot

patches:             ## apply our local patches to third_party/ checkouts (fm1-emu flash id) and rebuild them
	tools/apply_patches.sh

m2-test: firmware    ## M2: headless emulator tests: the song store, every song, flash ids -> build/emu/m2/report.md
	.venv/bin/python -I tools/m2_test.py
store-test:          ## M2: the song store on a simulated flash, a power cut at every operation (ASan/UBSan)
	tools/store_test.sh

fuzz:                ## M1b: the device's song parser under ASan/UBSan, 10 minutes (tools/song_fuzz.sh)
	tools/song_fuzz.sh 600

hello:               ## the Step 0.6 hello world from git (fallback diagnostic) -> build/hello/fm1acc-hello.fwsc
	tools/build_hello.sh

songs:               ## M1a: regenerate the starter songs + previews, compile all to build/songs/*.fm1s
	.venv/bin/python songs/make_songs.py --preview
	for s in songs/*/; do tools/fm1song.sh compile $$s || exit 1; done

song-test:           ## M1a: the song compiler's tests (pytest, incl. fuzzing; ~5 s)
	cd tools/fm1song && ../../.venv/bin/python -m pytest -q
m51-test: firmware   ## M5.1: the song store on the TIMED emulator (flash timing, XIP-off reads, watchdog) -> build/emu/m51/report.md
	.venv/bin/python -I tools/m51_test.py
upreset-test:        ## M10: the user presets' object on a simulated flash, a power cut at every operation (ASan/UBSan)
	tools/upreset_test.sh
