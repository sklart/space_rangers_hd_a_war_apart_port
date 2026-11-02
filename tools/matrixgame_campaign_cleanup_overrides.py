"""Release proven owned campaign resources; preserve gameplay and borrowed values."""
def replace_body(text,start,end,old,new):
 assert text.count(start)==1,start
 begin=text.index(start);finish=text.index(end,begin+len(start));body=text[begin:finish]
 assert body.count(old)==1,(start,old)
 return text[:begin]+body.replace(old,new,1)+text[finish:]
def apply(name,text):
 if name=='aGalaxy.cpp':
  return replace_body(text,'    void TConstellation::RebuildOutlineSegments() {','    void TConstellation::ClearOutlineSegmentsAndBounds()',
   '            pas::free(OutlineSegments);','            ClearOutlineSegmentsAndBounds();\n            pas::free(OutlineSegments);')
 if name=='aPlanet.cpp':
  text=replace_body(text,'    void TPlanet_Destroy(TPlanet* Self) {','    void TPlanet::InitGenerated(',
   '        if (Self->Graphic != nullptr) {','        pas::free(Self->RangerRelations);\n        Self->RangerRelations = nullptr;\n        if (Self->Graphic != nullptr) {')
  start=text.index('    void TPlanet::NormalizeSurfaceLootEntries() {');end=text.index('    std::int32_t TPlanet::GetTotalSurfaceTileCount()',start)
  body=text[start:end];assert body.count('Y <= 7')==2 and body.count('X <= 14')==2
  body=body.replace('Y <= 7','Y < 7').replace('X <= 14','X < 14').replace('These native inclusive bounds overrun the 14-by-7 arrays; retained exactly.','Initialize exactly the valid 14-by-7 surface cells.')
  text=text[:start]+body+text[end:]
  start=text.index('    void TPlanet::TryTriggerEconomicEvent() {');end=text.index('\n    }\n',start)+7
  body=text[start:end];assert body.count('static_cast<std::int32_t>(GenerationSeed)')==30
  guard='(aGalaxy::Galaxy->CurrentTurn + static_cast<std::int32_t>(GenerationSeed)) % 30'
  assert body.count(guard)==1
  body=body.replace(guard,'static_cast<std::int32_t>(static_cast<std::uint32_t>(aGalaxy::Galaxy->CurrentTurn) + GenerationSeed) % 30',1)
  # RNG/text seeds consume the low 32 bits. Unsigned operations retain native
  # two-complement wrapping without signed C++ overflow or altered RNG draws.
  body=body.replace('static_cast<std::int32_t>(GenerationSeed)','GenerationSeed')
  return text[:start]+body+text[end:]
 if name=='aPlayer.cpp':
  return replace_body(text,'    void TPlayer_Destroy(TPlayer* Self) {','    void TPlayer::SaveToBuffer(',
   '        aRanger::TRanger_Destroy(Self);','        pas::free(Self->AchievementStats);\n        Self->AchievementStats = nullptr;\n        aRanger::TRanger_Destroy(Self);')
 if name=='EC_Ether.cpp':
  return replace_body(text,'    void TEther_Destroy(TEther* Self) {','    void TEther::Clear()',
   '        pas::critical_leave(Self->Lock);','        Self->Clear();\n        pas::free(Self->Lock);\n        Self->Lock = nullptr;')
 if name=='aRanger.cpp':
  return replace_body(text,'    void TRanger_Destroy(TRanger* Self) {','    void TRanger::InitializeAtPlanet(',
   '        if (aPlayer::GetPlayer() == Self) {','        pas::free(Self->Quests);\n        Self->Quests = nullptr;\n        if (aPlayer::GetPlayer() == Self) {')
 if name=='aGalaxyEvent.cpp':
  return replace_body(text,'    void TGalaxyEvent::ClearTextData() {','    void TGalaxyEvent::LoadFromBuffer(',
   '                pas::dispose(pas::list_get(TextData, i));','                pas::dispose(pas::list_at<pas::WideString>(TextData, i));')
 if name=='EC_Expression.cpp':
  text=replace_body(text,'    void TVarArrayEC_Create(TVarArrayEC* Self) {','    void TVarArrayEC_Destroy(',
   '        pas::object_create(Self);','        pas::object_create(Self);\n        srhd_awa::platform::script_arrays::Created(Self);')
  text=replace_body(text,'    void TVarArrayEC_Destroy(TVarArrayEC* Self) {','    void TVarArrayEC::ClearStorage()',
   '        Self->Clear();','        srhd_awa::platform::script_arrays::Destroyed(Self);\n        Self->Clear();')
  return '#include "script_array_lifetime.hpp"\n'+text
 return text
