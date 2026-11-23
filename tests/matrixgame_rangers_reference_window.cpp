// SPDX-License-Identifier: GPL-2.0-or-later
// Windows reference only: the compatibility token borrows the real test HWND
// through SDL. No native handle is truncated or fabricated.
#include "win32_compat_window.hpp"
#include <SDL2/SDL.h>
#include <cstdint>
#include <stdexcept>

namespace {
SDL_Window* window{};
std::uint32_t token{};
bool owns_video{};
}
std::uint32_t AttachRangersReferenceWindow(void* hwnd){
 if(!hwnd || window || token)throw std::logic_error("reference window already attached or missing");
 owns_video=(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO)==0;
 if(owns_video && SDL_InitSubSystem(SDL_INIT_VIDEO)!=0)throw std::runtime_error(SDL_GetError());
 window=SDL_CreateWindowFrom(hwnd);
 if(!window){if(owns_video)SDL_QuitSubSystem(SDL_INIT_VIDEO);owns_video=false;throw std::runtime_error(SDL_GetError());}
 token=srhd_awa::platform::win32_compat::RegisterMainWindow(window,1024,768);
 if(!token){SDL_DestroyWindow(window);window=nullptr;if(owns_video)SDL_QuitSubSystem(SDL_INIT_VIDEO);owns_video=false;throw std::runtime_error("reference window registration failed");}
 return token;
}
void DetachRangersReferenceWindow(){
 if(!window || !token)throw std::logic_error("reference window not attached");
 srhd_awa::platform::win32_compat::UnregisterMainWindow(token);token=0;
 SDL_DestroyWindow(window);window=nullptr;
 if(owns_video)SDL_QuitSubSystem(SDL_INIT_VIDEO);owns_video=false;
}
