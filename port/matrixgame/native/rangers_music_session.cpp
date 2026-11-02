// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_music_session.hpp"
#include "rangers_data_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Music.hpp"
#include "units/GlobalsV.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
struct RangersMusicSession::Impl {
    EC_BlockPar::TBlockParEC& config;
    GR_Music::TMusicControl* music{};
    bool borrowed_config{};
    GR_Sound::TSoundControl* sound=GR_Main::SoundManager;
    EC_Cache::TCacheEC* cache=GR_Main::GlobalCache;
    EC_Data::TDataEC* data=GR_Main::CacheDataRoot;
    Impl(EC_BlockPar::TBlockParEC& value,EC_BlockPar::TBlockParEC& language,bool borrowed=false):config(value),borrowed_config(borrowed){
        if(GR_Main::MusicManager || (borrowed_config?GR_Main::MainDataConfig!=&config:GR_Main::MainDataConfig!=nullptr))
            throw std::logic_error("Rangers music globals already owned");
        if(borrowed_config){
            if(GR_Main::LanguageDataConfig!=&language || !GR_Main::WideCaseTable.length())throw std::logic_error("Rangers data session context missing");
        }else if(GR_Main::WideCaseTable.length())throw std::logic_error("Rangers case table already owned");
        if(!sound || !cache || !data)throw std::logic_error("Rangers music requires live sound/cache services");
        if(!GlobalsV::MusicEnabled)throw std::invalid_argument("Rangers music requires enabled music");
        auto* category=config.GetBlockByPath(u"Music.Robot"_w);
        bool present=false;for(int i=0;i<category->GetParamCount();++i)if(category->GetParamValue(i)!=u"")present=true;
        if(!present)throw std::invalid_argument("Rangers music Robot playlist empty");
        auto* cases=language.GetBlock(u"CaseConv"sv);
        const auto count=cases->GetParamCount();
        if(count<=0)throw std::invalid_argument("Rangers language case table empty");
        for(int i=0;i<count;++i)
            if(cases->GetParamName(i).length()!=1 || cases->GetParamValue(i).length()!=1)
                throw std::invalid_argument("Rangers language case pair invalid");
        GR_Main::MainDataConfig=&config;
        try{
            // Same configuration-driven initialization as original GR_Main.
            if(!borrowed_config){
                GR_Main::WideCaseTable.set_length(count);
                for(int i=0;i<count;++i){
                    GR_Main::WideCaseTable[i].LowerChar=cases->GetParamName(i).read(1);
                    GR_Main::WideCaseTable[i].UpperChar=cases->GetParamValue(i).read(1);
                }
            }
            music=pas::construct_call<GR_Music::TMusicControl>(GR_Music::TMusicControl_Create);
            GR_Main::MusicManager=music;
            if(!music->Current || !music->Queued || !music->CompletionEvent)
                throw std::runtime_error("original Rangers music workers did not initialize");
        }catch(...){Clear();throw;}
    }
    void Clear()noexcept{
        // Original destructor signals stop, waits for control/current/queued
        // workers and releases their buffers while SoundManager still exists.
        if(music){pas::free(music);music=nullptr;GR_Main::MusicManager=nullptr;}
        if(!borrowed_config){GR_Main::WideCaseTable=nullptr;GR_Main::MainDataConfig=nullptr;}
    }
    ~Impl(){
        if(GR_Main::MusicManager!=music || GR_Main::MainDataConfig!=&config ||
           GR_Main::SoundManager!=sound || GR_Main::GlobalCache!=cache || GR_Main::CacheDataRoot!=data)std::terminate();
        Clear();
    }
};
RangersMusicSession::RangersMusicSession(EC_BlockPar::TBlockParEC& config,EC_BlockPar::TBlockParEC& language):impl_(std::make_unique<Impl>(config,language)){}
RangersMusicSession::RangersMusicSession(RangersDataSession& data):impl_(std::make_unique<Impl>(data.MainConfig(),data.LanguageConfig(),true)){}
RangersMusicSession::~RangersMusicSession()=default;
}
