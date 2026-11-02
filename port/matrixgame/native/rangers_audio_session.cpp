// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_audio_session.hpp"
#include "rangers_data_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_Cache.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Sound.hpp"
#include "units/GlobalsV.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
struct RangersAudioSession::Impl {
    EC_Data::TDataEC& root;
    EC_Cache::TCacheEC* cache{};
    GR_Sound::TSoundControl* sound{};
    bool borrowed_root{};
    std::uint8_t sound_enabled=GlobalsV::SoundEnabled,music_enabled=GlobalsV::MusicEnabled;
    explicit Impl(EC_Data::TDataEC& value,bool borrowed=false):root(value),borrowed_root(borrowed){
        if(GR_Main::GlobalCache || GR_Main::SoundManager || GR_Main::MusicManager ||
           (borrowed_root?GR_Main::CacheDataRoot!=&root:GR_Main::CacheDataRoot!=nullptr))
            throw std::logic_error("Rangers audio host globals already owned");
        if(GlobalsV::HardwareRenderingEnabled)
            throw std::logic_error("audio-only Rangers host cannot own renderer cache eviction");
        // The shared standalone context also supports the original fully-muted
        // mode: a real TSoundControl exists, without opening an audio device.
        if(!sound_enabled && !music_enabled && !borrowed_root)throw std::invalid_argument("Rangers audio host requires enabled sound or music");
        GR_Main::CacheDataRoot=&root;
        try{
            cache=pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);
            GR_Main::GlobalCache=cache;cache->SetDataRoot(&root);
            sound=pas::construct_call<GR_Sound::TSoundControl>(GR_Sound::TSoundControl_Create);
            GR_Main::SoundManager=sound;
            // The original backend may return after disabling sound on failure.
            // This owner must never publish that as a usable audio service.
            if((sound_enabled && !GlobalsV::SoundEnabled) || (music_enabled && !GlobalsV::MusicEnabled))
                throw std::runtime_error("real Rangers sound backend did not initialize");
        }catch(...){Clear();throw;}
    }
    void Clear()noexcept{
        if(sound){pas::free(sound);sound=nullptr;GR_Main::SoundManager=nullptr;}
        if(cache){pas::free(cache);cache=nullptr;GR_Main::GlobalCache=nullptr;}
        if(!borrowed_root)GR_Main::CacheDataRoot=nullptr;
        GlobalsV::SoundEnabled=sound_enabled;GlobalsV::MusicEnabled=music_enabled;
    }
    ~Impl(){
        if(GR_Main::SoundManager!=sound || GR_Main::GlobalCache!=cache || GR_Main::CacheDataRoot!=&root || GR_Main::MusicManager)std::terminate();
        Clear();
    }
};
RangersAudioSession::RangersAudioSession(EC_Data::TDataEC& root):impl_(std::make_unique<Impl>(root)){}
RangersAudioSession::RangersAudioSession(RangersDataSession& data):impl_(std::make_unique<Impl>(data.CacheData(),true)){}
RangersAudioSession::~RangersAudioSession()=default;
}
