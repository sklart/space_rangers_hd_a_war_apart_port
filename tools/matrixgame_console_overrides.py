"""Preserve full original developer-console logic with typed key/time inputs."""
import hashlib
import re

KEYS={'BACK':'Back','DELETE':'Delete','LEFT':'Left','RIGHT':'Right','HOME':'Home',
      'END':'End','RETURN':'Return','ESCAPE':'Escape','SHIFT':'Shift'}


def prepare_console(output):
    name='MatrixGame/src/DevConsole.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    if text.count('timeGetTime()')!=2:raise ValueError('Original console benchmark changed')
    text=text.replace('timeGetTime()','srhd_awa::matrixgame::console::BenchmarkMilliseconds()')
    for old,new in KEYS.items():
        pattern=r'\bVK_'+old+r'\b'
        if len(re.findall(pattern,text))!=(2 if old=='SHIFT' else 1):
            raise ValueError('Original console key boundary changed: '+old)
        text=re.sub(pattern,'srhd_awa::matrixgame::console::'+new,text)
    path.write_bytes(('#include "console_backend.hpp"\n'+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
