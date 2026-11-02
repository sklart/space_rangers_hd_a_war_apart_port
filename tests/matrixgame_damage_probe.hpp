#pragma once
#include <vector>
struct SObjectCore;class CMatrixMapStatic;
struct DamageState {
 bool exists{},health_known{},burn_known{},ablaze{};int object_type{},behaviour{-1},ttl{};float health{};
};
class DamageObservation {
public:
 DamageObservation()=default;DamageObservation(CMatrixMapStatic*,int);
 ~DamageObservation();
 DamageObservation(const DamageObservation&)=delete;DamageObservation& operator=(const DamageObservation&)=delete;
 SObjectCore* core{};DamageState before,burn_after;int weapon{};DamageObservation* previous{};
};
DamageObservation BeginOriginalDamage(CMatrixMapStatic*,SObjectCore*,int);
void ObserveOriginalDamage(const DamageObservation&,bool dead);
void ObserveOriginalBurnState(CMatrixMapStatic*,bool after,bool ablaze,int ttl);
class DamageProbe {
public:
 DamageProbe();~DamageProbe();void Verify();
 DamageProbe(const DamageProbe&)=delete;DamageProbe& operator=(const DamageProbe&)=delete;
private:
 friend DamageObservation BeginOriginalDamage(CMatrixMapStatic*,SObjectCore*,int);
 friend void ObserveOriginalDamage(const DamageObservation&,bool);
 struct Event{DamageState before,after;int weapon{},time{};bool dead{};};
 std::vector<Event> events_;
};
