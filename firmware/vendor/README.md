# Third-party code (vendored)

Other people's code, copied in unchanged except for the patches listed below, each folder with its own licence
files. Our own code is in `firmware/app/` (Licence: TBD, see D017). Copied 2026-11-04 (Step 0.6, D016).

| Folder | Upstream | Commit | Licence |
|---|---|---|---|
| `fm1-base/` | https://github.com/charlesvestal/fm1-base | `2d8ed748cdade582c65d9fc8d8cf17da4ddadec8` (2026-11-01) | MPL-2.0 (`fm1-base/LICENSE`, `LICENSING.md`; fonts SIL OFL 1.1) |
| `fm1-base/bsp/` | https://github.com/hugelton/felucca-bsp (fm1-base's pinned submodule) | `773b0b99f5ade1a30d1879b2618c6c3c3e6ba031` | MPL-2.0 (`fm1-base/bsp/LICENSE`, `LICENSING.md`) |
| `sonivox/` | https://github.com/EmbeddedSynth/sonivox (subset: `arm-wt-22k/lib_src`, `arm-wt-22k/host_src`, licence files, README) | `86bd07341078084cb9f224b9e7d508fbf4751af0` (2026-10-02, v4.0.x) | Apache-2.0 (`sonivox/LICENSE-2.0.txt`, `NOTICE`, `arm-wt-22k/NOTICE`); GM sound set `wt_200k_G` © 2009 Sonic Network Inc., Apache-2.0 |

These are the commits evaluated in Step 0.5 (fm1-base, felucca-bsp) and read in Step 0.6 (sonivox).
How they were copied: `git archive` of each commit (tracked files only, no `.git`).

The JieLi AC79 SDK files the build puts in the package (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`, Apache-2.0)
are read from `third_party/jieli/fw-AC79_AIoT_SDK` at build time and are not copied here.

## Local patches

| Patch | Files | Why |
|---|---|---|
| `patches/fm1-base-flash-id.patch` | `fm1-base/device/main_fm1.c` | The app accepts flash 0x856014 (Puya) **and 0xB36014** (UCUN UC25HQ80); any other id: RAM only, as before (CLAUDE.md flash rule) |
| | `fm1-base/bsp/loader/ldr_core.c`, `fm1-base/bsp/loader/loader.c` | The update loader **refuses an unknown flash id** (error -14) after the chip-key check, **before anything is erased**; it accepts 0x856014 and 0xB36014 (felucca-bsp's loader checked no id) |
| | `fm1-base/tests/ldr_test.c` | The loader's host test gets the new hook |

Each change is marked `fm1-accompaniment:` in the source. MPL-2.0 applies to these files as modified: the patch
is published with them. Applied by hand to the vendored copy; to move to a newer upstream, copy the new commit and
apply the patch again (`patch -p1 -d firmware/vendor/fm1-base < firmware/vendor/patches/fm1-base-flash-id.patch`).

Not copied, by decision D017: Polly's modifications of EAS and any GPL code (Polly, TRIO's references, fm1-chord,
CoralFlower, Felucca). Our EAS configuration and host layer are our own (`firmware/app/eas/`).
