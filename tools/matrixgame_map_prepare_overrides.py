"""Complete original map loading/restart/resource lifecycle, not a map stand-in."""
import hashlib,re
from matrixgame_vector_object_overrides import function

def prepare_map_prepare(output):
    text=(output/'MatrixGame/src/MatrixMapPrepare.cpp').read_text(encoding='utf-8')
    text=text.replace('CopyMemory(','std::memcpy(').replace('MoveMemory(','std::memmove(')
    text=re.sub(r'\bINT32\b','std::int32_t',text)
    replacements={
        'g_D3DD == NULL':'!prepare_device::DeviceAvailable()',
        'CDeviceState(g_D3DD)':'CDeviceState(prepare_device::StateBackend())',
        'HRESULT hr = g_D3DD->TestCooperativeLevel();':'auto hr = prepare_device::CooperativeLevel();',
        'g_D3DD->Reset(&g_D3Dpp)':'prepare_device::ResetCurrentPresentation()',
        'D3D_OK':'0','D3DERR_DEVICELOST':'static_cast<std::int32_t>(0x88760868u)',
        'D3DERR_DEVICENOTRESET':'static_cast<std::int32_t>(0x88760869u)',
        'D3DMATERIAL9':'srhd_awa::matrixgame::platform3g::VideoMaterial',
        'g_D3DD->SetMaterial(&mtrl)':'srhd_awa::matrixgame::platform3g::ReadyDevice().SetVideoMaterial(mtrl)',
        'g_D3DD->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL)':'srhd_awa::matrixgame::platform3g::ReadyDevice().SetRenderState(static_cast<srhd_awa::matrixgame::RasterRegister>(146),0u)',
        'D3DXMatrixRotationX(':'prepare_geometry::RotationX(',
        'D3DXVec3TransformNormal(':'prepare_geometry::TransformNormal(',
        '_alloca(':'MATRIXGAME_LOGIC_STACK_ALLOC(',
    }
    for old,new in replacements.items():
        if old not in text:raise ValueError('Original map preparation boundary changed: '+old)
        text=text.replace(old,new)
    source_ready='    memset(srcb, 0, sizeof(CBitmap *) * g_MatrixMap->IdsGetCount());'
    masks_ready='    memset(inmapb, 0, sizeof(CBitmap *) * bmpc->GetArraysCount());'
    if text.count(source_ready)!=1 or text.count(masks_ready)!=1:raise ValueError('Original union bitmap slots changed')
    text=text.replace(source_ready,source_ready+'\n    srhd_awa::matrixgame::BitmapArrayOwner source_owner({srcb, std::size_t(g_MatrixMap->IdsGetCount())},g_CacheHeap);')
    text=text.replace(masks_ready,masks_ready+'\n    srhd_awa::matrixgame::BitmapArrayOwner mask_owner({inmapb, std::size_t(bmpc->GetArraysCount())},g_CacheHeap);')
    cleanup='''    for (int i = 0; i < g_MatrixMap->IdsGetCount(); ++i) {
        if (srcb[i]) {
            HDelete(CBitmap, srcb[i], g_CacheHeap);
        }
    }
    for (std::uint32_t i = 0; i < bmpc->GetArraysCount(); ++i) {
        if (inmapb[i]) {
            HDelete(CBitmap, inmapb[i], g_CacheHeap);
        }
    }'''
    if text.count(cleanup)!=1:raise ValueError('Original union bitmap cleanup changed')
    text=text.replace(cleanup,'    // BitmapArrayOwner releases both original slot arrays on success and failure.')
    original=function(text,'static int BuildTexUnions(')
    helper=original.replace('static int BuildTexUnions(CStorage &stor, int lp1, int lp2)',
        'int BuildOriginalTextureUnions(CStorage &stor,const TextureUnionInput& input,int lp1,int lp2)')
    for old,new in [('g_MatrixMap->m_TexUnionDim','input.dimension'),('g_MatrixMap->m_TexUnionSize','input.cells'),
        ('g_MatrixMap->IdsGetCount()','input.id_count'),('g_MatrixMap->IdsGet(','input.Id('),
        ('g_LoadProgress->SetCurLPPos(','input.SetPosition(')]:helper=helper.replace(old,new)
    helper=helper.replace('{','''{
    if(input.dimension<1 || input.dimension>TEXUNION_DIM_MAX || input.cells!=input.dimension*input.dimension ||
       input.id_count<1 || input.id_count>65536 || !input.read_id)throw std::invalid_argument("original union resource view");''',1)
    source_scratch='''    CBitmap **srcb = (CBitmap **)MATRIXGAME_LOGIC_STACK_ALLOC(sizeof(CBitmap *) * input.id_count);
    memset(srcb, 0, sizeof(CBitmap *) * input.id_count);'''
    mask_scratch='''    CBitmap **inmapb = (CBitmap **)MATRIXGAME_LOGIC_STACK_ALLOC(sizeof(CBitmap *) * bmpc->GetArraysCount());
    memset(inmapb, 0, sizeof(CBitmap *) * bmpc->GetArraysCount());'''
    if source_scratch not in helper or mask_scratch not in helper:raise ValueError('Original union scratch changed')
    helper=helper.replace(source_scratch,'    std::vector<CBitmap*> source_slots(input.id_count);\n    CBitmap** srcb=source_slots.data();')
    helper=helper.replace(mask_scratch,'    std::vector<CBitmap*> mask_slots(bmpc->GetArraysCount());\n    CBitmap** inmapb=mask_slots.data();')
    validation='''    // Validate referenced records before any original pointer indexing.
    for(int i=0;i<cnt;++i){
        if(tuc->GetArrayLength(i)!=std::size_t(input.cells)*sizeof(int))throw std::invalid_argument("original union cell bytes");
        const int* cells=tuc->GetFirst<int>(i);
        for(int k=0;k<input.cells;++k)if(cells[k]>=0){
            if(unsigned(cells[k])>=botc->GetArraysCount())throw std::out_of_range("original union bottom index");
            const auto n=botc->GetArrayLength(cells[k]);const int* record=botc->GetFirst<int>(cells[k]);
            if(!n || !(n&1) || record[0]<0 || record[0]>=input.id_count)throw std::invalid_argument("original union bottom record");
            for(unsigned p=1;p<n;p+=2)if(record[p]>=input.id_count || record[p+1]<0 || unsigned(record[p+1])>=bmpc->GetArraysCount())throw std::out_of_range("original union mask/source index");
        }
    }
'''
    helper=helper.replace('    CBottomTextureUnion::Init(cnt);',validation+'    CBottomTextureUnion::Init(cnt);',1)
    helper=helper.replace('    CBottomTextureUnion::Init(cnt);','''    CBottomTextureUnion::Init(cnt);
    struct UnionRollback {
        bool committed{};
        ~UnionRollback(){if(!committed)CBottomTextureUnion::Clear();}
    } output_owner;''',1)
    if helper.count('    return 0;')!=1:raise ValueError('Original union completion changed')
    helper=helper.replace('    return 0;','    output_owner.committed=true;\n    return 0;',1)
    needle='        CBottomTextureUnion::Get(i).MakeFromBitmap(bmp);'
    if helper.count(needle)!=1:raise ValueError('Original union publication changed')
    helper=helper.replace(needle,'        if(input.observe)input.observe(input.observation,i,bmp);\n'+needle)
    decode_calls=[('srcb[*bot]->LoadFromPNG(input.Id(*bot).c_str());','srcb[*bot]','false','*bot'),
        ('srcb[ids]->LoadFromPNG(input.Id(ids).c_str());','srcb[ids]','false','ids'),
        ('inmapb[ibm]->LoadFromPNG(bmpc->GetFirst<std::uint8_t>(ibm), bmpc->GetArrayLength(ibm));','inmapb[ibm]','true','ibm')]
    for call,bitmap,mask,index in decode_calls:
        if helper.count(call)!=1:raise ValueError('Original union decoder call changed: '+call)
        # The original compositor ignored the decoder's bool. An empty/short
        # mask could otherwise silently skip MergeByMask and publish a wrong
        # union as successful. Valid images still use the original pixel loops.
        checked='if(!'+call[:-1]+')throw std::runtime_error("original union PNG decode failed");'
        helper=helper.replace(call,checked+'\n                    input.Decoded('+mask+','+index+',*'+bitmap+');')
    wrapper='''static int BuildTexUnions(CStorage &stor,int lp1,int lp2) {
    srhd_awa::matrixgame::TextureUnionInput input;
    input.dimension=g_MatrixMap->m_TexUnionDim;input.cells=g_MatrixMap->m_TexUnionSize;
    input.id_count=g_MatrixMap->IdsGetCount();input.ids=g_MatrixMap;
    input.read_id=[](const void* owner,int id){return std::wstring(static_cast<CMatrixMap*>(const_cast<void*>(owner))->IdsGet(id).c_str());};
    input.progress=g_LoadProgress;
    input.position=[](void* owner,int value){static_cast<CLoadProgress*>(owner)->SetCurLPPos(value);};
    return srhd_awa::matrixgame::BuildOriginalTextureUnions(stor,input,lp1,lp2);
}'''
    text=text.replace(original,wrapper,1)
    helper_name='MatrixGame/src/OriginalTextureUnions.cpp'
    helper_text='''// Original MatrixMapPrepare.cpp BuildTexUnions, GPL-2.0-or-later.
#include "original_texture_unions.hpp"
#include "MatrixMapTexture.hpp"
#include "CStorage.hpp"
#include "Common.hpp"
#include "bitmap_array_owner.hpp"
#include <cstring>
#include <vector>
namespace srhd_awa::matrixgame {
using namespace Base;
void TextureUnionInput::Decoded(bool mask,int id,const CBitmap& bitmap)const {
    if(!bitmap.Data() || bitmap.SizeX()<TEX_BOTTOM_SIZE || bitmap.SizeY()<TEX_BOTTOM_SIZE ||
       bitmap.BytePP()<1 || bitmap.BytePP()>4 || bitmap.Pitch()<std::int64_t(bitmap.SizeX())*bitmap.BytePP())
        throw std::invalid_argument("original union incomplete bitmap");
    if(decoded)decoded(observation,mask,id,std::size_t(bitmap.Pitch())*bitmap.SizeY());
}
'''+helper+'\n}\n'
    (output/helper_name).write_bytes(helper_text.encode())
    prelude='''#include "Texture.hpp"
#include "bitmap_array_owner.hpp"
#include "original_texture_unions.hpp"
#include "map_device_backend.hpp"
#include "DeviceState.hpp"
#include "map_logic_backend.hpp"
#include "object_geometry.hpp"
#include <cstring>
namespace prepare_device=srhd_awa::matrixgame::map_device;
namespace prepare_geometry=srhd_awa::matrixgame::object_geometry;
'''
    name='MatrixGame/src/OriginalMapPrepare.cpp';path=output/name
    path.write_bytes((prelude+text).encode());return {name:hashlib.sha256(path.read_bytes()).hexdigest(),helper_name:hashlib.sha256((output/helper_name).read_bytes()).hexdigest()}
