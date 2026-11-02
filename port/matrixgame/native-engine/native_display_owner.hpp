// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace srhd_awa::matrixgame {
// Acquire each SDK resource once and unwind only completed acquisitions.
// Backend operations throw on SDK failure; releases never throw.
template<class Backend> class NativeDisplayOwner {
    Backend& backend_;
    bool service_{},display_{},layer_{},window_{};
    void Release()noexcept{
        if(window_){backend_.CloseWindow();window_=false;}
        if(layer_){backend_.CloseLayer();layer_=false;}
        if(display_){backend_.CloseDisplay();display_=false;}
        if(service_){backend_.ExitService();service_=false;}
    }
public:
    explicit NativeDisplayOwner(Backend& backend):backend_(backend){
        try{
            backend_.InitializeService();service_=true;
            backend_.OpenDisplay();display_=true;
            backend_.CreateLayer();layer_=true;
            backend_.ConfigureLayer();
            backend_.CreateWindow();window_=true;
            backend_.ConfigureWindow();
        }catch(...){Release();throw;}
    }
    ~NativeDisplayOwner(){Release();}
    NativeDisplayOwner(const NativeDisplayOwner&)=delete;
    NativeDisplayOwner& operator=(const NativeDisplayOwner&)=delete;
};
}
