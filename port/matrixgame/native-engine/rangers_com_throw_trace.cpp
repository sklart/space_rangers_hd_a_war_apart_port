// Diagnostic-only hook for the original COM interface boundary. The exception
// is still thrown normally; do not convert errors or change game behaviour.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <typeinfo>
#include <unwind.h>

extern "C" int main();
extern "C" [[noreturn]] void __real___cxa_throw(void*, void*, void (*)(void*));

namespace {
struct Trace { unsigned count{}; };
_Unwind_Reason_Code RecordFrame(_Unwind_Context* context, void* arg) {
    auto& trace = *static_cast<Trace*>(arg);
    const auto address = _Unwind_GetIP(context);
    if (address && trace.count++ < 32)
        std::fprintf(stderr, " %llx", static_cast<unsigned long long>(address));
    return trace.count >= 32 ? _URC_END_OF_STACK : _URC_NO_REASON;
}
}

extern "C" [[noreturn]] void __wrap___cxa_throw(void* object, void* type, void (*destroy)(void*)) {
    static thread_local bool tracing = false;
    const auto* info=static_cast<std::type_info*>(type);
    const bool com=object && info && *info==typeid(std::runtime_error) &&
        std::strcmp(static_cast<std::runtime_error*>(object)->what(), "COM call through nil interface") == 0;
    const bool frame=object && info && *info==typeid(std::logic_error) &&
        std::strcmp(static_cast<std::logic_error*>(object)->what(), "native frame already acquired") == 0;
    if (!tracing && (com || frame)) {
        tracing = true;
        std::fprintf(stderr, "%s main=%llx frames:", com ? "COM_NIL_TRACE" : "NATIVE_FRAME_TRACE",
                     static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&main)));
        Trace trace;
        _Unwind_Backtrace(RecordFrame, &trace);
        std::fputc('\n', stderr);
        std::fflush(stderr);
        tracing = false;
    }
    __real___cxa_throw(object, type, destroy);
}
