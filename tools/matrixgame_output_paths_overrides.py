"""Original output-path construction with explicit native Shell ownership."""
import hashlib

def prepare_output_paths(output):
    name='MatrixGame/src/MatrixGamePathUtils.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    text=text.replace('#include "windows.h"','#include "output_paths_backend.hpp"\nnamespace output_native=srhd_awa::matrixgame::output_paths;')
    text=text.replace('#include <shlobj.h>','')
    replacements={
        'ITEMIDLIST *pidl;':'output_native::FolderItem pidl;',
        'HRESULT hRes = SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl);':'auto hRes = output_native::PersonalLocation(pidl);',
        'hRes == NOERROR':'hRes == 0',
        'wchar_t lpPath[MAX_PATH];':'wchar_t lpPath[260];',
        'SHGetPathFromIDListW(pidl, lpPath);':'output_native::PersonalPath(pidl, lpPath, 260);',
        'CreateDirectoryW(path.c_str(), NULL);':'output_native::CreateFolder(path.c_str());',
    }
    for old,new in replacements.items():
        if text.count(old)!=1:raise ValueError('Original output path boundary changed: '+old)
        text=text.replace(old,new,1)
    path.write_bytes(text.encode());return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
