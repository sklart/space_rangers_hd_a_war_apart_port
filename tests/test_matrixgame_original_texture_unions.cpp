// Execute the original union compositor from real storage, without a fake map/Init.
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>
#include "original_texture_unions.hpp"
#include "MatrixMapTexture.hpp"
#include "Common.hpp"
#include "CStorage.hpp"
#include "CFile.hpp"
#include "config_unicode.hpp"
#include "FilePNG.hpp"
#include "native_texture.hpp"
#include "texture_asset.hpp"
namespace mg=srhd_awa::matrixgame;
namespace nt=mg::native_texture;
namespace p=mg::platform3g;
std::vector<std::byte> ReadUnionFile(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary|std::ios::ate);assert(file);std::vector<std::byte> bytes(std::size_t(file.tellg()));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());assert(file);return bytes;}
struct UnionImage final:nt::Image {nt::Residency& arena;nt::Residency::Range range;UnionImage(nt::Residency& a,std::size_t size):arena(a),range(a.Allocate(size,256)){}~UnionImage(){arena.Release(range);}};
struct UnionRouting final:p::Device,nt::Provider {
    std::shared_ptr<nt::Session> session;explicit UnionRouting(std::shared_ptr<nt::Session> s):session(std::move(s)){}
    std::shared_ptr<nt::Session> TextureSession()override{return session;}
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("resource compositor CPU check has no GPU caps");}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("resource compositor CPU check has no actual GPU budget");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("resource compositor CPU check has no gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
struct UnionObservation {
    std::vector<std::wstring> ids;
    std::filesystem::path references;
    std::set<int> requested;
    std::set<std::pair<bool,int>> decoded;
    unsigned id_reads{},observed{},progress_calls{};int position{};
    std::size_t bitmap_cache_bytes{};
    void Reset(){requested.clear();decoded.clear();id_reads=observed=progress_calls=0;position=0;bitmap_cache_bytes=0;}
};
int main(int argc,char** argv){
    assert(argc==4);Base::CFile::StaticInit();const auto package=::matrixgame::native::ConfigWide(argv[1]);Base::CFile::AddPackFile(package.c_str());Base::CFile::OpenPackFiles();
    Base::CStorage storage;assert(storage.Load(L"Matrix\\Map\\desert1_3e.cmap"));
    UnionObservation observed;observed.references=argv[2];auto* strings=storage.GetBuf(DATA_STRINGS,DATA_STRINGS_STRING,Base::ST_WCHAR);assert(strings);
    for(unsigned i=0;i<strings->GetArraysCount();++i)observed.ids.emplace_back(strings->GetAsWStr(i));observed.ids.emplace_back(L"Matrix\\Map\\desert1_3e.cmap");
    mg::TextureUnionInput input;input.dimension=16;
    auto* keys=storage.GetBuf(DATA_PROPERTIES,DATA_PROPERTIES_NAME,Base::ST_WCHAR);auto* values=storage.GetBuf(DATA_PROPERTIES,DATA_PROPERTIES_VALUE,Base::ST_WCHAR);assert(keys && values);
    const auto dimension=keys->FindAsWStr(std::wstring(DATA_TEXUNIONDIM));if(dimension>=0)input.dimension=values->GetAsParamParser(dimension).GetInt();
    input.cells=input.dimension*input.dimension;input.id_count=observed.ids.size();input.ids=&observed;input.progress=&observed;input.observation=&observed;
    input.read_id=[](const void* owner,int id){auto& o=*const_cast<UnionObservation*>(static_cast<const UnionObservation*>(owner));++o.id_reads;assert(o.requested.insert(id).second);return o.ids.at(id);};
    input.position=[](void* owner,int value){auto& o=*static_cast<UnionObservation*>(owner);assert(value>=o.position);o.position=value;++o.progress_calls;};
    input.decoded=[](void* owner,bool mask,int id,std::size_t bytes){auto& o=*static_cast<UnionObservation*>(owner);assert(bytes && o.decoded.emplace(mask,id).second);o.bitmap_cache_bytes+=bytes;};
    input.observe=[](void* owner,int index,const CBitmap& bitmap){auto& o=*static_cast<UnionObservation*>(owner);Base::CBuf serialized;bitmap.SaveInDDSUncompressed(serialized);auto expected=ReadUnionFile(o.references/("union-"+std::to_string(index)+".dds"));assert(expected.size()==std::size_t(serialized.Len()) && std::memcmp(expected.data(),serialized.Get(),expected.size())==0);++o.observed;};
    nt::Residency arena(16*1024*1024);auto session=std::make_shared<nt::Session>();unsigned uploads=0;int fail_after=-1;
    session->upload=[&](const nt::Texture& texture)->std::shared_ptr<nt::Image>{nt::ValidateUpload(texture);if(fail_after==int(uploads))throw std::bad_alloc();++uploads;std::size_t size=0;for(const auto& level:texture.levels)size+=level.bytes.size();return std::make_shared<UnionImage>(arena,size);};
    UnionRouting device(session);assert(!p::Current().ready);p::Current().ready=true;p::Current().device=&device;
    unsigned expected_decodes=0;
    for(unsigned cycle=0;cycle<2;++cycle){observed.Reset();uploads=0;const auto start=std::chrono::steady_clock::now();assert(mg::BuildOriginalTextureUnions(storage,input,2100,50000)==0);const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        assert(input.dimension==16 && input.cells==256 && observed.observed==6 && uploads==6 && observed.progress_calls==1536 && observed.id_reads==observed.requested.size());
        if(!cycle)expected_decodes=observed.decoded.size();else assert(observed.decoded.size()==expected_decodes);
        for(unsigned i=0;i<6;++i){auto texture=nt::Find(CBottomTextureUnion::Get(i).GetTexture()->Tex());auto expected=mg::TextureAsset::Decode(ReadUnionFile(std::filesystem::path(argv[3])/("union-"+std::to_string(i)+"-native.dds")));assert(texture->levels.size()==6);
            for(unsigned level=0;level<6;++level)assert(texture->levels[level].bytes==expected.levels[level].bytes);}
        auto held=nt::Resident(nt::Find(CBottomTextureUnion::Get(0).GetTexture()->Tex()));CBottomTextureUnion::Clear();assert(nt::HandleCount()==0 && arena.Free()==arena.Capacity()-698880);held.reset();assert(arena.Free()==arena.Capacity());
        std::printf("original BuildTexUnions cycle=%u unions=6 composed_DDS=WINDOWS_EXACT BC1=EXACT elapsed_ms=%.3f unique_decodes=%zu source_reads=%u cached_bitmap_bytes=%zu progress_calls=%u\n",cycle,elapsed,observed.decoded.size(),observed.id_reads,observed.bitmap_cache_bytes,observed.progress_calls);
    }
    // The original algorithm now unwinds actual decoded bitmaps and partially
    // created typed textures when the third upload fails.
    observed.Reset();uploads=0;fail_after=2;bool failed=false;
    try{mg::BuildOriginalTextureUnions(storage,input,2100,50000);}catch(const std::exception&){failed=true;}assert(failed && uploads==2 && nt::HandleCount()==0 && arena.Free()==arena.Capacity());CBottomTextureUnion::Clear();assert(nt::HandleCount()==0 && arena.Free()==arena.Capacity());fail_after=-1;
    // Invalid referenced record is rejected before original array indexing.
    auto* unions=storage.GetBuf(DATA_TEXUINIONS,DATA_TEXUINIONS_DATA,Base::ST_BYTE);int* cells=unions->GetFirst<int>(0);const int saved=cells[0];cells[0]=0x7fffffff;failed=false;
    try{mg::BuildOriginalTextureUnions(storage,input,2100,50000);}catch(const std::exception&){failed=true;}cells[0]=saved;assert(failed && nt::HandleCount()==0);
    // Mutate only the loaded storage in RAM, never the protected package.
    // Find a mask actually consumed by this map's original compositor.
    auto* bottoms=storage.GetBuf(DATA_BOTTOM,DATA_BOTTOM_DATA,Base::ST_INT32);
    auto* bitmaps=storage.GetBuf(DATA_BITMAPS,DATA_BITMAPS_BITMAP,Base::ST_BYTE);int mask=-1;
    for(unsigned u=0;u<unions->GetArraysCount() && mask<0;++u){auto* entries=unions->GetFirst<int>(u);
        for(int c=0;c<input.cells && mask<0;++c)if(entries[c]>=0 && bottoms->GetArrayLength(entries[c])>=3)mask=bottoms->GetFirst<int>(entries[c])[2];}
    assert(mask>=0);auto* original=bitmaps->GetFirst<std::uint8_t>(mask);
    const std::vector<std::uint8_t> saved_png(original,original+bitmaps->GetArrayLength(mask));
    std::uint8_t pixel[]{1,2,3,255};const auto encoded_size=FilePNG::Write(nullptr,0,pixel,4,1,1,4,0);assert(encoded_size>0);
    std::vector<std::uint8_t> encoded(encoded_size);assert(FilePNG::Write(encoded.data(),encoded.size(),pixel,4,1,1,4,0)==encoded_size);
    std::vector<std::vector<std::uint8_t>> invalid_pngs{{},{saved_png.front()},encoded,saved_png};
    invalid_pngs.back()[16]=0x7f; // invalid IHDR extent/CRC, no giant allocation.
    const auto observe_good=input.observe;input.observe=nullptr;
    for(const auto& bytes:invalid_pngs){bitmaps->SetArray<std::uint8_t>(mask,bytes.data(),int(bytes.size()));observed.Reset();uploads=0;failed=false;
        try{mg::BuildOriginalTextureUnions(storage,input,2100,50000);}catch(const std::exception&){failed=true;}
        assert(failed && nt::HandleCount()==0 && arena.Free()==arena.Capacity());}
    bitmaps->SetArray<std::uint8_t>(mask,saved_png.data(),int(saved_png.size()));input.observe=observe_good;
    std::puts("original compositor empty/truncated/short-valid/corrupt-dimension PNG rejection and batch cleanup PASS cases=4");
    session->Shutdown();p::Current().device=nullptr;p::Current().ready=false;Base::CFile::ReleasePackFiles();
    std::puts("original native texture union assembly/typed BC1/rebuild/third-upload OOM/record bounds PASS CPU_GPU_UPLOAD_MODEL_ONLY NO_GAME_INIT SWITCH_HARDWARE_NOT_RUN");
}
