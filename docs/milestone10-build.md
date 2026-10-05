# Milestone 10 build provenance

Current pre-hardware diagnostic artifact is built from commit `ec4493f`:

```text
SpaceRangersHDAWarApart.nro
size:   7117229 bytes
SHA-256:90C21E57486BA763B82CDB24EEED96D21141A5F8968068FB298C303A67654364
ELF:    ELF64 AArch64
icon SHA-256: BCFEC7FB1B6DE55205E5A0DFC9D2353C17C4EE2BC14B13FAC59EAE8D174B46A1
```

Validation before hardware handoff:

- clean `make clean && make`: PASS;
- ELF prohibited-symbol audit: PASS;
- NRO ASET JPEG equals `assets/icon.jpg`: PASS;
- package-host CI: PASS, pending the pushed commit.

This is a Switch hardware diagnostic only. Its first real run must use the
deployment instructions in `switch-deployment.md`; no game assets are part of
this repository or artifact.