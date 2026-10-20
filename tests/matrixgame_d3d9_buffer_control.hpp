// Windows reference provider for unchanged BigVB/BigIB bodies. All resource
// operations below call the real Windows SDK D3D9 interfaces; no renderer stub.
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d9.h>
#include "CMain.hpp"
#include "CHeap.hpp"
#include "CException.hpp"
#include <stdexcept>
#include <cstring>
using Base::CMain;
using Base::CHeap;
extern IDirect3DDevice9* g_D3DD;
#define DTRACE()
#define ASSERT_DX(call) do { if((call)!=D3D_OK) throw std::runtime_error("original D3D9 buffer operation failed"); } while(0)
using D3D_VB=IDirect3DVertexBuffer9*;
using D3D_IB=IDirect3DIndexBuffer9*;
#define CREATE_VB(size,layout,buffer) do {if(g_D3DD->CreateVertexBuffer((size),D3DUSAGE_WRITEONLY,(layout),D3DPOOL_DEFAULT,&(buffer),nullptr)!=D3D_OK) (buffer)=nullptr;} while(0)
#define CREATE_IB16(count,buffer) do {if(g_D3DD->CreateIndexBuffer((count)*sizeof(WORD),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&(buffer),nullptr)!=D3D_OK) (buffer)=nullptr;} while(0)
#define IS_VB(buffer) ((buffer)!=nullptr)
#define IS_IB(buffer) ((buffer)!=nullptr)
#define GET_VB(buffer) (buffer)
#define GET_IB(buffer) (buffer)
#define DESTROY_VB(buffer) do {(buffer)->Release();(buffer)=nullptr;} while(0)
#define DESTROY_IB(buffer) do {(buffer)->Release();(buffer)=nullptr;} while(0)
#define LOCKP_VB(buffer,offset,size,out) do {ASSERT_DX((buffer)->Lock((offset),(size),reinterpret_cast<void**>(out),0));} while(0)
#define LOCKP_IB(buffer,offset,size,out) do {ASSERT_DX((buffer)->Lock((offset),(size),reinterpret_cast<void**>(out),0));} while(0)
#define UNLOCK_VB(buffer) do {ASSERT_DX((buffer)->Unlock());} while(0)
#define UNLOCK_IB(buffer) do {ASSERT_DX((buffer)->Unlock());} while(0)
