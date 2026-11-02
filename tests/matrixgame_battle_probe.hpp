#pragma once
#include <vector>
struct SObjectCore;class CMatrixMapStatic;
struct BattleHitSnapshot {
 BattleHitSnapshot()=default;BattleHitSnapshot(CMatrixMapStatic*,int);
 ~BattleHitSnapshot();
 BattleHitSnapshot(const BattleHitSnapshot&)=delete;BattleHitSnapshot& operator=(const BattleHitSnapshot&)=delete;
 SObjectCore* core{};float before{};int weapon{},side{},kind{};
};
BattleHitSnapshot BeginBattleHit(CMatrixMapStatic*,SObjectCore*,int);
void EndBattleHit(const BattleHitSnapshot&,bool);
class BattleProbe {
public:
 BattleProbe();~BattleProbe();void Issue();bool Continue()const;void Verify();
 BattleProbe(const BattleProbe&)=delete;BattleProbe& operator=(const BattleProbe&)=delete;
private:
 friend BattleHitSnapshot BeginBattleHit(CMatrixMapStatic*,SObjectCore*,int);
 friend void EndBattleHit(const BattleHitSnapshot&,bool);
 struct Hit{int time,weapon,side,kind;float before,after;bool exists,dead;};
 SObjectCore* attacker_{};SObjectCore* target_{};int start_{};unsigned reductions_{};
 std::vector<Hit> hits_;
};
