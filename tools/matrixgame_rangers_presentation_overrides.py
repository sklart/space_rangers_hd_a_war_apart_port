"""Translate only the original Windows SDK Reset boundary; preserve Win32 records."""
def render_inline(original):
 before='''        using CppMethod = std::int32_t (PAS_STDCALL *)(void*, void*);
        return pas::com_method<CppMethod>(Self.get(), 16)(Self.get(), Parameters);'''
 assert original.count(before)==1
 after='''#if defined(_WIN32) && defined(_WIN64)
        static_assert(sizeof(TD3DPresentParameters)==sizeof(srhd_awa::matrixgame::RangersLegacyPresentation));
        static_assert(offsetof(TD3DPresentParameters,DeviceWindow)==offsetof(srhd_awa::matrixgame::RangersLegacyPresentation,DeviceWindow));
        return srhd_awa::matrixgame::ResetRangersLegacyPresentation(Self.get(),Parameters);
#else
'''+before+'''
#endif'''
 include='#include "units/Direct3D9.hpp"'
 assert original.count(include)==1
 return original.replace(include,include+'\n#if defined(_WIN32) && defined(_WIN64)\n#include "rangers_legacy_presentation.hpp"\n#endif',1).replace(before,after,1)
