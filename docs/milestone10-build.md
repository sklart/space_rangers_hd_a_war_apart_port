# Milestone 10 build provenance

Hardware diagnostic artifact built from commit `bfa8e34`:

```text
SpaceRangersHDAWarApart.nro
size:   7086275 bytes
SHA-256:A8B8E9BF1FC0C501D3AA552343241877ECBC2861F9FE268C78752350E8879589
```

Validation before hardware handoff:

- clean `make clean && make`: PASS;
- ELF: `ELF64 AArch64`;
- prohibited-symbol audit: PASS;
- asset-free real Linux CI: PASS, run `37372903464`.

The artifact is a Switch hardware diagnostic only. Its first real run must use
the deployment instructions in `switch-deployment.md`; no game assets are part
of this repository or artifact.
