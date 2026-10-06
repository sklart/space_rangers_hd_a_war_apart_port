# Milestone 10 build provenance

Current M11 pre-hardware diagnostic artifact is built from commit `325d7e6`:

```text
SpaceRangersHDAWarApart.nro
size:   7125421 bytes
SHA-256:819BFA1B7565848217C3F3948A15E85DF107C1760D47E79A6B311B9697CB953B
ELF:    ELF64 AArch64
icon SHA-256: BCFEC7FB1B6DE55205E5A0DFC9D2353C17C4EE2BC14B13FAC59EAE8D174B46A1
```

Validation before hardware handoff:

- clean `make clean && make`: PASS;
- ELF prohibited-symbol audit: PASS;
- NRO ASET JPEG equals `assets/icon.jpg`: PASS;
- package-host CI: PASS, PASS, run `37414363538`.

This is a Switch hardware diagnostic only. Its first real run must use the
deployment instructions in `switch-deployment.md`; no game assets are part of
this repository or artifact.