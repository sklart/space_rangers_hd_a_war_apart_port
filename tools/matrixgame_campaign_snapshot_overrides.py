"""Release constructor-owned resources replaced by the original memory loader."""
from matrixgame_campaign_cleanup_overrides import replace_body

def apply(name,text):
 if name=='aShip.cpp':
  return replace_body(text,'    void TShip::LoadFromBuffer(EC_Buf::TBufEC* Buffer, aGalaxy::TGalaxy* Galaxy) {','    void TShip::SaveToBlock(',
   '        RangerRelations = pas::make_object<pas::List>();','        pas::free(RangerRelations);\n        RangerRelations = pas::make_object<pas::List>();').replace(
   '// Native replaces the constructor-created list without freeing it here.','// Release the constructor-owned list before reading the saved relations.')
 if name=='aGalaxy.cpp':
  return replace_body(text,'    void TConstellation::LoadFromBuffer(EC_Buf::TBufEC* Buffer, TGalaxy* Galaxy) {','    void TConstellation::ResolveLoadedReferences(',
   '                OutlinePolygons = Polygon;','                pas::free(OutlinePolygons);\n                OutlinePolygons = Polygon;')
 return text
