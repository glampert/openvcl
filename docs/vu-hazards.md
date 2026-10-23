# VU hazards: what the hardware interlocks

From the VU User's Manual (SCE, v6.0), sections 3.3.5, 3.4.1, 3.4.8, 7.4 and
figures 3-16/3-17. A local markdown copy is in
`~/Repos/GitHub/PS2Docs/markdown/VU_Users_Manual.md` (and
`vu-instruction-manual.md`).

## Interlocked: a short distance costs cycles, never a wrong value

- **VF results** of FMAC ops, `lq`/`lqi`/`lqd`, `move`, `mr32`, `mfir`, `mfp`,
  `itof`/`ftoi`, `rget`/`rnext`: readable 4 cycles after issue. A read before
  that stalls the whole pair, field by field. VF00 is exempt.
- **VI loads** (`ilw`, `ilwr`): 4 cycles, stall the same way.
- **IALU results** are ready the next cycle.
- **FDIV and EFU busy**: a new `div`/`sqrt`/`rsqrt` waits for the previous one
  (throughput 7, 7, 13); likewise the EFU ops.
- ACC accumulate chains (`mula`, `madda`... `madd`) issue back to back.

## Not interlocked: read silently as the old value

- **MAC, status and clip flags** are set at the S stage, 4 cycles after issue.
  Only `add/sub/mul/madd/msub` (and their a/bc/i/q forms) and `opmula/opmsub`
  set MAC and status; `abs`, `max`, `mini`, `ftoi`, `itof` and `clip` do not.
  `clip` sets only the clip flags (a shift register: each `clipw` shifts the
  previous judgements by 6 bits). `div` sets status D/I.
- **Q and P**: a reader gets the old value until the divide (EFU op)
  completes, 7 cycles after `div`; only `waitq`/`waitp` wait. A `waitq` stalls
  until Q lands, so instructions before it are that much further from reads
  after it.
- **Branch conditions**: a conditional branch reads its VI a stage early, so a
  value set by an IALU op in the instruction right before it is not seen (flag
  tests excepted; loads stall).

## Consequences

- Across block boundaries, only flags, Q/P and branch conditions can be
  silently wrong. Register padding across blocks is timing-neutral: a nop
  instead of a stall.
- SCE vcl relies on the interlocks (it emits no nops for register latencies and
  marks them `STALL_LATENCY ?n`), and uses the non-interlocked timing on
  purpose: it reads a quotient between its divide landing and the next one
  landing ([sce-vcl-comparison.md](sce-vcl-comparison.md)).
- openvcl pads every latency with nops, and its table makes `move`, `mr32`,
  `mfir` and `ilw` one cycle longer than the manual's 4.

## vu-checker

[vu-checker](https://github.com/glampert/vu-checker) (`check_vu_code.py`)
checks crossloop, regalloc (reaching definitions from the `-c` comments),
loopvar, immediates, latency (clip/MAC/status flags at 4 cycles, Q with per-path
`waitq` credit, branch conditions; not register RAW, which is interlocked) and
branches (decodes `.vutext`, skipping LOI immediates). It needs `.pp.vcl`,
`.vsm`, `.c.vsm` and `.o` for each program. quake2-ps2 and quake-ps2 pin it as
the submodule `src/tools/vu-checker`.
