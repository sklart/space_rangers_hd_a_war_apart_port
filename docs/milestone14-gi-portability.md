# M14 portability boundary

M14 direct TgiGR DecodeToGraphBuf is blocked by the GR_DX fan-out.
M14R CPU TGraphBufGR is separately blocked: the upstream type layout owns Direct3D9 IDirect3DTexture9, so ComPtr lifecycle remains linked even if allocation branches are CPU-only.

M14P decodes only GI Format 0 into a POD CPU image. It uses pinned upstream GR_gi.cpp 57fa689c630193a66fdea6ca4c79a188814991cd as semantic oracle: RGB565 uses portable OKGF and alpha-bearing Format 0 is copied byte-for-byte. It creates no TgiGR, TGraphBufGR, texture, surface, cache, or UI object.

M14P is **SUCCESS / COMPLETE**: the hardware-tested NRO embedded `46330b4` loaded real key `Bm.Captain.2BlazerBi` and verified its 93x104 BGRA output (pitch 372, CRC32 `cf5b1d56`, FNV-1a `a668e341bc42a6fb`) before completing the persistent loop and orderly `PLUS` shutdown. The direct M14 and M14R architectural blockers remain recorded rather than being hidden by this portable solution.
