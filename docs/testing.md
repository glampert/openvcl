# Testing

## Unit and integration tests

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ctest --test-dir build --output-on-failure

The build writes `./openvcl` at the root of the source tree; the integration
tests fork it. Copy the binary elsewhere before building another configuration
of the same tree.

## The local corpus

The orphan branch `test/local-corpus` (worktree `../openvcl-corpus`, pushed to
origin, never merged or PR'd) holds 49 preprocessed VU1 programs: quake2-ps2 and
quake-ps2, Tyra's 15, ps2gl's fixtures plus `scei` and `billboard_renderer`,
openvcl's and fjtrujy/openvcl's examples, and our bug repros. It keeps them
off the PR branches because they carry GPL-2.0, LGPL-2.1, Apache-2.0 and
AFL-2.0 code; its README has the provenance and licences.

    python3 harness.py run BASE CAND out/NAME     # compare two openvcl builds
    python3 harness.py sanitize TREE out/NAME     # ASan/UBSan build of TREE, tests and corpus
    python3 harness.py prepare                    # refresh inputs/ from the source checkouts

`run` compiles every input with both binaries, plain and with `-c`, and
assembles and vu-checks the candidate's output. Per program: `identical`,
`CHANGED` (allowed only when intended), `NEW`, `failing` or `BROKEN` (exit
status 1), plus notes for `-c` changing the code and vu-checker findings fixed
or new. BASE is normally `../openvcl-baseline/openvcl`, a build of
upstream/master.

## Sanitizers on macOS

- Apple clang's ASan at -O1 gets some child `openvcl` runs SIGKILLed by the
  kernel (exit 137, `EXC_GUARD` in `/usr/bin/log show`); not an openvcl bug.
  Use Homebrew LLVM (`/opt/homebrew/opt/llvm/bin/clang++`) for -O1, or -O0.
- LeakSanitizer works only with Homebrew clang. openvcl leaks its allocator
  data, so leak checks fail about 130 tests; CI runs with `detect_leaks=0`.
- Sanitized openvcl is slow on the ps2gl fixtures (up to about 90 s each at
  -O0); CI stays at -O0 because GCC at -O1 builds 7x slower on the runner.
- The shell here is zsh: `log` is a builtin, `status` is read-only, `echo ====`
  fails, and `$c:file` is a modifier (write `${c}:file`).

## SCE vcl

- **CI, native**: `ci/sce-vcl-check` (worktree `../openvcl-ci`) runs Tyra's
  vcl 1.4beta7 on `cases/*.vcl` and Tyra's programs in an Ubuntu 20.04
  container on every push. Results come back through check-run annotations
  (`/actions/runs/<id>/jobs` then `/check-runs/<job>/annotations`, no token
  needed), 6 cases per notice; artifacts and logs need a token. vcl requires
  flags to be set before they are read and rejects `waitq` outside raw mode.
- **Local, emulated**: [sce-vcl/](sce-vcl/README.md), for full outputs.

## quake2-ps2 and quake-ps2

`make clean_vu && make` rebuilds their VU programs and runs vu-checker on each
(`ok ...: all 6 VU checks`). They use the installed openvcl in `~/ps2dev/bin`,
not this tree. dvp-as's "operand out of range" warnings there are the three
intended wraparound branches.

## CI on the fork

Pushing a branch runs the CI and Tools workflows on glampert/openvcl. Check
them through `GET /repos/glampert/openvcl/actions/runs?head_sha=<sha>`; runs
appear about a minute after the push, so an immediate "no run" is not a failure.
