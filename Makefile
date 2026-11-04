# Licence: TBD (see D017)
# Everything here runs on the laptop and in the emulator only; nothing writes to the FM-1.
.PHONY: firmware emu shot patches

firmware:            ## build firmware/app/build/fm1acc.fwsc
	firmware/build.sh

emu:                 ## build, then play it in the fm1-emu window
	tools/emu.sh

shot:                ## build, run 2.5 s headless with C4 held: build/emu/panel.png, build/emu/audio.raw
	tools/emu.sh --shot

patches:             ## apply our local patches to third_party/ checkouts (fm1-emu flash id) and rebuild them
	tools/apply_patches.sh
