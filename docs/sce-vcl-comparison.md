# openvcl against SCE vcl 1.4beta7

Measured 2026-10-23 with `main` at 20115e9 (every fix in [fixes.md](fixes.md)
merged) and the SCE vcl binary Tyra ships (`assets/vcl`, sha256 83bee752...),
both compiling the same preprocessed corpus inputs with no options.

## Summary

SCE vcl generates faster code than openvcl on every hot loop in the corpus:

- 1.1 to 1.4 times faster on quake2-ps2 and openvcl's own examples,
- 1.2 to 2.8 times faster on Tyra's vertex loops,
- 2.5 to 4 times faster on ps2gl's `--LoopCS` loops.

Its code is also smaller in every program except `billboard_renderer`,
`general` and `general_nospec`, where its software pipelining adds prologue and
epilogue copies of the loop. openvcl pads latencies with nops; SCE leaves them
to the hardware interlock, which costs the same cycles in less code.

Where openvcl is ahead is correctness: SCE reorders memory accesses through
different base registers even when they hit the same address (see
[below](#where-sce-is-wrong)).

## Method

- SCE vcl ran under emulation on the Mac ([sce-vcl/](sce-vcl/README.md)). Its
  optimiser stops each search on a wall-clock `alarm()` (4 s for straight-line
  code, 34-37 s for loops); the emulator delivers it after the same seconds of
  CPU time times 25, the emulator's slowdown, calibrated on Tyra's 15 programs
  (96 s in native CI, about 2,540 CPU-seconds emulated). Halving the scale gave
  byte-identical output on the one program checked (`stapip_as_is_td`).
- Both outputs were scored by one cycle model (`sce-vcl/vucycles.py`): VU
  manual latencies, per-field interlock stalls, Q/P/FDIV/EFU timing, and each
  loop's steady-state cycles per iteration on its fall-through path. It matches
  SCE's own per-block cycle estimates (the `_LNOPT_w`/`_LPOPT_w` comments) on
  389 of 396 blocks; the rest are within 2 cycles (program ends, and four
  prologue/epilogue blocks of ps2gl `general`).
- openvcl's `--cost` analyser was not usable for this: it does not model
  interlock stalls, so it undercounts SCE's code.

## Results

Cycles per loop iteration, ours / SCE. "pairs" is the program's size in
instruction pairs. SCE's renamed pipelined loops
(`EXPL_<file>_<stream>__MAIN_LOOP`) are listed under their source label.

| program | pairs ours | pairs SCE | loops, ours/SCE |
|---|---|---|---|
| examples sprite, triangle, triangle_strip | 43 | 25 | vertexLoop 31/27 |
| ps2gl-extra billboard_renderer | 102 | 128 | xform_loop 64/24 |
| ps2gl-extra scei | 287 | 204 | adcLoop 52/44, xform_loop 77/19 |
| ps2gl fast | 300 | 211 | adcLoop 52/44, xform_loop 60/16 |
| ps2gl fast_nolights | 155 | 139 | adcLoop 52/44, xform_loop 31/12 |
| ps2gl general | 478 | 660 | adcLoop 52/50, xform_loop 55/22, dir_light_vert_loop 64/18, pt_light_vert_loop 141/39, final_loop 14/5; per-light outer loops dir_light_loop 132/164, pt_light_loop 184/266 |
| ps2gl general_nospec | 446 | 450 | adcLoop 52/50, xform_loop 55/22, dir_light_vert_loop 31/8, pt_light_vert_loop 79/26, final_loop 14/5; outer dir_light_loop 74/78, pt_light_loop 175/137 |
| quake2 particles | 89 | 60 | lParticleLoop 56/40 |
| quake2 textured_triangles | 1827 | 998 | lFanLoop 116/85, clip-plane loops 87/77-79, lTriangleLoop path 356/305 |
| Tyra dynpip_c | 187 | 80 | vertexLoop 152/59 |
| Tyra dynpip_d | 332 | 137 | vertexLoop 299/107 |
| Tyra dynpip_tc | 221 | 98 | vertexLoop 180/64 |
| Tyra dynpip_td | 379 | 155 | vertexLoop 341/125 |
| Tyra mcpip_as_is | 57 | 48 | vertexLoop 37/30 |
| Tyra mcpip_cull | 169 | 99 | blocksLoop 158/88, vertexLoop 106/45 |
| Tyra stapip_as_is_c / _d / _tc / _td | 71 / 163 / 81 / 163 | 60 / 96 / 71 / 103 | vertexLoop 39/30, 129/67, 46/36, 125/70 |
| Tyra stapip_cull_c / _d / _tc / _td | 140 / 228 / 148 / 233 | 84 / 120 / 90 / 128 | vertexLoop 103/51, 193/84, 106/53, 196/88 |

The ps2gl per-light outer loops are counted with one pass of their inner
vertex loop, so SCE's larger prologue and epilogue dominate them; with more
than a vertex or two per light, SCE's per-vertex advantage wins.

Not measured: the nine other `general_*` variants and `indexed` (they reuse
`general`'s loops and take an hour or more each to emulate), the gasp example
(needs gasp), and the four repros SCE rejects (below).

With `--enable-known-loop-optimizations`, openvcl's hand-written ps2gl kernels
match SCE exactly on the `fast` transform loops (16 and 12 cycles), but they are
off by default and barely help `general` (xform 64, nospec dir_light_vert 34).

## Why SCE is faster

Largest effect first.

### 1. Real software pipelining of `--LoopCS` loops

SCE modulo-schedules these loops: a prologue, a kernel that overlaps the
stages of consecutive vertices, and an epilogue. In ps2gl `scei` the kernel
loads the next vertex while storing the previous one, both pointers advancing by
3, in 19 cycles. openvcl's generic pipeliner only carries the Q producer (the
`div`) and prefetches across the back edge, so its body still runs one vertex at
a time with 3-nop gaps: 77 cycles.

### 2. Overlap of independent work in straight-line code

Tyra's loops transform three vertices per trip, written out one after another.
SCE interleaves them; openvcl does not, even with distinct registers. The
reason is Q and the clip flags: the dependency graph
(`buildVuDependencyGraph`) treats Q as a pairwise resource and orders the clip
flags precisely, so vertex 2's `div` gets a write-after-read edge to vertex 1's
`mulq`. SCE uses the fact that neither is interlocked: Q keeps the old quotient
until the next divide completes, so it reads each quotient in the window
between its own divide landing and the next one landing. From `dynpip_c`:

```
mulax ACC,VF02,VF14x  div Q,VF00w,VF13w   ; vertex 1 divide, cycle 25 (Q ready 32)
mulax ACC,VF02,VF11x  div Q,VF00w,VF12w   ; vertex 2 divide, cycle 32 (Q ready 39)
mulq.xyz VF13,VF13,Q                      ; vertex 1 quotient, cycle 37
```

Likewise each `fcand` (which tests the last three clip judgements) issues once
its vertex's `clipw` is 4 cycles old and before the next one is. A cycle trace
confirmed SCE's schedule computes what the source order says.

Register choice adds to this: `processAliases` and its two-address chain
pre-pass give every live range the lowest-numbered free register, so the three
vertices land in the same VF11-VF13 and the scheduler sees false dependencies.
An experiment with round-robin choice gained a little (Tyra dynpip_tc 180 to
153, mcpip_cull 106 to 89) and ran out of registers on ps2gl `general*` and
quake2 `textured_triangles`; it is the smaller factor, and a real fix needs
schedule-aware allocation or renaming.

### 3. Small per-iteration overheads

The sprite example has the same dependency chain in both compilers, neither
pipelines it, and the 4-cycle gap is exactly these:

```
SCE, 10 pairs, 27 cycles                       openvcl, 25 pairs, 31 cycles
NOP     lqi VF05,(VI01++)                      nop     lqi VF05, (VI01++)
mulax   ACC,VF01,VF05x  isubiu VI04,VI04,1     ...
...                                            nop     sqi VF05, (VI02++)
NOP     ibne VI04,VI00,vertexLoop              nop     iaddi VI04, VI04, -1
NOP     sqi VF05,(VI02++)    <- delay slot     nop     nop      <- pre-branch bubble
                                               nop     ibne VI04, VI00, vertexLoop
                                               nop     nop      <- empty delay slot
```

- The pre-branch bubble (`padForBranchPreBubble` / `branchNeedsPreBubble`) is a
  nop pair before every conditional branch and register jump. The hardware only
  needs it when the instruction right before the branch sets the tested VI with
  an IALU op; elsewhere it is a wasted cycle per iteration.
- SCE hoists counter and pointer updates into stall shadows, rewrites the later
  store offsets (`-3(VI07)`, `-6(VI07)`) and puts a store in the delay slot;
  openvcl keeps the update last and leaves the slot empty.

### 4. One cycle of over-padding

openvcl's instruction table gives `move`, `mr32`, `mfir` and `ilw` latency 4,
against 3 for the FMAC ops and `lq`, and pads latency + 1: 5 cycles to a
consumer. The VU manual (7.4) gives 4 for all of them.

### Other SCE behaviour noticed

- It folds address constants: `iaddiu iWalk, vi00, 900` / `sq fC, 1(iWalk)`
  becomes `iaddiu VI01, VI00, 0` / `sq VF01, 901(VI01)`.
- It uses `isubiu` where the source has `iaddi` with a negative step.

## Where SCE is wrong

- **Memory aliasing.** SCE takes different base registers as proof that two
  accesses touch different memory. On `repro__store_load_alias` it folds
  `iWalk = 900` into the store offset and then hoists `lq 901(VI00)` above
  `sq 901(VI01)` with VI01 = 0, so the load reads stale data; on
  `repro__store_store_alias` the two stores to 901 land in the wrong order.
  This is what `fix/memory-alias` corrected in openvcl (at about 2 cycles on
  `dynpip_c`), and why quake2-ps2 needs `--barrier` with either compiler.
- **Flags across joins** are handled correctly: on `repro__mac_join` SCE puts 3
  nops at the join, so `fmand` is at least 4 cycles after the FMAC on both paths.
- **It rejects** programs that read the clip or MAC flags before any instruction
  sets them ("used before set"): `repro__clip_join`, `repro__clip_loop_entry`,
  `repro__clipw_implicit_w`, fjtrujy's `quad_adc_bug`. openvcl accepts them.
- **Its output depends on the machine.** The optimiser's searches stop on
  wall-clock alarms; several hit them (the "time out" warnings: 15 in `general`).
  A faster or slower machine can get different code.

## What would close the gap

1. Modulo scheduling for `--LoopCS` loops (or turning on and generalising the
   known-loop kernels).
2. Timing-aware handling of Q and the clip/MAC flags in the scheduler: allow a
   reader of Q to move past the next divide's issue when it stays before that
   divide's completion, and the same for flag readers and writers.
3. Register allocation that does not reuse a register as soon as it is free,
   or renaming before scheduling.
4. The pre-branch bubble only when the previous pair sets the tested VI; delay
   slot filling with stores and hoisted pointer updates.
5. Latency 4, not 5, after `move`, `mr32`, `mfir` and `ilw`.

## Reproducing

See [sce-vcl/README.md](sce-vcl/README.md). Our side is `main`'s `openvcl`
run on `openvcl-corpus/inputs/*.vcl` with no options.
