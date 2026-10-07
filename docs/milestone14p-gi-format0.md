# M14P GI Format-0

M14P is the portable CPU boundary selected after two confirmed limits: direct upstream TgiGR to TGraphBufGR reaches GR_DX; the CPU TGraphBufGR alternative still links Direct3D ComPtr lifecycle from its object layout.

The portable decoder validates the GI disk header and plane before allocation, limits decoded output to 256 MiB, uses the existing OKGF RGB565-to-BGRA bridge, and byte-copies Format-0 alpha payloads. It never constructs TgiGR or TGraphBufGR.

Current evidence: synthetic RGB565 and direct-copy fixtures pass twice; host binary has no GI/D3D/GraphBuf symbols; ARM64 decoder object has no such symbols; full ARM64 NRO links with zero undefined symbols. The real release probe remains valid because decoder, validator and release-test semantics were transferred unchanged. GitHub CI and Switch runtime are **PASS**.

Hardware provenance: commit and embedded `build_git` `46330b4`; real GI key `Bm.Captain.2BlazerBi`; decoded 93x104 BGRA, pitch 372, CRC32 `cf5b1d56`, FNV-1a `a668e341bc42a6fb`. The same test completed M13 metadata and M12 with 1,506 frames/presents in 73,472 ms, `PLUS` exit, and `[BOOT] COMPLETE`.

The hardware-tested `Space Rangers HD - A War Apart.nro` was 7,158,064 bytes, SHA-256 `1CDFFC96A8505BA6D9114222F46CF32B4A16E80C93C93735D13198293E61D97E`. `port.log` confirms embedded `build_git`; deployment preflight/manifest confirms SHA-256. M14P is **HARDWARE PASS** and M14 is **COMPLETE**; M15 is **NOT STARTED**.
