# Bugs found and their fixes

quake2-ps2 (and quake-ps2) build their VU1 programs with
`vclpp -> openvcl -> dvp-as` and run [vu-checker](https://github.com/glampert/vu-checker)
on the result to catch silent miscompiles. Each problem below was reproduced
with ps2dev/openvcl v0.4.0 (what ps2toolchain-dvp installs), fixed on its own
`fix/<name>` branch off `upstream/master`, and merged into our `main`. Status as
of 2026-10-23; see [upstream-workflow.md](upstream-workflow.md) for the process.

Every fix keeps the corpus byte-identical except where the change is intended
and listed (see [testing.md](testing.md)), and adds integration tests that fail
on master.

| # | Problem | Branch | Upstream |
|---|---|---|---|
| 1 | `clipw.xyz a, b` without `[w]` rejected; SCE accepts it (9 of Tyra's 15 programs) | fix/clipw-implicit-w | merged, PR #2 |
| 2 | Immediates silently truncated by dvp-as | fix/immediate-ranges | merged, PR #3 |
| 3 | Diagnostics: no `error:` tag, out-of-registers without a name or line, a failed build left a fresh .vsm | fix/diagnostics | merged, PR #4 |
| 4 | `-c` changed the code (extra nop pairs) and left instructions uncommented | fix/emit-source | merged, PR #5 |
| - | ASan/UBSan job in CI | ci/sanitizers | merged, PR #6 |
| 5 | Range ends past the program when a branch is its last token (`tokens.end()` dereferenced) | fix/trailing-branch-range | merged, PR #7 |
| 6 | Loop-carried values lose their live range past an inner branch | fix/loop-liveness (7a) | not submitted |
| 7 | Partial writes joined to the old value although the kept lanes are dead (Tyra stapip_cull_td ran out of VF registers) | fix/partial-write-ranges (7b, on 7a) | not submitted |
| 8 | Software pipeliner reuses registers live across the loop | fix/pipeline-live-registers | open, PR #8 |
| 9 | `--exit` outputs lost on taken-branch paths, and their registers reused before the exit | fix/exit-outputs (on 7b) | not submitted |
| 10 | Flag, VI-test and register latencies not carried across labels and branches | fix/cross-block-latency | not submitted |
| 11 | Memory accesses through different base registers reordered | fix/memory-alias | experimental, ours only |

## Details

**1. clipw.** The CLIPw operand templates now take a second operand without a
field as the `w` broadcast, emitting what `b[w]` emits; other fields are still
rejected. Checked against SCE vcl in CI (`ci/sce-vcl-check`).

**2. Immediate ranges.** Decided from SCE vcl's behaviour: `iaddi` outside
-16..15 becomes `iaddiu`/`isubiu` of the magnitude, beyond +-32767 an error;
`iaddiu`/`isubiu` 0..32767, `fc*` 24 bits and `fs*` 12 bits are errors when out
of range; `lq/sq/ilw/isw` offsets outside -1024..1023 warn (VU1 wraps at 1024
quadwords). Scheduler rewrites that change immediates are guarded. Second
commit: `--warnings-as-errors`.

**3. Diagnostics.** `file(line) : error: ...`; "Register allocation ran out of
VF registers (31 available) for outputColor3" at the first line using it; no
output file after an error; two UBSan finds fixed (uninitialised
`m_newSyntax`, signed gasp temp name).

**4. -c.** Comments are kept apart from the code lines, so the pre-branch
bubble check no longer sees a comment; every emitted instruction gets its
`; Line N:` comment.

**5. Trailing branch range.** `processBranchState` used `*curr` after a branch
that was the program's last token; ps2gl programs end with `b main_loop_lid`.
Output unchanged; only `--show-reg-alloc` printed garbage.

**6. Loop liveness (7a).** `processBranchState` stops a path at a branch it has
taken before or past `--bthres`, so back-edge paths were never walked past the
first inner branch, and a later temporary took a loop-carried register (repro:
`isw.x iCount, 0(iPtr)` / `iaddiu iPtr, iPtr, 1` / `iaddiu iT2, vi00, 7`, iPtr
and iT2 both VI01). `completeLiveRanges` now walks each block's CFG back from
every read to the writes that reach it, per lane, and joins them. Fixed real
miscompiles in ps2gl `general*` (material_spec overwritten in the inner light
loop) and Tyra (gifSetTag clobbered across MSCNT continuations).

**7. Partial-write ranges (7b).** A partial write was joined to the earlier
value whenever the variable had other lanes, so values stayed live for the
whole program; now only if a kept lane is read later. Tyra stapip_cull_td
compiles. Should be submitted after the pipeliner fix, since it exposes more of
that bug in ps2gl `*_quad`/`*_tri`.

**8. Pipeline live registers.** `assignRotationScratchRegisters` and the kernel
renames treated a VF register the loop body does not name as free; registers
live at the loop's entry or exit are now reserved. The env-gated kernel
rewrite's rename pool is not covered (path not enabled).

**9. Exit outputs.** `BranchState`'s copy constructor dropped the outputs, and
an output's range ended at its last access; the exit now reads every
`out_vf`/`out_vi`, pinning the register, with a conflict error when an input
must be in two registers. No corpus program uses outputs.

**10. Cross-block latency.** The emitter's `VuLatencyTracker` reset at labels
and branches, and the fall-through carry was a cycle short. Now each branch
records what its target inherits, labels merge it, and the program is
re-emitted until those records settle. Per the VU manual only the flag, Q and
branch-condition cases were silent miscompiles; register cases are interlocked
and timing-neutral (see [vu-hazards.md](vu-hazards.md)). 12 ps2gl programs gain
nop pairs.

**11. Memory aliasing.** Ported from ps2dev's `optimizations` branch (ab670c7,
aafeb58, by fjtrujy): different base registers are no alias proof, and VI
registers are tracked as root plus constant through `iaddiu`/`isubiu` so
related addresses still compare. Our port also fixes a hole in upstream's
pipeliner check: a hoisted next-iteration load was compared with the delayed
store without the loop step, so `lq -1(vi03)` after `sq 0(vi03)` / step 1 was
hoisted above the store it reads. 21 of 46 corpus programs change; SCE vcl has
the same bug (see [sce-vcl-comparison.md](sce-vcl-comparison.md#where-sce-is-wrong)).

## Not openvcl, or not fixed

- quake2 c676556 (alpha 1 instead of 128 after masked and unmasked writes
  across a branch) is not explained by a late `move.w`: the store would stall
  for it. Locked by a test in `test_cross_block_latency.cpp`.
- openvcl leaks its register allocation data (Alias/Range/Dependency); not being
  fixed, so LeakSanitizer stays off.
- vu-checker false positives (tabbed source lines, flag-test registers, mr32
  lanes, LOI immediates decoded as `bal`, waitq counted as one cycle) were fixed
  in vu-checker itself.
- Out of scope from openvcl 0.3.3: output parameters at the "proper" exit,
  gasp file-name tracking.
- Measured codegen gaps against SCE vcl: [sce-vcl-comparison.md](sce-vcl-comparison.md).
