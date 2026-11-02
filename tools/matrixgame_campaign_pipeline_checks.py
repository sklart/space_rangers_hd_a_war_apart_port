"""Verify production-generated affected methods match the compiled SDK cleanup/seed sources."""
import json
from pathlib import Path
from e2e_source_overrides import generated_source
from e2e4_script_width_overrides import prepare_script_source_overlay
from matrixgame_standard_nro_audit import ROOT,sha

def main():
 base=ROOT/'build/matrixgame/e2e601-revolution-seed/production-pipeline';base.mkdir(parents=True,exist_ok=True);i=1
 while (base/str(i)).exists():i+=1
 out=base/str(i);out.mkdir();overlay=out/'source-overlay';prepare_script_source_overlay(ROOT/'upstream/cpp/src',overlay);result={'status':'INCOMPLETE','scope':'GENERATOR SOURCE EQUIVALENCE ONLY; NOT RUNTIME','sources_sha256':{},'methods':[],'full_source_matches':[]}
 cases={
  'aPlanet.cpp':('build/matrixgame/e2e601-revolution-seed/sdk-dependency/aPlanet.cpp',[
   ('    void TPlanet::TriggerGovernmentRevolution() {','    void TPlanet::TryTriggerEconomicEvent() {'),
   ('    void TPlanet::TryTriggerEconomicEvent() {','\n    }\n'),
   ('    void TPlanet::NormalizeSurfaceLootEntries() {','    std::int32_t TPlanet::GetTotalSurfaceTileCount()')]),
  'aShip.cpp':('build/matrixgame/e2e600-campaign-navigation/sdk-dependency/aShip.cpp',[
   ('    void TShip::LoadFromBuffer(EC_Buf::TBufEC* Buffer, aGalaxy::TGalaxy* Galaxy) {','    void TShip::SaveToBlock(')]),
  'aGalaxy.cpp':('build/matrixgame/e2e600-campaign-navigation/sdk-dependency/aGalaxy.cpp',[
   ('    void TConstellation::LoadFromBuffer(EC_Buf::TBufEC* Buffer, TGalaxy* Galaxy) {','    void TConstellation::ResolveLoadedReferences('),
   ('    void TConstellation::RebuildOutlineSegments() {','    void TConstellation::ClearOutlineSegmentsAndBounds()')]),
  'aRanger.cpp':('build/matrixgame/e2e599-campaign-cleanup/sdk-dependency/aRanger.cpp',[
   ('    void TRanger_Destroy(TRanger* Self) {','    void TRanger::InitializeAtPlanet(')]),
  'aPlayer.cpp':('build/matrixgame/e2e599-campaign-cleanup/sdk-dependency/aPlayer.cpp',[
   ('    void TPlayer_Destroy(TPlayer* Self) {','    void TPlayer::SaveToBuffer(')]),
  'aGalaxyEvent.cpp':('build/matrixgame/e2e599-campaign-cleanup/sdk-dependency/aGalaxyEvent.cpp',[
   ('    void TGalaxyEvent::ClearTextData() {','    void TGalaxyEvent::LoadFromBuffer(')]),
  'EC_Ether.cpp':('build/matrixgame/e2e599-campaign-cleanup/sdk-dependency/EC_Ether.cpp',[
   ('    void TEther_Destroy(TEther* Self) {','    void TEther::Clear()')]),
  'EC_Expression.cpp':('build/matrixgame/e2e599-campaign-cleanup/sdk-dependency/EC_Expression.cpp',[
   ('    void TVarArrayEC_Create(TVarArrayEC* Self) {','    void TVarArrayEC_Destroy('),
   ('    void TVarArrayEC_Destroy(TVarArrayEC* Self) {','    void TVarArrayEC::ClearStorage()')])}
 cases['aPlanet.cpp'][1].append(('    void TPlanet_Destroy(TPlanet* Self) {','    void TPlanet::InitGenerated('))
 try:
  for name,(compiled_name,methods) in cases.items():
   original=ROOT/'upstream/cpp/src'/name;input_source=overlay/name;assert input_source.is_file();dest=out/name;actual=generated_source(input_source,dest,audio_on=True,e2e5_profile=True,script_width=True);assert actual==dest,(name,'production routing bypassed')
   compiled=ROOT/compiled_name;left=dest.read_text(encoding='utf-8');right=compiled.read_text(encoding='utf-8')
   assert dest.read_bytes()==compiled.read_bytes(),(name,'full generated source differs from compiled SDK input')
   result['full_source_matches'].append(name)
   for start,end in methods:
    assert left.count(start)==right.count(start)==1,start
    a=left.index(start);b=left.index(end,a+len(start));c=right.index(start);d=right.index(end,c+len(start));assert left[a:b]==right[c:d],start
    result['methods'].append({'source':name,'method':start.strip(),'generated_matches_compiled_SDK':True})
   for p in (original,input_source,dest,compiled):result['sources_sha256'][str(p.relative_to(ROOT))]=sha(p)
  for p in (ROOT/'tools/e2e_source_overrides.py',ROOT/'tools/e2e4_script_width_overrides.py',ROOT/'tools/e2e5_host_game_build.py',ROOT/'tools/matrixgame_campaign_cleanup_overrides.py',ROOT/'tools/matrixgame_campaign_snapshot_overrides.py',ROOT/'tools/matrixgame_campaign_revolution_overrides.py',Path(__file__).resolve()):result['sources_sha256'][str(p.relative_to(ROOT))]=sha(p)
  assert len(result['full_source_matches'])==8 and len(result['methods'])==13
  result['status']='PASS; EIGHT PRODUCTION GENERATED SOURCES AND THIRTEEN CAMPAIGN CLEANUP/SEED METHODS MATCH COMPILED SDK INPUTS'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(result['status'])

if __name__=='__main__':main()
