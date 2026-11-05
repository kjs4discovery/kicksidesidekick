# Licence: TBD (see D017)
# Everything here runs on the laptop and in the emulator only; nothing writes to the FM-1.
.PHONY: firmware emu shot patches proto-test hello songs song-test

firmware:            ## build firmware/app/build/fm1acc.fwsc
	firmware/build.sh

emu:                 ## build, then play it in the fm1-emu window
	tools/emu.sh

shot:                ## build, run 2.5 s headless with C4 held: build/emu/panel.png, build/emu/audio.raw
	tools/emu.sh --shot

patches:             ## apply our local patches to third_party/ checkouts (fm1-emu flash id) and rebuild them
	tools/apply_patches.sh

proto-test: firmware ## Step 0.7 prototype: headless tests (A4, A5) -> build/emu/proto/report.md (~10 min)
	.venv/bin/python -I tools/proto_test.py

hello:               ## the Step 0.6 hello world from git (fallback diagnostic) -> build/hello/fm1acc-hello.fwsc
	tools/build_hello.sh

songs:               ## M1a: regenerate the starter songs + previews, compile all to build/songs/*.fm1s
	.venv/bin/python songs/make_songs.py --preview
	for s in songs/*/; do tools/fm1song.sh compile $$s || exit 1; done

song-test:           ## M1a: the song compiler's tests (pytest, incl. fuzzing; ~5 s)
	cd tools/fm1song && ../../.venv/bin/python -m pytest -q
