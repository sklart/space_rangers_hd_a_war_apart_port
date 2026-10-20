"""Copy pinned BigVB/BigIB bodies for a real Windows D3D9 resource API reference."""
import argparse
import hashlib
import json
from pathlib import Path
from matrixgame_big_buffer_overrides import INPUTS
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    out=parser.parse_args().output.resolve()
    if not out.is_relative_to(ROOT/'build'): raise ValueError('Use an isolated build output')
    if out.exists() and any(out.iterdir()) and not (out/'reference-source.json').is_file():
        raise ValueError('Refusing unmanaged source output')
    files={}
    for name,digest in INPUTS.items():
        raw=(ROOT/'upstream/matrixgame/MatrixLib/3G'/name).read_bytes().replace(b'\r\n',b'\n')
        if hashlib.sha256(raw).hexdigest()!=digest: raise ValueError('Pinned reference source changed: '+name)
        assert raw.count(b'#include "D3DControl.hpp"')==1
        files[name]=raw.replace(b'#include "D3DControl.hpp"',b'#include "matrixgame_d3d9_buffer_control.hpp"')
    out.mkdir(parents=True,exist_ok=True)
    for name,data in files.items(): (out/name).write_bytes(data)
    (out/'reference-source.json').write_text(json.dumps({'scope':'Original algorithm bodies; include-only adapter to real Windows D3D9',
        'source_commit':'218115a72fa9c5d43269f4ca84a8596a0bcb82f1','inputs_sha256':INPUTS,
        'outputs_sha256':{name:hashlib.sha256(data).hexdigest() for name,data in files.items()}},indent=2)+'\n')
if __name__=='__main__': main()
