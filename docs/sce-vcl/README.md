# SCE vcl under emulation, and a VU1 cycle model

The tools behind [../sce-vcl-comparison.md](../sce-vcl-comparison.md).

SCE vcl 1.4beta7 is a 32-bit i386 Linux binary; the Mac is arm64 with no
Docker or VM. These scripts run it under [Qiling](https://github.com/qilingframework/qiling)
(a Python framework on the Unicorn CPU emulator) with an i386 rootfs taken from
Ubuntu 20.04 packages, the system Tyra's Dockerfile runs vcl on.

The CI branch `ci/sce-vcl-check` also runs vcl natively, but it can only report
small results through annotations: artifacts and logs need a token, and
publishing outputs to a branch from CI was refused as data exfiltration.

## Setup

    docs/sce-vcl/setup.sh

builds `$SCEVCL_HOME` (default `~/.cache/openvcl-scevcl`): a venv with qiling
(keystone replaced by a stub, since its native library does not load and vcl
never needs an assembler), the rootfs (`libc6`, `libstdc++5`, `libgcc-s1`,
checked by SHA-256), Tyra's `assets/vcl` (from `$TYRA`, checked by SHA-256) and
`info.json`, openvcl's instruction table (`--dump-instruction-info-json`).

## Running vcl

    $SCEVCL_HOME/venv/bin/python run_vcl.py $SCEVCL_HOME/rootfs prog.vcl out.vsm out.err [vcl args]
    JOBS=8 $SCEVCL_HOME/venv/bin/python run_all.py OUTDIR 'tyra__*'

`run_all.py` compiles the corpus inputs (`$CORPUS_INPUTS`, default
`~/Repos/GitHub/openvcl-corpus/inputs`) matching the pattern, and writes
`.vsm`, `.err` and a `.status` line per program (exit status, CPU time, alarms
armed and fired). The `.status` comes from the parent: stopping `run_all.py`
leaves the running children to finish without one.

**The optimiser timeout.** vcl bounds each search with `alarm()`; its SIGALRM
handler (at 0x804ac30) only stores 1 through the pointer at 0x80bbd90, which
the search polls. Qiling's `alarm` does nothing, so without help the searches
never stop (ps2gl programs ran over 13 minutes). `run_vcl.py` arms a deadline
of n x `VCL_TIME_SCALE` seconds of process CPU time (default 25, the measured
emulator slowdown against a native x86 runner) and a watcher thread does the
handler's store. CPU time, not wall time, so running many at once does not
change the result. `probe_handler.py` is how the handler was found and
disassembled; `measure_speed.py` counts executed code to estimate the slowdown.

Costs at scale 25: Tyra's 15 programs about 2,500 CPU-seconds in all, most in
the four `dynpip_*`; ps2gl's programs 8 to 40 CPU-minutes each.

## The cycle model

    python3 vucycles.py report prog.vsm        # loops and streams
    python3 vucycles.py trace prog.vsm LABEL   # one steady-state iteration, pair by pair
    python3 compare.py OURS_DIR SCE_DIR...     # per program, ours vs SCE

`vucycles.py` reads both openvcl's and SCE's `.vsm`. Timing follows the VU
manual (7.4, 3.4): VF results and VI loads readable 4 cycles after issue, a
read before that stalls the pair field by field; IALU results the next cycle;
Q and P not interlocked (only WAITQ/WAITP wait), FDIV and EFU busy for their
throughput; ACC and flags never stall; `xgkick`'s GIF wait not modelled.
Loops are measured as steady-state cycles per iteration on the fall-through
path (inner forward branches not taken, inner loops run once). For SCE's
output it also checks itself against SCE's own per-stream estimates.

`compare.py` pairs SCE's renamed pipelined loops
(`EXPL_<file>_<stream>__MAIN_LOOP`) with the source's `--LoopCS` loops: by name
when the stream is the loop's own label, otherwise in order.
