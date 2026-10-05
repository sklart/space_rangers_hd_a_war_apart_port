# Milestone 10 build provenance

Current pre-hardware diagnostic artifact is built from commit `5e01a6b`:

```text
SpaceRangersHDAWarApart.nro
size:   7117229 bytes
SHA-256:20BBB6E86BCB770F1147C5826FC6E181BBDA7BB03989F9E50F68F0A0DC88ED19
ELF:    ELF64 AArch64
icon SHA-256: BCFEC7FB1B6DE55205E5A0DFC9D2353C17C4EE2BC14B13FAC59EAE8D174B46A1
```

Validation before hardware handoff:

- clean `make clean && make`: PASS;
- ELF prohibited-symbol audit: PASS;
- NRO ASET JPEG equals `assets/icon.jpg`: PASS;
- package-host CI: PASS, run `37381737826`.

This is a Switch hardware diagnostic only. Its first real run must use the
deployment instructions in `switch-deployment.md`; no game assets are part of
this repository or artifact.