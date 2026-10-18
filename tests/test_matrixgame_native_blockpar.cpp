// Same valid parser/configuration cases run on original Win32 x86 and native CPU core.
#include "CBlockPar.hpp"
#include "CStorage.hpp"
#include "CFile.hpp"
#include "utf16.hpp"
#include "utils.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#define CHECK(e) do { if (!(e)) { std::fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e); std::exit(3); } } while(0)
using namespace Base;
namespace fs=std::filesystem;
using matrixgame::native::EncodeUtf16LE;
uint64_t digest=14695981039346656037ull;
void bytes(const void* data,size_t size) { auto*p=static_cast<const uint8_t*>(data); for(size_t i=0;i<size;++i)digest=(digest^p[i])*1099511628211ull; }
template<class T> void scalar(T value) { bytes(&value,sizeof(value)); }
void text(std::wstring_view value) { auto data=EncodeUtf16LE(value); scalar(uint32_t(data.size())); bytes(data.data(),data.size()); }
struct Counts { unsigned blocks=0,parameters=0; };
void tree(const CBlockPar& block,Counts& count) {
    ++count.blocks; scalar(uint32_t(block.ParCount())); scalar(uint32_t(block.BlockCount()));
    for(int i=0;i<block.ParCount();++i) { ++count.parameters; text(block.ParGetName(i)); text(block.ParGet(i)); }
    for(int i=0;i<block.BlockCount();++i) { text(block.BlockGetName(i)); tree(*block.BlockGet(i),count); }
}
uint64_t hash(const CBlockPar& block,Counts* count=nullptr) {
    const auto previous=digest; digest=14695981039346656037ull; Counts found; tree(block,found);
    auto result=digest; digest=previous; if(count)*count=found; return result;
}
void dump(const fs::path& path,const CBuf& buf) { std::ofstream out(path,std::ios::binary); out.write(static_cast<const char*>(buf.Get()),buf.Len()); CHECK(bool(out)); }
std::wstring wide(const fs::path& path) { return utils::to_wstring(path.string()); }
int run(int argc,char**argv) {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    CHECK(argc==2||argc==3); const fs::path output=fs::absolute(argv[1]); fs::create_directories(output);
    // Every UTF-16 unit detects signed-wchar/hex-subtraction differences.
    for(unsigned unit=0;unit<65536;++unit) {
        ParamParser value(std::wstring(1,wchar_t(unit)));
        scalar(int32_t(value.GetInt())); scalar(value.GetDword()); scalar(value.GetHexUnsigned());
    }
    for(const auto& str:std::vector<std::wstring>{L"",L"12a3-4",L"-2147483648",L"4294967295",L"4294967296",L"9999999999999999999999999",L"-.a12.3x4",L"0xFF",L"A\U00010030Z"}) {
        ParamParser value(str); scalar(int32_t(value.GetInt())); scalar(value.GetDword()); scalar(value.GetHexUnsigned());
        scalar(std::bit_cast<uint64_t>(value.GetDouble())); scalar(uint32_t(value.IsOnlyInt()));
    }
    const auto numeric=digest;
    const std::wstring long_value(9000,L'z');
    CHECK(utils::format(L"%ls",long_value.c_str())==long_value);
    CHECK(utils::format(L"%.8f",1.25)==L"1.25000000");
    CHECK(ParamParser(L"oN").GetBool() && !ParamParser(L"FaLsE").GetBool());
    ParamParser split(L"one,,three=4"); CHECK(split.GetCountPar(L",=")==4);
    CHECK(split.GetStrPar(1,L",=").empty() && split.GetStrPar(2,3,L",=")==L"three=4");
    const std::wstring input=L"// comment\r\na=1\r\na=two // tail\r\nouter {\r\nname=\u0416\u6c34\U0001f680\r\ninner {\r\nx=-1.25\r\n}\r\n}\r\nempty {}\r\nunnamed\r\n";
    CBlockPar block; CHECK(block.LoadFromText(input)>0);
    CHECK(block.ParCount(L"a")==2 && block.ParGet(L"a",1)==L"two");
    CHECK(block.BlockGet(L"outer")->ParGet(L"name")==L"\u0416\u6c34\U0001f680");
    auto canonical=hash(block);
    CBlockPar copy; copy.CopyFrom(block); CHECK(hash(copy)==canonical);
    copy.ParPathSetAdd(L"outer.inner.x",L"7");
    CHECK(copy.ParPathGet(L"outer.inner.x")==L"7");
    block.SaveInTextFile(wide(output/"fixture-utf16.txt"),false);
    block.SaveInTextFile(wide(output/"fixture-utf8.txt"),true);
    CBlockPar from16,from8;
    from16.LoadFromTextFile(wide(output/"fixture-utf16.txt")); from8.LoadFromTextFile(wide(output/"fixture-utf8.txt"));
    CHECK(hash(from16)==canonical && hash(from8)==canonical);
    CBlockPar include;
    include.LoadFromText(L"inc ="+wide(output/"fixture-utf16.txt")+L" {}\r\n");
    CHECK(hash(*include.BlockGet(L"inc"))==canonical);
    CBlockPar unchanged; unchanged.CopyFrom(block);
    for (const auto& empty : std::vector<std::string>{std::string{},std::string("\xff\xfe",2)}) {
        const auto filename=output/"empty-text.txt";
        { std::ofstream out(filename,std::ios::binary); out.write(empty.data(),empty.size()); }
        unchanged.LoadFromTextFile(wide(filename)); CHECK(hash(unchanged)==canonical);
    }
    CStorage packed; packed.StoreBlockPar(L"da",block); CBuf stored; packed.Save(stored,false); dump(output/"fixture-config.strg",stored);
    CStorage loaded; CHECK(loaded.Load(stored)); CBlockPar restored; loaded.RestoreBlockPar(L"da",restored); CHECK(hash(restored)==canonical);
    Counts counts; (void)hash(block,&counts);
    std::printf("{\"numeric_fnv64\":\"%016llx\",\"tree_fnv64\":\"%016llx\",\"blocks\":%u,\"parameters\":%u}\n",(unsigned long long)numeric,(unsigned long long)canonical,counts.blocks,counts.parameters);
#ifndef MATRIXGAME_ORIGINAL_BLOCKPAR_REFERENCE
    bool rejected=false;
    try { CStorage invalid; CBlockPar target; invalid.RestoreBlockPar(L"missing",target); } catch(const CException&) { rejected=true; }
    CHECK(rejected);
    for (const auto& bad : std::vector<std::string>{std::string("\xff\xfe" "A",3), std::string("\xff",1)}) {
        const auto filename=output/"invalid-text.txt";
        { std::ofstream out(filename,std::ios::binary); out.write(bad.data(),bad.size()); }
        rejected=false;
        try { CBlockPar invalid; invalid.LoadFromTextFile(wide(filename)); }
        catch (const std::exception&) { rejected=true; }
        catch (const CException&) { rejected=true; }
        CHECK(rejected);
    }
    // A storage reference cycle must fail before overflowing the process stack.
    packed.GetBuf(L"da",L"3",ST_WCHAR)->SetWStr(0,L"da");
    rejected=false; try { CBlockPar cyclic; packed.RestoreBlockPar(L"da",cyclic); }
    catch(const CException&) { rejected=true; } CHECK(rejected);
    CBlockPar after_error; loaded.RestoreBlockPar(L"da",after_error); CHECK(hash(after_error)==canonical);
    rejected=false; try { CBlockPar deep; std::wstring source; for(unsigned i=0;i<140;++i)source+=L"a {\n"; deep.LoadFromText(source); } catch(const CException&) { rejected=true; } CHECK(rejected);
#endif
    if(argc==3) {
        CStorage real; CHECK(real.Load(wide(fs::absolute(argv[2])).c_str()));
        for(const auto root:{L"da",L"if"}) {
            CBlockPar config; real.RestoreBlockPar(root,config); Counts found; auto value=hash(config,&found);
            std::printf("{\"real_root\":\"%s\",\"tree_fnv64\":\"%016llx\",\"blocks\":%u,\"parameters\":%u}\n",root==std::wstring(L"da")?"da":"if",(unsigned long long)value,found.blocks,found.parameters);
        }
    }
    return 0;
}
int main(int argc,char**argv) {
    try { return run(argc,argv); }
    catch(const CException& error) { auto info=error.Info(); std::fprintf(stderr,"BlockPar: %s\n",std::string(info.begin(),info.end()).c_str()); return 2; }
    catch(const std::exception& error) { std::fprintf(stderr,"BlockPar: %s\n",error.what()); return 2; }
}
