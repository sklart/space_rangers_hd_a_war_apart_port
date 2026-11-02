// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_host_callbacks.hpp"
#include "runtime_support.hpp"
#include "units/Robot.hpp"
namespace srhd_awa::matrixgame {
namespace {
void PAS_STDCALL RenderHostText(char16_t* text,char16_t* font,std::uint32_t color,
    std::int32_t width,std::int32_t height,std::int32_t align_x,std::int32_t align_y,
    std::int32_t wrap,std::int32_t offset_x,std::int32_t offset_y,Types::PRect clip,Robot::PRobotTextImage image){
    // The native request permits no clip. Original RobotRenderText requires a
    // rectangle and expands right/bottom itself when width/height are measured.
    Types::TRect full{0,0,width,height};
    Robot::RobotRenderText(text,font,color,width,height,align_x,align_y,wrap,offset_x,offset_y,clip?clip:&full,image);
}
}
Robot::TRobotCallbacks MakeRangersHostCallbacks(){
    Robot::TRobotCallbacks callbacks{};
    callbacks.PlaySound=Robot::TRobotPlaySound(Robot::RobotPlaySound);
    callbacks.CreateSound=Robot::TRobotCreateSound(Robot::RobotCreateSound);
    callbacks.FreeSound=Robot::TRobotSoundAction(Robot::RobotFreeSound);
    callbacks.StartSound=Robot::TRobotSoundAction(Robot::RobotStartSound);
    callbacks.IsSoundPlaying=Robot::TRobotSoundQuery(Robot::RobotIsSoundPlaying);
    callbacks.SetSoundVolume=Robot::TRobotSoundSetValue(Robot::RobotSetSoundVolume);
    callbacks.SetSoundPan=Robot::TRobotSoundSetValue(Robot::RobotSetSoundPan);
    callbacks.GetSoundVolume=Robot::TRobotSoundGetValue(Robot::RobotGetSoundVolume);
    callbacks.GetSoundPan=Robot::TRobotSoundGetValue(Robot::RobotGetSoundPan);
    callbacks.RenderText=Robot::TRobotRenderText(RenderHostText);
    callbacks.FreeText=Robot::TRobotFreeText(Robot::RobotFreeText);
    callbacks.SetProgress=Robot::TRobotProgress(Robot::RobotSetProgress);
    callbacks.PlayMusic=Robot::TRobotAction(Robot::RobotPlayMusic);
    callbacks.ReleaseTextures=Robot::TRobotAction(Robot::RobotReleaseTextures);
    callbacks.GetMusicVolume=Robot::TRobotGetVolume(Robot::RobotGetMusicVolume);
    callbacks.SetMusicVolume=Robot::TRobotSetVolume(Robot::RobotSetMusicVolume);
    return callbacks;
}
}
