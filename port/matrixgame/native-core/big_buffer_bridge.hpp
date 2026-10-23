// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "buffer_resources.hpp"
#include "CHeap.hpp"
#include "CMain.hpp"
#include "CException.hpp"
#include "legacy_vertex_layout.hpp"
#include <cstring>
#if defined(_DEBUG) || defined(_TRACE) || defined(MEM_SPY_ENABLE)
#error Native BigVB/BigIB currently preserve the original release path only.
#endif
using Base::CHeap;
using Base::CMain;
// Private names avoid impersonating D3D9 headers/interfaces or colliding with
// the unported D3DControl.hpp. These create real owned CPU staging resources.
#define NATIVE_BUF_CREATE_VB(bytes,layout,buffer) do { (buffer)=::srhd_awa::matrixgame::buffers::Current().CreateVertex((bytes),(layout)); } while(0)
#define NATIVE_BUF_CREATE_IB16(count,buffer) do { (buffer)=::srhd_awa::matrixgame::buffers::Current().CreateIndex(::srhd_awa::matrixgame::buffers::IndexBytes((count),::srhd_awa::matrixgame::buffers::IndexWidth::U16),::srhd_awa::matrixgame::buffers::IndexWidth::U16); } while(0)
#define NATIVE_BUF_IS(buffer) ((buffer)!=nullptr)
#define NATIVE_BUF_DESTROY(buffer) do { if(buffer) (buffer)->Release();(buffer)=nullptr; } while(0)
#define NATIVE_BUF_GET(buffer) (buffer)
#define NATIVE_BUF_LOCK_PART(buffer,offset,bytes,out) do { ::srhd_awa::matrixgame::buffers::MapPart((buffer),(offset),(bytes),(out)); } while(0)
#define NATIVE_BUF_UNLOCK(buffer) do { (buffer)->Unmap(); } while(0)
// Additional original effect/map clients use the same owned staging context.
// These do not bind a D3D device or claim that CPU allocation uploaded to GPU.
#define NATIVE_BUF_CREATE_DYNAMIC_VB(bytes,layout,buffer) do { (buffer)=::srhd_awa::matrixgame::buffers::Current().CreateVertex((bytes),(layout),::srhd_awa::matrixgame::buffers::Pool::Default,true); } while(0)
#define NATIVE_BUF_CREATE_MANAGED_VB(bytes,layout,buffer) do { (buffer)=::srhd_awa::matrixgame::buffers::Current().CreateVertex((bytes),(layout),::srhd_awa::matrixgame::buffers::Pool::Managed); } while(0)
#define NATIVE_BUF_CREATE_MANAGED_IB(bytes,buffer) do { (buffer)=::srhd_awa::matrixgame::buffers::Current().CreateIndex((bytes),::srhd_awa::matrixgame::buffers::IndexWidth::U16,::srhd_awa::matrixgame::buffers::Pool::Managed); } while(0)
#define NATIVE_BUF_CREATE_DYNAMIC_IB(bytes,buffer) do { (buffer)=::srhd_awa::matrixgame::buffers::Current().CreateIndex((bytes),::srhd_awa::matrixgame::buffers::IndexWidth::U16,::srhd_awa::matrixgame::buffers::Pool::Default,true); } while(0)
#define NATIVE_BUF_CREATE_IB32(count,buffer) do { (buffer)=::srhd_awa::matrixgame::buffers::Current().CreateIndex(::srhd_awa::matrixgame::buffers::IndexBytes((count),::srhd_awa::matrixgame::buffers::IndexWidth::U32),::srhd_awa::matrixgame::buffers::IndexWidth::U32); } while(0)
#define NATIVE_BUF_LOCK(buffer,out) NATIVE_BUF_LOCK_PART(buffer,0,0,out)
#define NATIVE_BUF_MAP_MODE(buffer,offset,bytes,out,mode) do { ::srhd_awa::matrixgame::buffers::MapPart((buffer),(offset),(bytes),(out),(mode)); } while(0)
#define NATIVE_BUF_LOCK_DISCARD(buffer,out) NATIVE_BUF_MAP_MODE(buffer,0,0,out,::srhd_awa::matrixgame::buffers::MapMode::Discard)
#define NATIVE_BUF_LOCK_PART_DISCARD(buffer,offset,bytes,out) NATIVE_BUF_MAP_MODE(buffer,offset,bytes,out,::srhd_awa::matrixgame::buffers::MapMode::Discard)
#define NATIVE_BUF_LOCK_PART_NOOVERWRITE(buffer,offset,bytes,out) NATIVE_BUF_MAP_MODE(buffer,offset,bytes,out,::srhd_awa::matrixgame::buffers::MapMode::NoOverwrite)
