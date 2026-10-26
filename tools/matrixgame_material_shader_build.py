"""Compile native material programs and embed validated DKSH; no packaging."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--sdk',type=Path,default=Path('C:/devkitPro'));args=parser.parse_args()
    out=args.output;out.mkdir(parents=True,exist_ok=True);entries={};header='#pragma once\n#include <array>\n#include <cstdint>\nnamespace srhd_awa::matrixgame::materials::compiled {\n'
    for stage in ('vert','frag'):
        source=ROOT/f'port/matrixgame/deko/material.{stage}.glsl';target=out/f'material.{stage}.dksh';uam=args.sdk/'tools/bin/uam'
        if not uam.exists():uam=uam.with_suffix('.exe')
        command=list(map(str,[uam,'-s',stage,'-o',target,source]));p=subprocess.run(command,capture_output=True,text=True)
        if p.returncode:raise RuntimeError(p.stdout+p.stderr)
        data=target.read_bytes();magic,head,control,code,program,count=struct.unpack_from('<6I',data)
        assert magic==0x48534b44 and 24<=head<=program<control and control+code==len(data) and code and count==1
        entries[stage]={'command':command,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'dksh_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
        header+=f'inline constexpr std::array<std::uint8_t,{len(data)}> {stage}'+'{'+','.join(map(str,data))+'};\n'
    (out/'material_shaders.hpp').write_text(header+'}\n',encoding='utf-8')
    (out/'shader-build.json').write_text(json.dumps(entries,indent=2)+'\n')
if __name__=='__main__':main()
