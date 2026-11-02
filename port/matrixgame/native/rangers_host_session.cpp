// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "rangers_host_callbacks.hpp"
#include "rangers_audio_session.hpp"
#include "rangers_music_session.hpp"
#include "runtime_support.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_DX.hpp"
#include "units/GlobalsV.hpp"
#include "units/Robot.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
struct HostSettings {
    std::uint8_t hardware=GlobalsV::HardwareRenderingEnabled,sound=GlobalsV::SoundEnabled,music=GlobalsV::MusicEnabled;
    std::uint8_t robot_sound=Robot::RobotSound,robot_music=Robot::RobotMusic;
    float music_scale=GlobalsV::MusicVolumeScale;
    explicit HostSettings(const RangersHostOptions& options){
        if(EngineCallbacks::Bound() || GR_Main::Direct3DDevice!=nullptr || GR_DX::TextureCaches || GR_Main::GlobalCache || GR_Main::SoundManager || GR_Main::MusicManager)
            throw std::logic_error("Rangers host requires an unbound standalone context");
        GlobalsV::HardwareRenderingEnabled=false;
        GlobalsV::SoundEnabled=options.sound;GlobalsV::MusicEnabled=options.music;
        Robot::RobotSound=options.sound;Robot::RobotMusic=options.music;
    }
    ~HostSettings(){
        GlobalsV::HardwareRenderingEnabled=hardware;GlobalsV::SoundEnabled=sound;GlobalsV::MusicEnabled=music;
        Robot::RobotSound=robot_sound;Robot::RobotMusic=robot_music;GlobalsV::MusicVolumeScale=music_scale;
    }
};
}
struct RangersHostSession::Impl {
    // Members unwind in the reverse dependency order on construction failure too.
    HostSettings settings;
    RangersDataSession data;
    RangersAudioSession audio;
    std::unique_ptr<RangersMusicSession> music;
    std::unique_ptr<EngineCallbacks> callbacks;
    explicit Impl(const RangersHostOptions& options,bool bind_callbacks=true):settings(options),data(options.data),audio(data),
        music(options.music?std::make_unique<RangersMusicSession>(data):nullptr),
        callbacks(bind_callbacks?std::make_unique<EngineCallbacks>(MakeRangersHostCallbacks()):nullptr){}
};
RangersHostSession::RangersHostSession(const RangersHostOptions& options):impl_(std::make_unique<Impl>(options)){}
RangersHostSession::RangersHostSession(const RangersHostOptions& options,RangersDeferredCallbacksTag):impl_(std::make_unique<Impl>(options,false)){}
RangersHostSession::~RangersHostSession()=default;
const SMGDRangersInterface& RangersHostSession::Table()const noexcept{return *EngineCallbacks::Table();}
}
