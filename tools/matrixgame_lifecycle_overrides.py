"""Preserve original CPU base/form lifecycle without the Windows release tracer include."""
import re

INPUTS = {
    'CMain.cpp':'442f6b5a6218c6d2c3423dd80d133e80ff6186471642492e6951fddd2a094a9f',
    'Form.cpp':'a2fec34d9ed1d24e4014ed93f333148477ca3e5fd80acff6d84fa79918782195',
    'Form.hpp':'e55f462a1e28261837f3194c08ba40f163db7f7493b94d8a81a27038b2220663',
}
SUBDIRS = {'Form.cpp':'../3G','Form.hpp':'../3G'}

def patch_lifecycle(files):
    for name in ('CMain.cpp','Form.cpp'):
        text=files[name].decode()
        if text.count('#include "Tracer.hpp"')!=1:
            raise ValueError('Pinned lifecycle trace include changed: '+name)
        text=text.replace('#include "Tracer.hpp"','''#if defined(_DEBUG) || defined(_TRACE) || defined(MEM_SPY_ENABLE)
#error Native lifecycle requires the original release path; Windows debug tracer is not ported.
#endif''')
        # Original DTRACE is empty in release. Keep all lifecycle bodies intact.
        text=re.sub(r'\bDTRACE\(\);','',text)
        files[name]=text.encode()
