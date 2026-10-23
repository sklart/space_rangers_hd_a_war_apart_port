// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "cache_resources.hpp"
#include "buffer_resources.hpp"
#include "platform_3g.hpp"
#include <memory>
#include <optional>
struct SRobotsSettings;
class CMatrixCursor;
class CLoadProgress;
class CTextureManaged;
namespace Base {class CBlockPar;}
extern Base::CHeap* g_MatrixHeap;
namespace srhd_awa::matrixgame {
struct Cpu3GState {void* module;void* window;bool bound;};
void L3GInitAsDLLCpuPhase(void* module,void* window);
void L3GDeinitCpuPhase() noexcept;
Cpu3GState InspectCpu3GState() noexcept;
// Single cache/buffer/platform owner. Device/window providers are borrowed and
// must outlive Deinit. This is a prerequisite of CGame::Init, not its implementation.
class NativeInitRuntime {
public:
    NativeInitRuntime();
    ~NativeInitRuntime();
    NativeInitRuntime(const NativeInitRuntime&)=delete;
    NativeInitRuntime& operator=(const NativeInitRuntime&)=delete;
    void Begin(void* module,void* window);
    void AttachDevice(platform3g::Window&,platform3g::Device&);
    void BindLegacyProviders(platform3g::Window&,platform3g::Device&);
    static void EnterLegacyDll(void* module,void* window,std::uintptr_t api,std::uintptr_t device);
    static void LeaveLegacyDll();
    void DetachDevice() noexcept;
    void ApplySamplerSettings(SRobotsSettings&);
    void ApplyDefaultDeviceState();
    void ApplyVideoSettings(SRobotsSettings&);
    void InitializeGameDefaults();
    void LoadGameData(const wchar_t*);
    void ReadGameParameters();
    void ApplyGameSettings(SRobotsSettings&);
    void InitializeLoadProgress();
    void FinishLoadProgress()noexcept;
    CLoadProgress* LoadProgress()const noexcept{return load_progress_.get();}
    void LoadOriginalManagedTexture(const wchar_t*,bool convert_to_16bit=false);
    void FinishOriginalManagedTexture()noexcept;
    CTextureManaged* OriginalManagedTexture()const noexcept{return original_texture_;}
    bool GameDefaultsReady()const noexcept{return CpuReady() && game_defaults_;}
    void InitializeCursor();
    CMatrixCursor* Cursor()const noexcept{return cursor_.get();} // Borrowed until DetachDevice/Deinit.
    bool DefaultDeviceStateReady()const noexcept {return GpuReady() && defaults_applied_;}
    std::optional<platform3g::DeviceResult> LastStateRestoreResult()const noexcept{return last_restore_;}
    void Deinit() noexcept;
    bool CpuReady() const noexcept {return cpu_owned_;}
    bool OriginalStaticsReady()const noexcept {return statics_owned_;}
    // Device negotiation completed; does not imply a game draw/map is supported.
    bool GpuReady() const noexcept {return cpu_owned_ && platform3g::Current().ready && bool(device_snapshot_);}
    static const char* FirstUnsupportedOperation() noexcept;
private:
    std::unique_ptr<Base::CHeap> matrix_heap_;
    Base::CBlockPar* matrix_data_{};
    std::unique_ptr<CMatrixCursor> cursor_;
    std::unique_ptr<CLoadProgress> load_progress_;
    CTextureManaged* original_texture_{}; // HNew/CCache::Destroy, not delete or a CPU cache cast.
    std::unique_ptr<buffers::Context> buffers_;
    std::unique_ptr<buffers::Scope> scope_;
    std::unique_ptr<platform3g::DeviceStateLease> device_snapshot_;
    platform3g::Window* legacy_window_{};
    platform3g::Device* legacy_device_{};
    std::optional<platform3g::DeviceResult> last_restore_;
    bool sampler_configured_{},defaults_applied_{};
    bool game_defaults_{};
    bool cache_owned_{},cpu_owned_{};
    bool statics_owned_{};
};
}
