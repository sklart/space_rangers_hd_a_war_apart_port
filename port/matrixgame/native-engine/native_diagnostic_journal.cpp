// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_diagnostic_journal.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace srhd_awa::matrixgame::diagnostic {
namespace {
std::atomic<Journal*> active{};
std::atomic<unsigned> ordinal{};
std::uint64_t Micros()noexcept{return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::size_t Write(const void* data,std::size_t bytes,FILE* file){return std::fwrite(data,1,bytes,file);}
int Flush(FILE* file){
    if(std::fflush(file))return -1;
#ifdef _WIN32
    return _commit(_fileno(file));
#else
    return fsync(fileno(file));
#endif
}
bool Rooted(const std::filesystem::path& path){
    if(path.is_absolute())return true;
    const auto s=path.generic_u8string();const auto p=s.find(':');
    return p!=s.npos && p>0 && p+1<s.size() && s[p+1]=='/' && s.find('/')>p;
}
bool Copy(std::span<char> destination,std::string_view value)noexcept{
    if(value.size()>=destination.size() || value.find('\0')!=value.npos)return false;
    std::memcpy(destination.data(),value.data(),value.size());destination[value.size()]=0;return true;
}
struct Line {
    std::array<char,8192> data{};std::size_t used{};bool valid{true};
    void Add(std::string_view s)noexcept{
        if(s.size()>data.size()-used){valid=false;return;}
        std::memcpy(data.data()+used,s.data(),s.size());used+=s.size();
    }
    template<class T>void Number(T v)noexcept{
        char buffer[32];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),v);
        if(r.ec!=std::errc{})valid=false;else Add({buffer,std::size_t(r.ptr-buffer)});
    }
    void String(std::string_view value)noexcept{
        Add("\"");constexpr char hex[]="0123456789abcdef";
        for(unsigned char c:value){
            if(c=='"')Add("\\\"");else if(c=='\\')Add("\\\\");
            else if(c<32){const char escape[]{'\\','u','0','0',hex[c>>4],hex[c&15]};Add({escape,6});}
            else{const char byte=static_cast<char>(c);Add({&byte,1});}
        }Add("\"");
    }
};
}
IO SystemIO()noexcept{return {Write,Flush,std::fclose};}
Journal::Journal(const std::filesystem::path& root,std::string_view build,std::string_view source,IO io):io_(io),started_(Micros()){
    if(!Rooted(root) || !io_.write || !io_.flush || !io_.close || build.empty() || !Copy(build_id_,build) ||
       source.size()!=64 || !std::all_of(source.begin(),source.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}) || !Copy(source_sha_,source))
        throw std::invalid_argument("invalid diagnostic root/build/source/IO");
    std::filesystem::create_directories(root);
    const auto epoch=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for(unsigned attempt=0;attempt<1000;++attempt){
        std::snprintf(run_id_.data(),run_id_.size(),"run-%llu-%u",static_cast<unsigned long long>(epoch),ordinal.fetch_add(1));
        directory_=root/run_id_.data();std::error_code error;
        if(std::filesystem::create_directory(directory_,error))break;
        if(error)throw std::filesystem::filesystem_error("create diagnostic run directory",directory_,error);
        if(attempt==999)throw std::runtime_error("diagnostic run directory collision limit");
    }
    path_=directory_/"run.jsonl";
#ifdef _WIN32
    file_=_wfopen(path_.c_str(),L"wb");
#else
    file_=std::fopen(path_.c_str(),"wb");
#endif
    if(!file_)throw std::system_error(errno,std::generic_category(),"open diagnostic journal");
    if(!Emit("journal_open","entry","durable journal opened before GPU allocation",0,0)){
        const auto error=error_;io_.close(file_);file_=nullptr;
        throw std::system_error(error,std::generic_category(),"flush initial diagnostic record");
    }
}
Journal::~Journal(){Close();}
bool Journal::Emit(std::string_view event,std::string_view operation,std::string_view detail,std::int64_t result,std::uint64_t elapsed,std::uint64_t value)noexcept{
    if(error_ || !file_)return false;
    if(event=="failed" || event=="exception" || event=="sdk_error")++failures_;
    if(event.size()>40 || operation.size()>96){error_=EINVAL;return false;}
    const bool truncated=detail.size()>768;
    if(truncated){std::size_t cut=768;while(cut && (static_cast<unsigned char>(detail[cut])&0xc0)==0x80)--cut;detail=detail.substr(0,cut);}
    Line line;line.Add("{\"schema\":1,\"seq\":");line.Number(++sequence_);
    line.Add(",\"time_us\":");line.Number(Micros()-started_);
    line.Add(",\"run_id\":");line.String(run_id_.data());line.Add(",\"build_id\":");line.String(build_id_.data());
    line.Add(",\"source_sha256\":");line.String(source_sha_.data());line.Add(",\"event\":");line.String(event);
    line.Add(",\"operation\":");line.String(operation);line.Add(",\"last_ok\":");line.String(last_ok_.data());
    line.Add(",\"detail\":");line.String(detail);line.Add(",\"detail_truncated\":");line.Add(truncated?"true":"false");
    line.Add(",\"result\":");if(result==std::numeric_limits<std::int64_t>::min())line.Add("null");else line.Number(result);line.Add(",\"elapsed_us\":");line.Number(elapsed);line.Add(",\"value\":");line.Number(value);line.Add("}\n");
    if(!line.valid){error_=EOVERFLOW;return false;}
    errno=0;
    const auto io_started=Micros();
    const bool failed=io_.write(line.data.data(),line.used,file_)!=line.used || io_.flush(file_)!=0;
    ++io_stats_.calls;io_stats_.bytes+=line.used;io_stats_.us+=Micros()-io_started;
    if(failed){error_=errno?errno:EIO;return false;}
    return true;
}
bool Journal::Record(std::string_view event,std::string_view operation,std::string_view detail,std::int64_t result)noexcept{
    std::lock_guard lock(mutex_);return Emit(event,operation,detail,result,0);
}
bool Journal::Metric(std::string_view name,std::uint64_t value)noexcept{
    std::lock_guard lock(mutex_);return Emit("metric",name,{},0,0,value);
}
std::uint64_t Journal::Begin(std::string_view operation){
    std::lock_guard lock(mutex_);
    if(error_ || !file_)throw std::runtime_error("diagnostic journal unavailable before operation");
    if(depth_==stack_.size() || operation.empty() || operation.size()>96 || operation.find('\0')!=operation.npos)
        throw std::invalid_argument("invalid diagnostic operation nesting/name");
    auto& stage=stack_[depth_++];Copy(stage.name,operation);stage.token=++token_;stage.started=Micros();
    if(!Emit("begin",operation,{},0,0))throw std::system_error(error_,std::generic_category(),"diagnostic begin flush");
    return stage.token;
}
bool Journal::End(std::uint64_t token,bool success,std::int64_t result,bool cancelled)noexcept{
    std::lock_guard lock(mutex_);
    if(!depth_ || stack_[depth_-1].token!=token){if(!error_)error_=EINVAL;return false;}
    const auto& stage=stack_[depth_-1];const auto ok=Emit(cancelled?"cancelled":success?"ok":"failed",stage.name.data(),{},result,Micros()-stage.started);
    if(ok && success && !cancelled)last_ok_=stage.name;
    --depth_;return ok;
}
bool Journal::Healthy()const noexcept{std::lock_guard lock(mutex_);return file_ && !error_;}
int Journal::Error()const noexcept{std::lock_guard lock(mutex_);return error_;}
std::uint64_t Journal::Failures()const noexcept{std::lock_guard lock(mutex_);return failures_;}
JournalIOStats Journal::Statistics()const noexcept{std::lock_guard lock(mutex_);return io_stats_;}
void Journal::RequireHealthy()const{if(!Healthy())throw std::runtime_error("diagnostic journal not healthy");}
bool Journal::Close()noexcept{
    std::lock_guard lock(mutex_);if(closed_)return !error_;closed_=true;
    if(file_){Emit("journal_close","cleanup",depth_?"unfinished operations remain":"all operation scopes retired",depth_?-1:0,0);errno=0;if(io_.close(file_) && !error_)error_=errno?errno:EIO;file_=nullptr;}
    return !error_;
}
Binding::Binding(Journal& journal):journal_(&journal){Journal* expected=nullptr;if(!active.compare_exchange_strong(expected,journal_))throw std::logic_error("diagnostic journal already bound");}
Binding::~Binding(){auto* expected=journal_;if(!active.compare_exchange_strong(expected,nullptr))std::terminate();}
Operation::Operation(std::string_view name):journal_(active.load()),exceptions_(std::uncaught_exceptions()){
    if(journal_)token_=journal_->Begin(name);
}
Operation::~Operation(){const bool unwind=exceptions_!=std::uncaught_exceptions();if(journal_)journal_->End(token_,!failed_ && !unwind,unwind && !failed_?std::numeric_limits<std::int64_t>::min():result_,cancelled_ && !failed_);}
void Record(std::string_view event,std::string_view operation,std::string_view detail,std::int64_t result)noexcept{if(auto* journal=active.load())journal->Record(event,operation,detail,event=="exception" && !result?std::numeric_limits<std::int64_t>::min():result);}
void Metric(std::string_view name,std::uint64_t value)noexcept{if(auto* journal=active.load())journal->Metric(name,value);}
JournalIOStats CurrentIOStats()noexcept{if(auto* journal=active.load())return journal->Statistics();return {};}
}
