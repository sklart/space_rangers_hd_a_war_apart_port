"""Extract unchanged prepared original cache methods; only add a test friend to the real header."""
import hashlib,json
from pathlib import Path
from matrixgame_vector_object_overrides import function
ROOT=Path(__file__).resolve().parents[1]
def prepare(source,out):
 out.mkdir(parents=True,exist_ok=True);header=(source/'MatrixGame/src/MatrixMinimap.hpp').read_text();marker='class CMinimap : public CMain {';assert header.count(marker)==1;header=header.replace(marker,marker+'\n    friend struct NativeMinimapTestAccess;')
 (out/'MinimapFixture.hpp').write_text(header)
 text=(source/'MatrixGame/src/MatrixMinimap.cpp').read_text();signatures=['void CMinimap::Clear(void)','void CMinimap::StoreTexture(void)','void CMinimap::RestoreTexture(void)','bool CMinimap::LoadMinimapFromCache(','bool CMinimap::StoreMinimapToCache(']
 prelude='''#include "Texture.hpp"
class CMatrixMapStatic;
#include "MinimapFixture.hpp"
#include "unmanaged_texture_backend.hpp"
#include "output_paths_backend.hpp"
#include "offscreen_scope.hpp"
#include "minimap_lifetime.hpp"
#include <utils.hpp>
#include <stupid_logger.hpp>
#include <memory>
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace mm_texture=srhd_awa::matrixgame::legacy_texture;
namespace mm_unmanaged=srhd_awa::matrixgame::unmanaged_texture;
namespace mm_offscreen=srhd_awa::matrixgame::offscreen;
namespace mm_output=srhd_awa::matrixgame::output_paths;
namespace mm_lifetime=srhd_awa::matrixgame::minimap_lifetime;
namespace {struct MinimapCacheDeleter {void operator()(CTexture* p)const{if(p)CCache::Destroy(p);}};}
#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#define ASSERT_DX(call) FAILED_DX(call)
'''
 methods={s:function(text,s) for s in signatures};(out/'OriginalMinimapCache.cpp').write_text(prelude+'\n\n'.join(methods.values())+'\n')
 cache=(source/'portable-core/CacheNative.cpp').read_text();cache_signatures=['void CacheReplaceFileExt(','void CacheReplaceFileNameAndExt(','void CCacheData::Prepare(','void CCacheData::LoadFromFile(','void CCache::Add(','void CCache::Delete(','void CCache::Up(','CCacheData* CCache::Find(','void CCache::Destroy('];cache_methods={s:function(cache,s) for s in cache_signatures}
 first=cache.index('void CacheReplaceFileExt(');start=cache.index('CCache *g_Cache;');end=cache.index('void CacheInit(',start)
 logger='logger_type lgr{std::clog};';assert (source/'MatrixGame/src/MatrixGame.cpp').read_text().count(logger)==1
 (out/'OriginalCachePrimitives.cpp').write_text(cache[:first]+'#include <stupid_logger.hpp>\n#include <iostream>\n'+logger+'\n'+'\n\n'.join(cache_methods.values())+'\n'+cache[start:end])
 result={'cache_method_sha256':{s:hashlib.sha256(b.encode()).hexdigest() for s,b in cache_methods.items()},'method_sha256':{s:hashlib.sha256(b.encode()).hexdigest() for s,b in methods.items()},'fixture_changes':'Only adds friend NativeMinimapTestAccess to original CMinimap header; methods copied byte-for-byte from prepared production source.'}
 (out/'extraction.json').write_text(json.dumps(result,indent=2)+'\n');return result
