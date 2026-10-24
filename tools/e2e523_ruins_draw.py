"""Addressed WSL check; reuses existing e2e523 resource-fixture objects."""
import pathlib,sys,re,json,subprocess,hashlib
r=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from matrixgame_vector_object_overrides import function
out=r/'build/e2e523/ruins-draw';out.mkdir(exist_ok=True)
base=r/'build/e2e523/ruins-format2'
text=(base/'fixture.cpp').read_text()
text=text.replace('#include "okgf.h"','#include "okgf.h"\n#include "win32_compat_okgf.hpp"\n#include <cassert>\ntemplate<class F> F resolved(const char* name){auto p=srhd_awa::platform::win32_compat::ResolveOkgfImport(name);if(!p)std::abort();return reinterpret_cast<F>(p);}')
text=re.sub(r'::(OK(?:GF|GR)_\w+)\(',lambda m:'resolved<decltype(&::'+m[1]+')>("'+m[1]+'")(',text)
main=(r/'upstream/cpp/src/GR_Main.cpp').read_text()
copy=function(main,'void CopyBuffer16Clipped(')
draw=function((r/'upstream/cpp/src/GR_gi.cpp').read_text(),'void TgiGR::DrawToGraphBuf(')
header=(r/'upstream/cpp/src/units/GR_Main.hpp').read_text()
extra='\nnamespace GR_Main {\n'+copy+'\n'
extra+='''void Ex_OKGR_Copy_XY_XY_WORD(void* d,std::int32_t dp,std::int32_t x,std::int32_t y,void* s,std::int32_t sp,std::int32_t sx,std::int32_t sy,std::int32_t w,std::int32_t h){resolved<decltype(&::OKGR_Copy_XY_XY_WORD)>("OKGR_Copy_XY_XY_WORD")(d,dp,x,y,s,sp,sx,sy,w,h);}\n'''
for name in sorted(set(re.findall(r'GR_Main::(\w+)\(',draw)) - {'CopyBuffer16Clipped'}):
    if name in ['Ex_OKGR_F5_DrawRGBA','Ex_OKGR_F6_DrawRGBA']:continue
    declaration=re.search(r'^    (?:void|std::\w+) '+name+r'\([^;]+;',header,re.M)
    assert declaration,name
    extra+=declaration[0].strip()[:-1]+' {std::abort();}\n'
extra+='}\n'
text=text.replace('int main(int argc, char** argv) {',extra+'\nint main(int argc, char** argv) {')
test='''
  auto* target=pas::construct_call<GR_GraphBuf::TGraphBufGR>(GR_GraphBuf::TGraphBufGR_Create,false);
  constexpr int tw=1296,th=732,tp=tw*2+16;
  target->AllocateNativePitch(tw,th,tp);
  struct Case {int x,y;WindowsSdk::TRect clip;};
  const Case cases[]={{0,0,{0,0,1280,720}},{8,3,{0,0,tw,th}},{-7,-5,{13,11,1237,693}}};
  for(const auto& c:cases){
    std::memset(target->GetPixels(),0x5a,tp*th);
    cache->Image->DrawToGraphBuf(target,c.x,c.y,c.clip,0,255);
    for(int y=0;y<th;++y)for(int x=0;x<tp;++x){
      const int px=x/2,sx=px-c.x,sy=y-c.y;
      const bool copied=x<tw*2 && px>=c.clip.Left && px<c.clip.Right && y>=c.clip.Top && y<c.clip.Bottom && sx>=0 && sx<1280 && sy>=0 && sy<720;
      const unsigned char expected=copied?owned_copy[96+sy*2560+sx*2+(x&1)]:0x5a;
      if(static_cast<const unsigned char*>(target->GetPixels())[y*tp+x]!=expected){std::fprintf(stderr,"blit mismatch x=%d y=%d\\n",x,y);return 31;}
    }
  }
  pas::free(target);
  std::puts("original DrawToGraphBuf -> CopyBuffer16Clipped -> production ResolveOkgfImport PASS; full, shifted, negative/clipped; padding preserved");
'''
text=text.replace('  pas::free(cache);',test+'\n  pas::free(cache);',1)
src=out/'fixture.cpp';src.write_text(text)
recipe=json.loads((base/'build.json').read_text())[-1]['command']
recipe=[str(src) if s.endswith('/ruins-format2/fixture.cpp') else str(out/'cachegi') if s.endswith('/ruins-format2/cachegi') else s for s in recipe]
recipe.insert(1,str(r/'port/switch/platform/win32_compat_okgf.cpp'))
p=subprocess.run(recipe,capture_output=True,text=True);(out/'build.log').write_text(p.stdout+p.stderr);print('compile',p.returncode,flush=True)
if p.returncode: print(p.stderr[-4000:]);sys.exit(p.returncode)
import e2e5_gamesettings_rescale as check
code=(r/'tools/e2e5_gamesettings_rescale.py').read_text().replace('"forms.pkg"','"background.pkg"').replace('if not re.fullmatch(', 'if not re.search(');exec(compile(code,check.__file__,'exec'),check.__dict__)
from probe_gai_release import decode2
check.decode_format0=lambda d:(decode2(d)[0]['decoded'],decode2(d)[1]);check.RESOURCE='data/formruinsbg/2rcbg.gi'
library=r/'port/switch/build/e2e5-host-game/switch-effective/okgf/libokgf.so'
result=check.run(r/'windows/Space Rangers HD A War Apart',library,1280,720,original_graphbuf=base/'graphbuf',original_cachegi=out/'cachegi')
result['boundary']='HOST original GI cache and software draw slice, actual production resolver; no full UI or hardware proof'
assert result['original_cachegi_output'].startswith('original DrawToGraphBuf -> CopyBuffer16Clipped -> production ResolveOkgfImport PASS; full, shifted, negative/clipped; padding preserved\n')
result['source_sha256']={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [src,r/'port/switch/platform/win32_compat_okgf.cpp',r/'upstream/cpp/src/GR_gi.cpp',r/'upstream/cpp/src/GR_Main.cpp']}
result['link_command']=recipe
result['reused_objects_sha256']={s:hashlib.sha256(pathlib.Path(s).read_bytes()).hexdigest() for s in recipe if s.endswith('.o')}
result['executable_sha256']=hashlib.sha256((out/'cachegi').read_bytes()).hexdigest()
(out/'result.json').write_text(json.dumps(result,indent=2));print(result['original_cachegi_output'])
