// Same physical SDK window, wrapped with the actual SDL foreign-window API.
// Appended to the port-generated GR_Main TU to bind its private platform state.
#include "win32_compat_window.hpp"
std::uint32_t AttachRangersReferenceWindow(void* hwnd){
 if(g_e2e_platform.native_window)throw std::logic_error("reference SDL window already bound");
 if(SDL_InitSubSystem(SDL_INIT_VIDEO)!=0)throw std::runtime_error(SDL_GetError());
 auto* window=SDL_CreateWindowFrom(hwnd);
 if(!window){SDL_QuitSubSystem(SDL_INIT_VIDEO);throw std::runtime_error(SDL_GetError());}
 const auto token=srhd_awa::platform::win32_compat::RegisterMainWindow(window,800,600);
 if(!token){SDL_DestroyWindow(window);SDL_QuitSubSystem(SDL_INIT_VIDEO);throw std::runtime_error("reference SDL window registration failed");}
 g_e2e_platform.native_window=window;g_e2e_platform.window_token=token;return token;
}
void DetachRangersReferenceWindow(){
 srhd_awa::platform::win32_compat::UnregisterMainWindow(g_e2e_platform.window_token);
 SDL_DestroyWindow(static_cast<SDL_Window*>(g_e2e_platform.native_window));g_e2e_platform.native_window=nullptr;g_e2e_platform.window_token=0;
 SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
