"""Prove effective compiler optimization and retained safety flags from CMake."""
import json,re
def optimized_compiler_provenance(build):
    commands=json.loads((build/'compile_commands.json').read_text(encoding='utf-8'))
    checked=[]
    for entry in commands:
        command=entry['command'];flags=re.findall(r'(?:^|\s)(-O\S+)',command)
        assert flags and flags[-1]=='-O2',(entry['file'],flags)
        assert '-UNDEBUG' in command and '-DNDEBUG' not in command,entry['file']
        assert not any(flag in command for flag in ('-ffast-math','-Ofast','-DASSERT_OFF','-fno-exceptions')),entry['file']
        checked.append(entry['file'])
    assert len(checked)>100
    for file in ('platform_switch.cpp','fixed_vertex.cpp','native_device_state.cpp','OriginalNativeLoop.cpp','material_program.cpp','matrixgame_native_diagnostic_main.cpp'):
        assert any(p.replace('\\','/').endswith('/'+file) for p in checked),file
    return {'configured_translation_units':len(checked),'optimization':'-O2','assertions':'-UNDEBUG; no -DNDEBUG or -DASSERT_OFF','fast_math':False,'exceptions':True,'prebuilt_rangers_archive':'unchanged; not rebuilt or claimed optimized'}
