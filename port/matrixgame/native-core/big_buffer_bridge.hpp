// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "buffer_resources.hpp"
#include "CHeap.hpp"
#include "CMain.hpp"
#include "CException.hpp"
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
