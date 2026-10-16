# SCE vcl behaviour check

A throwaway CI branch, kept apart from the openvcl sources so that only this
workflow runs. `.github/workflows/sce-vcl-check.yml` runs SCE's `vcl 1.4beta7`
(the binary the Tyra engine ships in `assets/vcl` and builds its VU1 programs
with) on the CLIPw operand forms in `cases/`, and on Tyra's own VU1 programs
preprocessed by vclpp. It records what SCE VCL accepts and emits so openvcl's
behaviour can be compared against it.

Tyra, the vcl binary (checked by SHA-256) and vclpp are all pinned in the
workflow's `env`.
