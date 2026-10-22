// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Mathematical D3DX data/API compatibility backed by real DirectXMath CPU operations.
// No device, resource or renderer API is provided here.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <DirectXMath.h>
#ifndef _MSC_VER
// Legacy SAL spellings collide with identifiers in GNU library headers included later.
#undef __in
#undef __out
#undef __valid
#undef __null
#endif
struct D3DXVECTOR2 : DirectX::XMFLOAT2 {
    using XMFLOAT2::XMFLOAT2;
    D3DXVECTOR2() = default;
    operator float*() noexcept { return &x; }
    operator const float*() const noexcept { return &x; }
    D3DXVECTOR2 operator+() const { return *this; }
    D3DXVECTOR2 operator-() const { return {-x,-y}; }
    D3DXVECTOR2 operator+(const D3DXVECTOR2& r) const { return {x+r.x,y+r.y}; }
    D3DXVECTOR2 operator-(const D3DXVECTOR2& r) const { return {x-r.x,y-r.y}; }
    D3DXVECTOR2 operator*(float s) const { return {x*s,y*s}; }
    D3DXVECTOR2 operator/(float s) const { return {x/s,y/s}; }
    D3DXVECTOR2& operator+=(const D3DXVECTOR2& r) { x+=r.x; y+=r.y; return *this; }
    D3DXVECTOR2& operator-=(const D3DXVECTOR2& r) { x-=r.x; y-=r.y; return *this; }
    D3DXVECTOR2& operator*=(float s) { x*=s; y*=s; return *this; }
    D3DXVECTOR2& operator/=(float s) { x/=s; y/=s; return *this; }
    bool operator==(const D3DXVECTOR2& r) const { return x==r.x && y==r.y; }
};
inline D3DXVECTOR2 operator*(float s,const D3DXVECTOR2& v) { return v*s; }
struct D3DXVECTOR3 : DirectX::XMFLOAT3 {
    using XMFLOAT3::XMFLOAT3;
    D3DXVECTOR3() = default;
    operator float*() noexcept { return &x; }
    operator const float*() const noexcept { return &x; }
    D3DXVECTOR3 operator+() const { return *this; }
    D3DXVECTOR3 operator-() const { return {-x,-y,-z}; }
    D3DXVECTOR3 operator+(const D3DXVECTOR3& r) const { return {x+r.x,y+r.y,z+r.z}; }
    D3DXVECTOR3 operator-(const D3DXVECTOR3& r) const { return {x-r.x,y-r.y,z-r.z}; }
    D3DXVECTOR3 operator*(float s) const { return {x*s,y*s,z*s}; }
    D3DXVECTOR3 operator/(float s) const { return {x/s,y/s,z/s}; }
    D3DXVECTOR3& operator+=(const D3DXVECTOR3& r) { x+=r.x; y+=r.y; z+=r.z; return *this; }
    D3DXVECTOR3& operator-=(const D3DXVECTOR3& r) { x-=r.x; y-=r.y; z-=r.z; return *this; }
    D3DXVECTOR3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
    D3DXVECTOR3& operator/=(float s) { x/=s; y/=s; z/=s; return *this; }
    bool operator==(const D3DXVECTOR3& r) const { return x==r.x && y==r.y && z==r.z; }
};
inline D3DXVECTOR3 operator*(float s,const D3DXVECTOR3& v) { return v*s; }
using D3DMATRIX = DirectX::XMFLOAT4X4;
struct D3DXMATRIX : D3DMATRIX {
    using D3DMATRIX::D3DMATRIX;
    D3DXMATRIX() = default;
    operator float*() noexcept { return &_11; }
    operator const float*() const noexcept { return &_11; }
};
struct D3DXPLANE { float a,b,c,d; };
static_assert(sizeof(D3DXVECTOR2)==8 && sizeof(D3DXVECTOR3)==12 && sizeof(D3DXMATRIX)==64 && sizeof(D3DXPLANE)==16);
static_assert(alignof(D3DXVECTOR3)==4 && alignof(D3DXMATRIX)==4 && offsetof(D3DXMATRIX,_41)==48);
static_assert(std::is_trivially_copyable_v<D3DXVECTOR3> && std::is_standard_layout_v<D3DXMATRIX>);
// Keep float object access explicit at the SIMD boundary. The packed SSE
// XMLoad/StoreFloat2/3 paths access the first two floats through double*, which
// GCC 13 + TSan can optimize independently of the named float fields.
inline DirectX::XMVECTOR LoadVector3(const D3DXVECTOR3* p) {
    return DirectX::XMVectorSet(p->x,p->y,p->z,0.f);
}
inline D3DXVECTOR3* StoreVector3(D3DXVECTOR3* out,DirectX::FXMVECTOR value) {
    out->x=DirectX::XMVectorGetX(value);out->y=DirectX::XMVectorGetY(value);out->z=DirectX::XMVectorGetZ(value);
    return out;
}
inline DirectX::XMVECTOR LoadVector2(const D3DXVECTOR2* p) {
    return DirectX::XMVectorSet(p->x,p->y,0.f,0.f);
}
inline float D3DXVec3Dot(const D3DXVECTOR3* a,const D3DXVECTOR3* b) {
    return DirectX::XMVectorGetX(DirectX::XMVector3Dot(LoadVector3(a),LoadVector3(b)));
}
inline float D3DXVec3LengthSq(const D3DXVECTOR3* v) { return D3DXVec3Dot(v,v); }
inline float D3DXVec3Length(const D3DXVECTOR3* v) { return DirectX::XMVectorGetX(DirectX::XMVector3Length(LoadVector3(v))); }
inline D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b) {
    const auto value=DirectX::XMVector3Cross(LoadVector3(a),LoadVector3(b));
    return StoreVector3(out,value);
}
inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* out,const D3DXVECTOR3* in) {
    const auto value=DirectX::XMVector3Normalize(LoadVector3(in)); return StoreVector3(out,value);
}
inline float D3DXVec2LengthSq(const D3DXVECTOR2* v) { return DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(LoadVector2(v))); }
inline D3DXVECTOR2* D3DXVec2Normalize(D3DXVECTOR2* out,const D3DXVECTOR2* in) {
    const auto value=DirectX::XMVector2Normalize(LoadVector2(in));
    out->x=DirectX::XMVectorGetX(value);out->y=DirectX::XMVectorGetY(value);return out;
}
inline D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* out,float* determinant,const D3DXMATRIX* in) {
    DirectX::XMVECTOR det;
    const auto value=DirectX::XMMatrixInverse(&det,DirectX::XMLoadFloat4x4(in));
    const auto scalar=DirectX::XMVectorGetX(det); if(determinant)*determinant=scalar;
    if(scalar==0) return nullptr;
    DirectX::XMStoreFloat4x4(out,value); return out;
}
inline D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* out,float angle) {
    DirectX::XMStoreFloat4x4(out,DirectX::XMMatrixRotationZ(angle));return out;
}
inline D3DXPLANE* StorePlane(D3DXPLANE* out,DirectX::FXMVECTOR value) {
    DirectX::XMFLOAT4 plane; DirectX::XMStoreFloat4(&plane,value);
    *out={plane.x,plane.y,plane.z,plane.w}; return out;
}
inline D3DXPLANE* D3DXPlaneFromPointNormal(D3DXPLANE* out,const D3DXVECTOR3* point,const D3DXVECTOR3* normal) {
    return StorePlane(out,DirectX::XMPlaneFromPointNormal(LoadVector3(point),LoadVector3(normal)));
}
inline D3DXPLANE* D3DXPlaneFromPoints(D3DXPLANE* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b,const D3DXVECTOR3* c) {
    return StorePlane(out,DirectX::XMPlaneFromPoints(LoadVector3(a),LoadVector3(b),LoadVector3(c)));
}
