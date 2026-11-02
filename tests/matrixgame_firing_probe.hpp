#pragma once
#include <cstdint>
#include <map>
struct SObjectCore;class CMatrixEffectWeapon;class CMatrixMapStatic;
class FiringProbe {
public:
 FiringProbe();~FiringProbe();void Issue();void Stop();void Verify();
 FiringProbe(const FiringProbe&)=delete;FiringProbe& operator=(const FiringProbe&)=delete;
private:
 friend void ObserveOriginalFire(CMatrixEffectWeapon&,SObjectCore*);
 friend void ObserveOriginalHit(CMatrixEffectWeapon&,SObjectCore*,CMatrixMapStatic*,std::uint32_t);
 struct Count{int first{},last{},events{},type{};};
 SObjectCore* core_{};int start_{},stop_{},last_fire_{};std::uint8_t fire_key_{};unsigned shots_{},hits_{},object_hits_{},land_hits_{};
 std::map<CMatrixEffectWeapon*,Count> weapons_;
};
void ObserveOriginalFire(CMatrixEffectWeapon&,SObjectCore*);
void ObserveOriginalHit(CMatrixEffectWeapon&,SObjectCore*,CMatrixMapStatic*,std::uint32_t);
