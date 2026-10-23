# Project notes

Our own notes on this fork of openvcl: what we found, how we test, and where
each fix stands. They live on `main` only and are not meant for upstream pull
requests; fix branches are cut from `upstream/master` and never carry them.

| Document | What it covers |
|---|---|
| [fixes.md](fixes.md) | The miscompiles and other bugs found through quake2-ps2 and vu-checker, and the branch that fixes each |
| [upstream-workflow.md](upstream-workflow.md) | Remotes, how a fix travels to ps2dev/openvcl, and the state of every branch |
| [testing.md](testing.md) | The local corpus and its harness, sanitizer runs, vu-checker, the SCE vcl CI check |
| [vu-hazards.md](vu-hazards.md) | Which VU results the hardware interlocks and which it reads silently stale |
| [sce-vcl-comparison.md](sce-vcl-comparison.md) | openvcl's code against SCE vcl 1.4beta7 on the corpus: how much slower, and why |
| [sce-vcl/](sce-vcl/README.md) | The tools behind that comparison: SCE vcl under emulation, and a VU1 cycle model |
