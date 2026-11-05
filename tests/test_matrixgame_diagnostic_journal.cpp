#include "native_diagnostic_journal.hpp"
#include "native_diagnostic_control.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <limits>
namespace d=srhd_awa::matrixgame::diagnostic;
namespace fs=std::filesystem;
namespace {
unsigned writes{},flushes{},syncs{},closes{};int fault{};
std::size_t Write(const void* p,std::size_t n,FILE* f){++writes;if(fault==1){errno=ENOSPC;return std::fwrite(p,1,n/2,f);}return d::SystemIO().write(p,n,f);}
int Flush(FILE* f){++flushes;if(fault==2){errno=EIO;return -1;}return d::SystemIO().flush(f);}
int Close(FILE* f){++closes;const auto r=std::fclose(f);if(fault==3){errno=EIO;return EOF;}return r;}
int Sync(FILE* f){++syncs;if(fault==4){errno=EIO;return -1;}return d::SystemIO().sync(f);}
constexpr auto sha="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
std::string Text(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
}
int main(int argc,char** argv){
    {
        using Result=d::RunControl::PollResult;
        d::RunControl policy(100,1000,4);assert(policy.Poll(100,true)==Result::Continue);
        assert(policy.Poll(101,true)==Result::Continue);assert(policy.Poll(102,false)==Result::Continue);
        assert(policy.Poll(103,true)==Result::UserExit);assert(policy.Poll(1100,false)==Result::Deadline);
        Reject([&]{policy.Poll(1099,false);});assert(!policy.Presented(0,0));
        assert(!policy.Presented(7,8));assert(!policy.Presented(8,10));assert(policy.Presented(10,11));assert(policy.Frames()==4);
        Reject([&]{policy.Presented(12,11);});Reject([&]{d::RunControl bad(0,0,4);});Reject([&]{d::RunControl bad(0,1,0);});
        d::RunControl overflow(0,1,UINT64_MAX);assert(!overflow.Presented(0,UINT64_MAX-1));Reject([&]{overflow.Presented(0,2);});
    }
    assert(argc==2);const auto cwd=fs::current_path();const fs::path root=fs::u8path(argv[1]);fs::create_directories(root);
    Reject([&]{d::Journal j("relative-path","fixture",sha);});
    Reject([&]{d::Journal j(root,"fixture","missing digest");});
    const auto unicode=root/fs::path(u8"журнал-🚀");
    fs::path first;
    {
        d::Journal j(unicode,"fixture-build",sha,{Write,Flush,Close,Sync});first=j.Directory();d::Binding binding(j);
        Reject([&]{d::Binding duplicate(j);});
        {
            d::Operation outer("OriginalInit");
            // A separate file read proves the begin is visible BEFORE work.
            assert(Text(j.Path()).find("\"event\":\"begin\",\"operation\":\"OriginalInit\"")!=std::string::npos);
            {d::Operation inner("RobotPreload");}
            try{d::Operation failure("stor.Load");throw std::runtime_error("actual exception boundary");}catch(const std::runtime_error&){}
            try{
                d::Operation stopped("ControlledDraw");
                try{throw 1;}catch(int){stopped.Cancel();throw;}
            }catch(int){}
            assert(Text(j.Path()).find("\"event\":\"cancelled\",\"operation\":\"ControlledDraw\"")!=std::string::npos);
            d::Record("exception","stor.Load","\"quoted\"\nUnicode: \xd0\xba\xd0\xb0\xd1\x80\xd1\x82\xd0\xb0",-17);
            {d::Operation returned_failure("ResultOperation");returned_failure.Fail(0xdead);}
        }
        std::vector<std::thread> threads;
        for(unsigned i=0;i<4;++i)threads.emplace_back([]{for(unsigned n=0;n<50;++n)d::Metric("worker_counter",n);});
        for(auto& thread:threads)thread.join();
        assert(j.Failures()==3);
        assert(j.Record("sdk_error","deko3d.fixture","SDK callback without a stage token",3));
        assert(j.Failures()==4);
        assert(j.Metric("max_uint64",std::numeric_limits<std::uint64_t>::max()));
        assert(j.Record("exception","long_detail",std::string(2000,'\n')));
        const std::string long_utf8=std::string(767,'a')+"\xf0\x9f\x9a\x80"+std::string(100,'x');
        assert(j.Record("exception","utf8_detail",long_utf8));
        std::vector<std::uint64_t> tokens;for(unsigned i=0;i<32;++i)tokens.push_back(j.Begin("depth"));
        Reject([&]{j.Begin("overflow");});for(auto it=tokens.rbegin();it!=tokens.rend();++it)assert(j.End(*it,true));
        assert(j.Close());assert(j.Close());assert(!j.Healthy());
        const auto stats=j.Statistics();
        assert(stats.calls==writes && stats.flush_calls==flushes && stats.sync_calls==syncs);
        assert(stats.calls==stats.flush_calls && stats.calls==stats.sync_calls);
        assert(stats.us==stats.write_us+stats.flush_us+stats.sync_us);
        assert(stats.bytes==fs::file_size(j.Path()));
    }
    {
        d::Journal second(unicode,"fixture-build",sha);assert(second.Directory()!=first);assert(second.Close());
    }
    {
        d::Journal j(root,"batch-success",sha,{Write,Flush,Close,Sync});d::Binding bind(j);
        {d::Operation boot("BOOT");assert(Text(j.Path()).find("\"operation\":\"BOOT\"")!=std::string::npos);}
        assert(j.Statistics().durable_rows==3);
        j.BeginBatching();
        for(unsigned n=0;n<10;++n)assert(j.Metric("batch_pending",n));
        assert(j.Statistics().durable_rows==3 && Text(j.Path()).find("batch_pending")==std::string::npos);
        std::this_thread::sleep_for(std::chrono::microseconds(d::Journal::BatchIntervalUs));
        assert(j.Checkpoint());
        const auto timed=j.Statistics();
        assert(timed.calls==4 && timed.sync_calls==4 && timed.durable_rows==13);
        assert(j.Checkpoint(true) && j.Statistics().calls==timed.calls);
        const std::string detail(768,'x');
        for(unsigned n=0;n<200;++n)assert(j.Record("sample","capacity_test",detail));
        const auto bounded=j.Statistics();
        assert(bounded.calls>timed.calls && bounded.durable_rows>timed.durable_rows && bounded.durable_rows<213);
        assert(j.Record("exception","urgent_error","persist preceding records too"));
        assert(j.Statistics().durable_rows==214);
        assert(Text(j.Path()).find("urgent_error")!=std::string::npos);
        assert(j.Metric("before_controlled_stop",1));
        {d::Operation stopped("controlled_stop");stopped.Cancel();}
        assert(j.Statistics().durable_rows==217);
        assert(j.Metric("before_teardown",2));
        assert(j.EndBatching() && j.Statistics().durable_rows==218);
        assert(j.Metric("synchronous_teardown",3));
        assert(Text(j.Path()).find("synchronous_teardown")!=std::string::npos);
        j.BeginBatching();assert(j.Metric("destructor_fallback",4));
        assert(j.Close() && j.Statistics().durable_rows==221);
        assert(j.Statistics().calls<j.Statistics().durable_rows);
    }
    for(int mode:{1,2,4}){
        d::Journal j(root,"batch-failure",sha,{Write,Flush,Close,Sync});
        j.BeginBatching();assert(j.Metric("unsaved_on_failure",1));assert(j.Metric("unsaved_on_failure",2));
        fault=mode;
        assert(!j.Checkpoint(true) && j.Error()==(mode==1?ENOSPC:EIO));
        const auto stats=j.Statistics();
        assert(stats.calls==2 && stats.durable_rows==1);
        assert(!j.Metric("must_not_accept",3));
        fault=0;
        assert(!j.EndBatching() && !j.Close());
        assert(j.Statistics().calls==stats.calls);
        assert(Text(j.Path()).find("journal_close")==std::string::npos);
    }
    unsigned failures=0;
    for(int mode:{1,2,4}){
        fault=mode;const auto before=closes;Reject([&]{d::Journal j(root,"fixture",sha,{Write,Flush,Close,Sync});});assert(closes==before+1);++failures;fault=0;
    }
    for(int mode:{1,2,4}){
        d::Journal j(root,"fixture",sha,{Write,Flush,Close,Sync});d::Binding bind(j);fault=mode;bool operation_executed=false;
        Reject([&]{d::Operation operation("gpu_allocation");operation_executed=true;});assert(!operation_executed && !j.Healthy() && j.Error());
        assert(j.Error()==(mode==1?ENOSPC:EIO));
        const auto stats=j.Statistics();
        assert(stats.calls==2 && stats.flush_calls==(mode==1?1u:2u) && stats.sync_calls==(mode==4?2u:1u));
        const auto before=writes;assert(!j.Record("retry","must_not_report_success"));assert(writes==before);fault=0;assert(!j.Close());++failures;
    }
    {
        d::Journal j(root,"fixture",sha,{Write,Flush,Close});fault=3;assert(!j.Close());assert(j.Error());fault=0;++failures;
    }
    {
        d::Journal j(root,"fixture",sha);const auto a=j.Begin("parent");j.Begin("child");assert(!j.End(a,true));assert(!j.Healthy());Reject([&]{j.Begin("more_work");});++failures;
    }
    std::ofstream(root/"not-a-directory")<<"fixture";
    Reject([&]{d::Journal j(root/"not-a-directory"/"child","fixture",sha);});++failures;
    {d::Operation unbound("normal engine has no journal");}
    assert(fs::current_path()==cwd);assert(failures==9);
    std::printf("DIAGNOSTIC_JOURNAL PASS durable_begin nested_failure result_error concurrent_records=200 unicode unique_runs sync_fault_cases=%u batch_fault_cases=3 batch_deadline capacity urgent controlled_stop teardown cleanup CPU_FILESYSTEM_ONLY NO_GPU\n",failures);
}
