# M14P GI Format-0

M14P is the portable CPU boundary selected after two confirmed limits: direct upstream TgiGR to TGraphBufGR reaches GR_DX; the CPU TGraphBufGR alternative still links Direct3D ComPtr lifecycle from its object layout.

The portable decoder validates the GI disk header and plane before allocation, limits decoded output to 256 MiB, uses the existing OKGF RGB565-to-BGRA bridge, and byte-copies Format-0 alpha payloads. It never constructs TgiGR or TGraphBufGR.

Current evidence: synthetic RGB565 and direct-copy fixtures pass twice; host binary has no GI/D3D/GraphBuf symbols; ARM64 decoder object has no such symbols; full ARM64 NRO links with zero undefined symbols. The previous real release probe PASS is preserved because decoder, validator and release-test semantics are transferred unchanged; GitHub synthetic CI and Switch runtime remain pending.
