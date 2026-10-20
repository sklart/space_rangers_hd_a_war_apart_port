// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone Switch launcher for the existing real VO/deko readback probe.
#define main MatrixGameDekoProbeMain
#include "probe_matrixgame_deko_link.cpp"
#undef main
#include <switch.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MATRIXGAME_SMOKE_GIT
#define MATRIXGAME_SMOKE_GIT "unknown"
#endif

int main() {
  consoleInit(nullptr);
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  PadState pad;
  padInitializeDefault(&pad);
  const char* root="sdmc:/switch/matrixgame-deko-smoke";
  const char* log_path="sdmc:/switch/matrixgame-deko-smoke/gpu-smoke.log";
  const char* model_path="sdmc:/switch/matrixgame-deko-smoke/model.vo";
  mkdir(root,0777);
  std::printf("MatrixGame GPU smoke (%s)\nReal VO, offscreen draw/readback; no battle.\n",MATRIXGAME_SMOKE_GIT);
  std::printf("Running once. Log: %s\n",log_path);
  consoleUpdate(nullptr);
  int result=2;
  FILE* log=std::fopen(log_path,"a");
  const int saved_out=dup(STDOUT_FILENO), saved_err=dup(STDERR_FILENO);
  if(log && saved_out>=0 && saved_err>=0) {
    std::fflush(stdout);std::fflush(stderr);
    const bool redirected=dup2(fileno(log),STDOUT_FILENO)>=0 && dup2(fileno(log),STDERR_FILENO)>=0;
    if(redirected) {
      std::printf("BEGIN MatrixGame GPU smoke build_git=%s model=%s battle_executed=false\n",MATRIXGAME_SMOKE_GIT,model_path);
      std::fflush(stdout);
      char option[]="--geometry-buffers";
      char* arguments[]={const_cast<char*>("matrixgame-gpu-smoke"),option,const_cast<char*>(model_path),nullptr};
      result=MatrixGameDekoProbeMain(3,arguments);
      std::printf("END MatrixGame GPU smoke status=%s code=%d battle_executed=false\n",result==0?"PASS":"FAIL",result);
      std::fflush(stdout);std::fflush(stderr);
    }
    dup2(saved_out,STDOUT_FILENO);dup2(saved_err,STDERR_FILENO);
  }
  if(saved_out>=0)close(saved_out);
  if(saved_err>=0)close(saved_err);
  if(log)std::fclose(log);
  std::printf("\n%s (code %d). B exits.\nSave gpu-smoke.log for analysis.\n",result==0?"GPU SMOKE PASS":"GPU SMOKE FAILED",result);
  while(appletMainLoop()) {
    padUpdate(&pad);
    if(padGetButtonsDown(&pad)&HidNpadButton_B)break;
    consoleUpdate(nullptr);
  }
  consoleExit(nullptr);
  return result;
}
