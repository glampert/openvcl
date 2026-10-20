# openvcl local corpus

The VU1 programs and the harness used to check openvcl fixes. A fix must leave the code
openvcl generates for these programs byte-identical, unless the change is intended. This is
the orphan branch `test/local-corpus` of glampert/openvcl: it shares no history with `main`,
is never opened as a pull request, and is never merged.

## Setup

Check the branch out next to the openvcl repository:

    git -C ~/Repos/GitHub/openvcl worktree add ../openvcl-corpus test/local-corpus

What the commands need:

| Command | Needs | Looked for at |
|---|---|---|
| all | Python 3 | |
| `run` | dvp-as, to assemble the outputs for vu-checker | `$PS2DEV/dvp/bin/dvp-as` |
| `run` | [vu-checker](https://github.com/glampert/vu-checker) | `$CORPUS_REPOS/vu-checker` |
| `run`, `sanitize` | masp, for the triangle-gasp example | `$PS2DEV/bin/masp` |
| `sanitize` | CMake, and a compiler with ASan and UBSan | |
| `prepare` | the source checkouts listed under [Programs and provenance](#programs-and-provenance) | `$CORPUS_REPOS/<name>`, openvcl at `$OPENVCL_REPO` |
| `prepare` | vclpp 2: [glampert/vclpp](https://github.com/glampert/vclpp) 95911cd, the `src/tools/vclpp` submodule of quake2-ps2, as built there | `$CORPUS_REPOS/quake2-ps2/build/tools/vclpp` |

`CORPUS_REPOS` defaults to `~/Repos/GitHub`, `OPENVCL_REPO` to `$CORPUS_REPOS/openvcl` and
`PS2DEV` to `~/ps2dev`. The first `prepare` clones fjtrujy/openvcl into `fjtrujy-openvcl/`.
Each command writes to the directory it is given; `out/` is ignored, for that use.

## Commands

### Compare two builds: `run`

    python3 harness.py run BASE CAND out/NAME

BASE and CAND are openvcl binaries. BASE is usually upstream master:

    git -C ~/Repos/GitHub/openvcl worktree add --detach ../openvcl-baseline upstream/master
    cmake -S ../openvcl-baseline -B ../openvcl-baseline/build -DCMAKE_BUILD_TYPE=Release
    cmake --build ../openvcl-baseline/build      # writes ../openvcl-baseline/openvcl

openvcl's build writes the binary to the root of its source tree, so copy a binary elsewhere
before building another configuration of the same tree.

Both binaries compile every input, plain and with `-c`. The plain outputs are assembled and
checked with vu-checker. Each program gets one line:

| Status | Meaning |
|---|---|
| `identical` | both compile it to the same plain `.vsm` |
| `CHANGED` | the plain outputs differ: acceptable only when intended, and listed in the PR |
| `NEW` | only CAND compiles it |
| `failing` | neither compiles it |
| `BROKEN` | only BASE compiles it; `run` exits with status 1 |

| Note | Meaning |
|---|---|
| `-c CHANGES CODE` | CAND's `-c` output, without comments, differs from its plain output |
| `-c output differs` | BASE and CAND write different `-c` output |
| `vu-checker fixed N`, `NEW`, `still` | findings that CAND's output no longer has, newly has, or still has |

vu-checker's `branches` check is ignored, since it reads LOI's float immediate as a branch.
Its regalloc check reads the `-c` output next to each object, so its findings can change
when only the `-c` output does.

### ASan and UBSan: `sanitize`

    python3 harness.py sanitize ~/Repos/GitHub/openvcl out/sanitize

Copies the tree's tracked and untracked files to `out/sanitize/src`, so uncommitted changes
are tested too and the tree itself is not touched. It builds the copy twice, Release and
Debug with `-fsanitize=address,undefined`, and runs the test suite on the sanitized build:
the unit tests in process, the integration tests through the sanitized `openvcl`. Then both
binaries compile the corpus.

It passes when the tests pass, every program compiles the same way with both builds (plain
and `-c`), and the sanitizers report nothing. Reports go to `out/sanitize/logs/`; the summary
lists each distinct one with a count and a log that has it.

It takes about 5 minutes on an 8-core Mac, most of it the sanitized openvcl compiling the
corpus. On upstream master (c686067) it reports the two UBSan findings that PR #4 fixes: an
uninitialized `bool` read at `Tokenizer.inl:82`, and a left shift that overflows `int` in the
gasp temporary file name at `Parser.cpp:2174`.

### Refresh the inputs: `prepare`

    python3 harness.py prepare

Rebuilds `inputs/` from the current checkouts. `inputs/manifest.json` records, for each
program, the repository and commit it came from, its path, how it became an input, and
`modified` when that checkout had local changes. Programs with the same text and flags
become one input that lists all its sources. Commit a refresh only on purpose: results from
before it no longer compare with results after it.

## Layout

| Path | Contents |
|---|---|
| `harness.py` | the commands above |
| `inputs/` | the corpus: one self-contained `.vcl` per distinct program, as openvcl reads it, and `manifest.json` |
| `repros/` | the bug repros; `prepare` copies them into `inputs/` as `repro__*` |
| `licenses/` | the license texts that cover the programs |

## Programs and provenance

Snapshot of 2026-10-20: 84 programs, 49 distinct inputs. `inputs/manifest.json` has the same
information for every program.

| Inputs | Source (commit) | Files | How | License |
|---|---|---|---|---|
| `quake2-ps2__textured_triangles`, `quake2-ps2__particles` | [glampert/quake2-ps2](https://github.com/glampert/quake2-ps2) (12f2f9a) | `src/ps2/renderer/vu1progs/{textured_triangles,particles}.vcl`, including `vu_clip.i` and `vu_common.i` | `vclpp -I src/ps2/renderer/vu1progs -Wundef -Werror -j` | GPL-2.0 |
| the same two | [glampert/quake-ps2](https://github.com/glampert/quake-ps2) (ac62ce9) | the same paths; the sources differ, but vclpp's output does not | the same | GPL-2.0 |
| `tyra__*` (15) | [h4570/tyra](https://github.com/h4570/tyra) (c59f406) | `engine/src/renderer/**/*.vclpp`, listed below | `vclpp`, run from `engine/` | Apache-2.0 |
| `ps2gl-extra__scei`, `ps2gl-extra__billboard_renderer` | [glampert/ps2gl](https://github.com/glampert/ps2gl) (794bedc, also jrandis/ps2gl's master) | `vu1/scei.vcl`, `examples/tricked_out/billboard_renderer.vcl` | `vclpp -m -x -f -I …`: masp mode | LGPL-2.1 |
| `ps2gl-fixtures__*` (12) | ps2dev/openvcl's `test/fixtures/`, from [glampert/openvcl](https://github.com/glampert/openvcl) main (6f8607b) | `fast`, `fast_nolights`, `general`, `general_nospec`, `general_nospec_quad`, `general_nospec_tri`, `general_pv_diff`, `general_pv_diff_quad`, `general_pv_diff_tri`, `general_quad`, `general_tri`, `indexed` | copied | LGPL-2.1 |
| `examples__sprite`, `examples__triangle`, `examples__triangle-gasp`, `examples__triangle_strip` | ps2dev/openvcl's `examples/`, from glampert/openvcl main (6f8607b) | `examples/<name>/vu1*.vcl` | copied; triangle-gasp is compiled with `-g --gasp $PS2DEV/bin/masp` | AFL-2.0 |
| `repo__test_repro_quad_adc_bug` | ps2dev/openvcl, from glampert/openvcl main (6f8607b) | `test/repro/quad_adc_bug.vcl`, with PR #3's `0x7fff` | copied | AFL-2.0 |
| `fjtrujy-master__test_repro_quad_adc_bug` | [fjtrujy/openvcl](https://github.com/fjtrujy/openvcl) master (aa5e594); its ps2gl branch (3e6ac4f) has the same file | `test/repro/quad_adc_bug.vcl`, the original, with `0xffff` | copied | AFL-2.0 |
| `repro__*` (12) | this branch | `repros/*.vcl`, described below | copied | AFL-2.0 |

fjtrujy/openvcl's other programs, on master (aa5e594) and ps2gl (3e6ac4f), are byte-identical
to ps2dev/openvcl's fixtures and examples, so the manifest lists them as further sources of
those inputs.

- **quake2-ps2 and quake-ps2**, by Guilherme Lampert: PS2 ports of Quake II and Quake, under
  GPL-2.0 like the id Software sources they build on. The VU programs carry no copyright
  line of their own.
- **Tyra**, by Sandro Sobczyński: each program's header reads "Copyright 2022, tyra -
  https://github.com/h4570/tyra. Licensed under Apache License 2.0". The repository has no
  LICENSE file, though its README also names Apache License 2.0; `licenses/Apache-2.0.txt`
  is the text from apache.org. The programs, under `engine/src/renderer/`:
  - `3d/pipeline/dynamic/core/programs/dynpip_{c,d,tc,td}_vu1.vclpp`
  - `3d/pipeline/minecraft/programs/as_is/mcpip_as_is_vu1.vclpp`
  - `3d/pipeline/minecraft/programs/cull/mcpip_cull_vu1.vclpp`
  - `3d/pipeline/static/core/programs/as_is/stapip_as_is_{c,d,tc,td}_vu1.vclpp`
  - `3d/pipeline/static/core/programs/cull/stapip_cull_{c,d,tc,td}_vu1.vclpp`
  - `core/paths/path1/draw_finish.vclpp`
- **ps2gl**, by Sony Computer Entertainment America: the headers read "Copyright (C)
  2000,2001,2002 Sony Computer Entertainment America. This file is subject to the terms and
  conditions of the GNU Lesser General Public License Version 2.1." The 12 fixtures are
  ps2gl's `vu1/*.vcl` renderers with their `.i` includes expanded, committed to
  ps2dev/openvcl without the header by Francisco Javier Trujillo Mata (3d8ee88, "Some vcl
  files").
- **openvcl**: the examples' headers read "Copyright (C) 2004 Jesper Svennevid, Daniel Collin.
  Licensed under the AFL v2.0." `test/repro/quad_adc_bug.vcl` is by Francisco Javier
  Trujillo Mata (07a3196) and has no header, so the repository's AFL-2.0 applies.

### Repros

Minimal programs for the bugs that quake2-ps2 and vu-checker found in openvcl 0.4.0, written
for these fixes. Their status on 2026-10-20:

| Repro | What it shows | Status |
|---|---|---|
| `clipw_implicit_w` | `clipw.xyz a, b` without `[w]`, which SCE vcl accepts | fixed by PR #2, merged upstream |
| `immediates_out_of_range` | `iaddi -18`, `iaddiu 40000`, `isubiu -1` and `isw.x 2000(vi00)`, which dvp-as truncated silently | fixed by PR #3: errors and warnings, so it fails to compile on purpose |
| `loop_pointer` | a loop pointer read after an inner branch shares its register with a later temporary | open: loop liveness in the register allocator |
| `crossloop_4regs` | a value set in one loop and read in a sibling loop, with 4 VI registers | open: loop liveness |
| `clip_join` | `clipw` on both sides of a join reaches `fcand` 3 cycles later on one path; 4 are needed | open: latency across blocks |
| `clip_loop_entry` | `clipw` before a loop reaches the `fcand` at the loop top 2 cycles later | open: latency across blocks |
| `mac_join` | a MAC flag read right after a join | open: latency across blocks |
| `ilw_branch_label` | an `ilw` result read by a branch across a label | open: latency across blocks |
| `store_store_alias` | `sq` through `iWalk` (900) moved after an `sq` to 901(vi00), the same address | open: memory ordering, which upstream's `optimizations` branch changes |
| `store_load_alias` | `lq` from 901(vi00) moved above the `sq` through `iWalk` that writes it | open: memory ordering |
| `seed_store_order` | quake2-ps2's seed stores, then a store to one of their addresses through vi00 | open: memory ordering |
| `masked_write_branch` | a full `move`, then masked writes on one side of a branch | not reproduced: openvcl keeps the order; kept as a check |

## Results on main, 2026-10-20

glampert/openvcl main at 6f8607b, which has PRs #2 to #5, compiles 46 of the 49 inputs. The
other three fail as expected:

- `tyra__stapip_cull_td_vu1`: "Register allocation ran out of VF registers (31 available)
  for outputColor3", at line 171. SCE vcl compiles it; openvcl keeps partly written values
  live for too long.
- `repro__immediates_out_of_range` and `fjtrujy-master__test_repro_quad_adc_bug`: immediates
  out of range, rejected since PR #3.

vu-checker still reports regalloc findings on the ps2gl `general`, `general_pv_diff`,
`general_pv_diff_quad`, `general_pv_diff_tri`, `general_quad` and `general_tri` fixtures and
on `ps2gl-extra__scei` and `ps2gl-extra__billboard_renderer`; latency on
`repro__clip_join` and `repro__clip_loop_entry`; and crossloop on `repro__crossloop_4regs`.

`harness.py sanitize` passes on main: the tests and the corpus run clean under ASan and UBSan.

## Licenses

A summary, not legal advice; the texts in `licenses/` are what count.

| License | Covers | When copying or changing these files |
|---|---|---|
| [AFL-2.0](licenses/AFL-2.0.txt), the Academic Free License 2.0 | openvcl's examples and repro, the fjtrujy/openvcl copies, and this branch's own files: `harness.py`, `repros/` and this README | keep the copyright, license and attribution notices |
| [LGPL-2.1](licenses/LGPL-2.1.txt) | the ps2gl programs: `ps2gl-extra__*` and `ps2gl-fixtures__*` | include the license and keep the notices; changed files must say what changed and when, and stay under the LGPL |
| [GPL-2.0](licenses/GPL-2.0.txt) | the quake2-ps2 and quake-ps2 programs | include the license and keep the notices; changed files must say what changed and when, and stay under GPL-2.0 |
| [Apache-2.0](licenses/Apache-2.0.txt) | the Tyra programs | include the license and keep the notices; changed files must say they were changed |

The inputs from vclpp are changed files in that sense: vclpp expanded their includes and
dropped their comments, the copyright headers among them. This README is the notice of that
change. It repeats those headers above, and the original files are at the listed commits.
