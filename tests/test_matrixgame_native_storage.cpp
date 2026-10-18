// Contract for the original CStorage format and native ownership/UTF-16 edits.
#include "CStorage.hpp"
#include "CFile.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#define CHECK(e) do { if (!(e)) { std::fprintf(stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #e); std::fflush(stderr); std::exit(3); } } while (0)
using namespace Base;
namespace fs = std::filesystem;

template<class T> T value(CDataBuf* buf, int array, uint32_t index) {
    CHECK(buf);
#ifdef MATRIXGAME_ORIGINAL_STORAGE_REFERENCE
    CHECK(index < buf->GetArrayLength(array));
    T result;
    std::memcpy(&result, buf->GetFirst<byte>(array) + size_t(index) * sizeof(T), sizeof(T));
    return result;
#else
    return buf->GetValue<T>(array, index);
#endif
}
uint32_t hash(const CBuf& buf) {
    auto* bytes = static_cast<const byte*>(buf.Get()); uint32_t result = 2166136261u;
    for (size_t i = 0; i < buf.Len(); ++i) result = (result ^ bytes[i]) * 16777619u;
    return result;
}
void dump(const fs::path& path, const CBuf& buf) {
    std::ofstream file(path, std::ios::binary); CHECK(file);
    file.write(static_cast<const char*>(buf.Get()), static_cast<std::streamsize>(buf.Len())); CHECK(file);
}
std::wstring wide(const fs::path& path) {
#ifdef _WIN32
    return path.wstring();
#else
    const auto s = path.native(); return {s.begin(), s.end()}; // fixture roots are ASCII
#endif
}
void populate(CStorage& storage) {
    CStorageRecord record(L"main-\u0416\U0001f680");
    for (const auto& item : std::array<std::pair<const wchar_t*,EStorageType>,6>{{
        {L"bytes",ST_BYTE},{L"ints",ST_INT32},{L"words",ST_DWORD},
        {L"floats",ST_FLOAT},{L"doubles",ST_DOUBLE},{L"text",ST_WCHAR}}})
        record.AddItem(CStorageRecordItem(item.first, item.second));
    storage.AddRecord(record);
    const wchar_t* table=L"main-\u0416\U0001f680";
    auto* bytes=storage.GetBuf(table,L"bytes",ST_BYTE);
    for (unsigned array=0;array<3;++array) {
        int index=bytes->AddArray();
        unsigned count=array==0?1:array==1?100:131071;
        std::vector<byte> data(count);
        for (unsigned i=0;i<count;++i)data[i]=byte((i*17+array)%251);
        bytes->AddToArray<byte>(index,data.data(),int(data.size()));
    }
    auto* ints=storage.GetBuf(table,L"ints",ST_INT32); ints->AddArray();
    for(int i=0;i<53;++i)ints->AddToArray<int32_t>(0,-i*7919);
    auto* words=storage.GetBuf(table,L"words",ST_DWORD); words->AddArray();
    for(uint32_t i=0;i<53;++i)words->AddToArray<uint32_t>(0,0xf0000000u+i);
    auto* floats=storage.GetBuf(table,L"floats",ST_FLOAT); floats->AddArray();
    for(int i=0;i<53;++i)floats->AddToArray<float>(0,float(i)/8.0f);
    auto* doubles=storage.GetBuf(table,L"doubles",ST_DOUBLE); doubles->AddArray();
    for(int i=0;i<53;++i)doubles->AddToArray<double>(0,double(i)/16.0);
    auto* text=storage.GetBuf(table,L"text",ST_WCHAR);
    for(const auto& str:std::vector<std::wstring>{L"",L"A\u0416\u6c34\U0001f680",
        std::wstring{wchar_t(0xd800),L'X',wchar_t(0xdcff)},std::wstring(L"A\0B",3)})
        text->AddWStr(str);
    // Loaded data pointers must survive record-array growth and deletion.
    for(int i=0;i<80;++i) { CStorageRecord next(L"t"+std::to_wstring(i)); storage.AddRecord(next); }
    CHECK(bytes==storage.GetBuf(table,L"bytes",ST_BYTE));
    storage.DelRecord(L"t3"); storage.DelRecord(L"t79"); storage.DelRecord(L"absent");
}
void check(CStorage& storage) {
    const wchar_t* table=L"main-\u0416\U0001f680";
    CHECK(storage.IsTablePresent(table) && !storage.IsTablePresent(L"t3") && !storage.IsTablePresent(L"t79"));
    CHECK(!storage.GetBuf(table,L"missing",ST_BYTE));
    auto* bytes=storage.GetBuf(table,L"bytes",ST_BYTE); CHECK(bytes && bytes->GetArraysCount()==3);
    for(unsigned array=0;array<3;++array) {
        unsigned count=array==0?1:array==1?100:131071;
        CHECK(bytes->GetArrayLength(array)==count);
        for(unsigned i=0;i<count;++i)CHECK(value<byte>(bytes,array,i)==byte((i*17+array)%251));
    }
    for(uint32_t i=0;i<53;++i) {
        CHECK(value<int32_t>(storage.GetBuf(table,L"ints",ST_INT32),0,i)==-int32_t(i)*7919);
        CHECK(value<uint32_t>(storage.GetBuf(table,L"words",ST_DWORD),0,i)==0xf0000000u+i);
        CHECK(value<float>(storage.GetBuf(table,L"floats",ST_FLOAT),0,i)==float(i)/8.0f);
        CHECK(value<double>(storage.GetBuf(table,L"doubles",ST_DOUBLE),0,i)==double(i)/16.0);
    }
    auto* text=storage.GetBuf(table,L"text",ST_WCHAR);
    CHECK(text && text->GetArraysCount()==4);
    CHECK(text->GetAsWStr(0).empty() && text->GetAsWStr(1)==L"A\u0416\u6c34\U0001f680");
    CHECK(text->GetAsWStr(2)==std::wstring({wchar_t(0xd800),L'X',wchar_t(0xdcff)}));
    CHECK(text->GetAsWStr(3)==std::wstring(L"A\0B",3));
    CHECK(text->FindAsWStr(L"A\u0416\u6c34\U0001f680")==1);
}
#ifndef MATRIXGAME_ORIGINAL_STORAGE_REFERENCE
bool reject(CStorage& storage, CBuf& input) {
    try { return !storage.Load(input); } catch(const Base::CException&) { return true; }
}
#endif
int run(int argc,char**argv) {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    CHECK(argc==2 || argc==4);
    const fs::path output=fs::absolute(argv[1]); fs::create_directories(output);
    CStorage storage; populate(storage); check(storage);
    CBuf plain; storage.Save(plain,false); check(storage);
    dump(output/"fixture.strg",plain);
    const uint32_t uniq=storage.CalcUniqID();
    CStorage loaded; CHECK(loaded.Load(plain)); check(loaded); CHECK(loaded.CalcUniqID()==uniq);
    CBuf second; loaded.Save(second,false); CHECK(second.Len()==plain.Len() && hash(second)==hash(plain));
    CBuf compressed; storage.Save(compressed,true); dump(output/"fixture-compressed.strg",compressed);
    CStorage zipped; CHECK(zipped.Load(compressed)); check(zipped); CHECK(zipped.CalcUniqID()==uniq);
#ifndef MATRIXGAME_ORIGINAL_STORAGE_REFERENCE
    for(size_t length:std::vector<size_t>{0,1,4,7,8,11,12,13,31,plain.Len()-1}) {
        CBuf truncated; truncated.Add(plain.Get(),length);
        CHECK(reject(loaded,truncated)); CHECK(loaded.CalcUniqID()==uniq);
    }
    for(size_t at:std::vector<size_t>{8,plain.Len()-4}) {
        CBuf bad=plain; std::memset(static_cast<byte*>(bad.Get())+at,0xff,4);
        CHECK(reject(loaded,bad)); CHECK(loaded.CalcUniqID()==uniq);
    }
    CBuf corrupt=compressed;
    static_cast<byte*>(corrupt.Get())[corrupt.Len()-1]^=0xff;
    CHECK(reject(loaded,corrupt) && loaded.CalcUniqID()==uniq);
    // Reject forged block counts before allocating a decoded buffer; preserve old data.
    for(uint32_t blocks:std::array<uint32_t,4>{0,4096,0x10000,0xffffffff}) {
        CBuf bad=compressed; std::memcpy(bad.Buff<byte>()+12,&blocks,4);
        CHECK(reject(loaded,bad) && loaded.CalcUniqID()==uniq);
    }
#ifndef _WIN32
    // Sparse disk input exercises the pre-allocation file-size gate without a large fixture.
    const auto oversized=fs::temp_directory_path()/
        ("matrixgame-storage-"+std::to_string(std::hash<std::string>{}(output.string()))+".strg");
    { std::ofstream stream(oversized,std::ios::binary); stream.put('S'); CHECK(bool(stream)); }
    fs::resize_file(oversized,matrixgame::native::StorageLimit+1);
    bool refused=false;
    try { refused=!loaded.Load(wide(oversized).c_str()); } catch(const Base::CException&) { refused=true; }
    fs::remove(oversized);
    CHECK(refused && loaded.CalcUniqID()==uniq);
#endif
    CStorage empty; CBuf tiny; empty.Save(tiny,true); CHECK(empty.Load(tiny));
    CBuf moved=std::move(tiny); CHECK(tiny.Len()==0 && tiny.Pointer()==0 && moved.Len()>0);
    CStorageRecordItem item(L"packed",ST_BYTE); item.InitBuf(nullptr);
    auto* data=item.GetBuf(ST_BYTE); data->AddArray(); data->AddToArray<byte>(0,93);
    CBuf column; item.Save(column,true); column.Pointer(0);
    CStorageRecordItem read(nullptr); CHECK(read.Load(column) && column.Pointer()==column.Len());
    CHECK(value<byte>(read.GetBuf(ST_BYTE),0,0)==93);
    read.ReleaseBuf(nullptr); item.ReleaseBuf(nullptr);
#endif
    std::printf("{\"plain_bytes\":%zu,\"plain_fnv32\":\"%08x\",\"storage_uid\":\"%08x\",\"types\":6}\n",
        plain.Len(),hash(plain),uniq);
    if(argc==4) {
        CFile::StaticInit(); const auto package=wide(fs::absolute(argv[2]));
        CFile::AddPackFile(package.c_str()); CFile::OpenPackFiles();
        {
            const std::string name(argv[3]);const std::wstring entry(name.begin(),name.end());
            CStorage asset; CHECK(asset.Load(entry.c_str()));
            const auto id=asset.CalcUniqID();
            CBuf exported; asset.Save(exported,false); dump(output/"real-export.strg",exported);
            CStorage again; CHECK(again.Load(exported) && again.CalcUniqID()==asset.CalcUniqID());
            std::printf("{\"real_storage_uid\":\"%08x\",\"real_export_bytes\":%zu,\"real_export_fnv32\":\"%08x\"}\n",
                id,exported.Len(),hash(exported));
        }
        CFile::ReleasePackFiles();
    }
    return 0;
}

int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const Base::CException& error) {
        const auto text = error.Info();
        const std::string diagnostic(text.begin(), text.end());
        std::fprintf(stderr, "CStorage exception: %s\n", diagnostic.c_str());
        return 2;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "CStorage standard exception: %s\n", error.what());
        return 2;
    }
}
