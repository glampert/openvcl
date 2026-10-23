# Upstream workflow

## Remotes

| Remote | Repository | Role |
|---|---|---|
| `origin` | glampert/openvcl | our fork; PRs are opened from here |
| `upstream` | ps2dev/openvcl | the maintained project, branch `master` |
| `jsvennevid` | jsvennevid/openvcl | the original 0.3.3 code |

ps2dev/openvcl is what ps2toolchain-dvp installs (v0.4.0, with a ~20k-line
scheduler: pairing, latency padding, store movement, delay-slot filling,
software pipelining). Its unmerged `optimizations` branch (fjtrujy) carries
further scheduler work.

## How a fix travels

1. Branch `fix/<name>` off `upstream/master` (or off the fix it depends on,
   saying so in the PR). Keep upstream's CMake build, test harness and C++11
   style; change only what the fix needs.
2. Add integration tests (`test/integration/test_<topic>.cpp`, registered in
   `test/CMakeLists.txt`) that fail on master.
3. Run the corpus against an upstream/master build, and the sanitizer run
   ([testing.md](testing.md)). Outputs stay byte-identical unless the change is
   intended and listed in the PR.
4. Push to origin; the CI and Tools workflows run on the fork.
5. The PR to ps2dev/openvcl:master is opened by hand (no `gh` here).
6. Merge the branch into our `main` (`--no-ff`, "Merge branch 'fix/...'") so
   main always carries every fix. Finish commit-message amends before that.
7. Once upstream merges a PR, delete its branch locally and on origin.

Sync main with `git merge upstream/master` (ps2dev merges PRs with merge
commits). Force-pushes only with an explicit OK. The corpus and these docs
never go into a PR.

## Branches (2026-10-23, after PR #7)

| Branch | Base | State |
|---|---|---|
| main | upstream/master + every fix | upstream 3ea8bf5 merged, plus docs |
| fix/loop-liveness (7a) | upstream/master | standalone; any time |
| fix/pipeline-live-registers | upstream/master | submit first: smallest standalone fix, and 7b should follow it; PR text written |
| fix/partial-write-ranges (7b) | fix/loop-liveness | after 7a and the pipeliner fix are merged; PR text not written yet |
| fix/exit-outputs | fix/partial-write-ranges | to submit after 7b |
| fix/cross-block-latency | upstream/master | independent; use the corrected PR text (register cases are timing-neutral) |
| fix/memory-alias | upstream/master | experimental; maintainers have their own version on `optimizations` |
| ci/sce-vcl-check | orphan | tool branch, worktree `../openvcl-ci` |
| test/local-corpus | orphan | the corpus, worktree `../openvcl-corpus`; never merged |

Merged upstream and deleted: fix/clipw-implicit-w (#2), fix/immediate-ranges
(#3), fix/diagnostics (#4), fix/emit-source (#5), ci/sanitizers (#6), fix/trailing-branch-range (#7).

Worktrees next to this repo: `../openvcl-baseline` (upstream/master build, the
corpus baseline; move and rebuild it when upstream moves), `../openvcl-ci`,
`../openvcl-corpus`.

## Next

- Submission order: pipeline-live-registers; then cross-block-latency and 7a in any order; then 7b; then exit-outputs.

- Move quake-ps2 and quake2-ps2 to a build of our main (they use the installed
  openvcl 0.4.0 today). Their vu-checker submodules are at 3917e1b, committed
  locally (quake2-ps2 f5fb890, quake-ps2 faa914b), not pushed.
- Ask the maintainers whether they want the memory-alias port on master before
  their `optimizations` branch lands.
