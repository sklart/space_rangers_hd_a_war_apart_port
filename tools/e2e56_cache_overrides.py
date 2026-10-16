"""Exception-safe cache loads in the disposable source tree only."""


def apply(text: str) -> str:
    text = '#include "e2e5_failure_diagnostics.hpp"\n#include "e2e_clock.hpp"\n' + text
    for method, load in [
        ('AcquireDataFromConfig', '''
            EC_Buf::TBufEC* Buffer = nullptr;
            pas::ScopeExit release_buffer = [&]() noexcept { pas::free(Buffer); };
            const auto parts = EC_Str::CountDelimitedPartsW(pas::view(CacheKey), u"?"sv);
            const auto path = parts < 2 ? CacheKey : EC_Str::ExtractDelimitedPartW(pas::view(CacheKey), 0, u"?"sv);
            const auto option = parts < 2 ? pas::WideString() : EC_Str::ExtractDelimitedRangeW(pas::view(CacheKey), 1, parts - 1, u"?"sv);
            Buffer = GR_Main::GlobalCache->OpenDataBuffer(path);
            Data->LoadFromConfigBuffer(Buffer, option);'''),
        ('AcquireDataFromDirectKey', '            Data->LoadFromKey(CacheKey);'),
    ]:
        start = text.index(f'    TCacheDataEC* TCacheControlEC::{method}(')
        end = text.index('\n    }', start) + len('\n    }')
        body = '''    TCacheDataEC* TCacheControlEC::METHOD(TCacheDataClass CacheDataClass) {
        auto* cache = GR_Main::GlobalCache;
        if (GlobalsV::HardwareRenderingEnabled) GR_DX::EvictTextureCaches(false);
        pas::critical_enter(cache->CacheLock);
        bool locked = true;
        pas::ScopeExit unlock = [&]() noexcept {
            if (locked) pas::critical_leave(cache->CacheLock);
        };
        auto leave = [&] { locked = false; pas::critical_leave(cache->CacheLock); };
        auto enter = [&] { pas::critical_enter(cache->CacheLock); locked = true; };
        TCacheDataEC* Data = nullptr;
        for (;;) {
            // Do not bind a waiting control until the entry has loaded. The
            // producer can then remove a failed entry without dangling waiters.
            Data = BoundData ? BoundData : cache->FindDataByKeyAndClass(pas::view(CacheKey), CacheDataClass);
            if (Data && Data->LoadCompleteEvent != 0) {
                // Recheck under CacheLock; never wait on a handle which the
                // producer can close between releasing the lock and WaitOne.
                leave();
                SysUtilsImports::Sleep(1u);
                enter();
                continue;
            }
            if (Data) {
                if (!BoundData) { Data->AppendControl(this); BoundData = Data; }
                ++RetainCount;
                cache->TouchData(Data);
                break;
            }
            Data = CreateData();
            bool published = false;
            pas::ScopeExit discard_unpublished = [&]() noexcept {
                if (!published) {
                    if (Data->LoadCompleteEvent) WindowsImports::CloseHandle(Data->LoadCompleteEvent);
                    pas::free(Data);
                }
            };
            Data->CacheKey = CacheKey;
            Data->LoadCompleteEvent = WindowsImports::CreateEvent(nullptr, -1, 0, nullptr);
            if (!Data->LoadCompleteEvent) throw std::bad_alloc();
            Data->AppendControl(this);
            BoundData = Data;
            RetainCount = 1;
            cache->AddDataToLruHead(Data);
            published = true;
            leave();
            const auto started = srhd_awa::platform::e2e_clock::Microseconds();
            char key[160]{};
            const auto view = pas::view(CacheKey);
            for (std::size_t i = 0; i < view.size() && i + 1 < sizeof(key); ++i)
                key[i] = view[i] < 128 ? static_cast<char>(view[i]) : '?';
            char detail[256]{};
            std::snprintf(detail, sizeof(detail), "cache load begin key=%s", key);
            srhd_awa::platform::e2e5_failure_diagnostics::Write(detail);
            try {
LOAD
            } catch (...) {
                enter();
                WindowsSdk::SetEvent(Data->LoadCompleteEvent);
                WindowsImports::CloseHandle(Data->LoadCompleteEvent);
                Data->LoadCompleteEvent = 0;
                RetainCount = 0;
                cache->RemoveAndFreeData(Data);
                leave();
                std::snprintf(detail, sizeof(detail), "cache load exception key=%s", key);
                srhd_awa::platform::e2e5_failure_diagnostics::Write(detail);
                throw;
            }
            enter();
            cache->ResidentBytes += Data->ResidentBytes;
            WindowsSdk::SetEvent(Data->LoadCompleteEvent);
            WindowsImports::CloseHandle(Data->LoadCompleteEvent);
            Data->LoadCompleteEvent = 0;
            std::snprintf(detail, sizeof(detail), "cache load end key=%s elapsed_us=%llu bytes=%d", key,
                static_cast<unsigned long long>(srhd_awa::platform::e2e_clock::Microseconds() - started), Data->ResidentBytes);
            leave();
            srhd_awa::platform::e2e5_failure_diagnostics::Write(detail);
            enter();
            break;
        }
        leave();
        cache->TrimToBudget(cache->ResidentByteLimit);
        return Data;
    }'''.replace('METHOD', method).replace('LOAD', load)
        text = text[:start] + body + text[end:]
    # Retain counts and bindings are read by the loader/reclaimer under CacheLock.
    # Publish their releases under the same lock, including Reset's initial test.
    for method in ('Reset', 'Release'):
        start = text.index(f'    void TCacheControlEC::{method}() {{')
        end = text.index('\n    }', start) + len('\n    }')
        body = text[start:end]
        body = body.replace('            pas::critical_enter(GR_Main::GlobalCache->CacheLock);\n', '')
        body = body.replace('            pas::critical_leave(GR_Main::GlobalCache->CacheLock);\n', '')
        opening = body.index(' {') + 2
        body = body[:opening] + '''
        if (!GR_Main::GlobalCache) {
            RetainCount = 0;
            NULL_CLEANUP
            return;
        }
        pas::critical_enter(GR_Main::GlobalCache->CacheLock);
        pas::ScopeExit release_lock = [&]() noexcept {
            pas::critical_leave(GR_Main::GlobalCache->CacheLock);
        };'''.replace('NULL_CLEANUP', 'BoundData = nullptr; CacheKey = pas::WideString();' if method == 'Reset' else '') + body[opening:]
        text = text[:start] + body + text[end:]
    marker = '        DataRoot->ReadBufferByPath(Path, Buffer);'
    assert text.count(marker) == 1
    text = text.replace(marker, '''        try {
            DataRoot->ReadBufferByPath(Path, Buffer);
        } catch (...) {
            pas::free(Buffer);
            throw;
        }''', 1)
    return '#include "units/SysUtilsImports.hpp"\n' + text
