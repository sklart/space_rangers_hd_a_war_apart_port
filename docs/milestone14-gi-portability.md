# M14 portability boundary

M14 direct TgiGR DecodeToGraphBuf is blocked by the GR_DX fan-out.
M14R CPU TGraphBufGR is separately blocked: the upstream type layout owns Direct3D9 IDirect3DTexture9, so ComPtr lifecycle remains linked even if allocation branches are CPU-only.

M14P decodes only GI Format 0 into a POD CPU image. It uses pinned upstream GR_gi.cpp 57fa689c630193a66fdea6ca4c79a188814991cd as semantic oracle: RGB565 uses portable OKGF and alpha-bearing Format 0 is copied byte-for-byte. It creates no TgiGR, TGraphBufGR, texture, surface, cache, or UI object.
