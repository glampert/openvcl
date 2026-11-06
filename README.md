# SCE vcl native corpus runs

This branch runs SCE's vcl 1.4beta7 natively on the 49 VU1 programs of the vcl-re corpus.
vcl-re is a clean-room reimplementation of vcl. It records the original's output under an
x86 emulator, where the watchdog alarm, the per-strategy time budget of the loop
scheduler, is measured in emulated CPU time. Here vcl runs at native speed, so its
`alarm(2)` budgets are real seconds. The time-outs and outputs show what an unemulated
run produces.

This branch is an orphan like `test/local-corpus`. It shares no history with `main`, is
never opened as a pull request, and is never merged.

## What runs

`.github/workflows/vcl-native.yml` runs on every push, or by hand with a different number
of runs or different vcl options:

- The job runs on an x86-64 GitHub runner, in an `ubuntu:20.04` container with
  `libc6:i386` and `libstdc++5:i386`, the same setup as `ci/sce-vcl-check`.
- vcl itself is not committed. It is fetched from [h4570/tyra](https://github.com/h4570/tyra)
  at c59f406 (`assets/vcl`) and checked against its SHA-256,
  `83bee75205b5b0f37dbefc2c2769eba2c353befa532472c49aedb2ebaf168274`.
- `run_corpus.sh` runs `vcl -t1 -uCORPUS NAME.vcl` on each input, in a directory holding
  only that input, three times. These are the options the corpus was recorded with.
- Four shards run in parallel. Each run keeps stdout, stderr, the exit status, wall
  seconds and its count of `time out..` lines. The results are uploaded as
  `vcl-native-shardN` artifacts.

## Programs and provenance

`inputs/manifest.json` lists every source of every input (repository, commit, path, and how
it was produced). It is vcl-re's `evidence/corpus/source-manifest.json`, and `sha256` there
identifies the source, not the compiled input. The table below comes from
`test/local-corpus`, whose `inputs/` are byte-identical to these except one:
`examples__triangle-gasp.vcl` here is the masp-preprocessed form that vcl-re's corpus
compiles, produced with `-g --gasp masp`, not the raw example.

| Inputs | Source (commit) | Files | How | License |
|---|---|---|---|---|
| `quake2-ps2__textured_triangles`, `quake2-ps2__particles` | [glampert/quake2-ps2](https://github.com/glampert/quake2-ps2) (12f2f9a); [glampert/quake-ps2](https://github.com/glampert/quake-ps2) (ac62ce9) gives the same output | `src/ps2/renderer/vu1progs/{textured_triangles,particles}.vcl`, with `vu_clip.i` and `vu_common.i` | `vclpp -I src/ps2/renderer/vu1progs -Wundef -Werror -j` | GPL-2.0 |
| `tyra__*` (15) | [h4570/tyra](https://github.com/h4570/tyra) (c59f406) | `engine/src/renderer/**/*.vclpp` | `vclpp`, run from `engine/` | Apache-2.0 |
| `ps2gl-extra__scei`, `ps2gl-extra__billboard_renderer` | [glampert/ps2gl](https://github.com/glampert/ps2gl) (794bedc) | `vu1/scei.vcl`, `examples/tricked_out/billboard_renderer.vcl` | `vclpp -m -x -f -I …` (masp mode) | LGPL-2.1 |
| `ps2gl-fixtures__*` (12) | ps2dev/openvcl's `test/fixtures/`, from [glampert/openvcl](https://github.com/glampert/openvcl) main (6f8607b) | `fast`, `fast_nolights`, `general`, `general_nospec`, `general_nospec_quad`, `general_nospec_tri`, `general_pv_diff`, `general_pv_diff_quad`, `general_pv_diff_tri`, `general_quad`, `general_tri`, `indexed` | copied | LGPL-2.1 |
| `examples__sprite`, `examples__triangle`, `examples__triangle-gasp`, `examples__triangle_strip` | ps2dev/openvcl's `examples/`, from glampert/openvcl main (6f8607b) | `examples/<name>/vu1*.vcl` | copied; triangle-gasp preprocessed by masp | AFL-2.0 |
| `repo__test_repro_quad_adc_bug` | ps2dev/openvcl, from glampert/openvcl main (6f8607b) | `test/repro/quad_adc_bug.vcl` | copied | AFL-2.0 |
| `fjtrujy-master__test_repro_quad_adc_bug` | [fjtrujy/openvcl](https://github.com/fjtrujy/openvcl) master (aa5e594) | `test/repro/quad_adc_bug.vcl`, the original with `0xffff` | copied | AFL-2.0 |
| `repro__*` (12) | glampert/openvcl `test/local-corpus` (5729d1f) | `repros/*.vcl` | copied | AFL-2.0 |

- **quake2-ps2 and quake-ps2**, by Guilherme Lampert, are PS2 ports of Quake II and Quake.
  They are under GPL-2.0, like the id Software sources they build on.
- **Tyra**, by Sandro Sobczyński: each program's header reads "Copyright 2022, tyra -
  https://github.com/h4570/tyra. Licensed under Apache License 2.0". The repository has
  no LICENSE file, so `licenses/Apache-2.0.txt` is the text from apache.org.
- **ps2gl**, by Sony Computer Entertainment America, is under LGPL-2.1 (its headers read
  "Copyright (C) 2000,2001,2002 Sony Computer Entertainment America"). The 12 fixtures are
  ps2gl's `vu1/*.vcl` renderers with their `.i` includes expanded. Francisco Javier
  Trujillo Mata committed them to ps2dev/openvcl without the header (3d8ee88).
- **openvcl**: the examples' headers read "Copyright (C) 2004 Jesper Svennevid, Daniel
  Collin. Licensed under the AFL v2.0." `test/repro/quad_adc_bug.vcl`, by Francisco
  Javier Trujillo Mata, and the `repro__*` programs have no header of their own, so the
  repository's AFL-2.0 applies.

Each input keeps the license of its source. The license texts are in `licenses/`. vcl is
SCE's and is not distributed here. The workflow, `run_corpus.sh` and this README are
under AFL-2.0, like the rest of openvcl.
