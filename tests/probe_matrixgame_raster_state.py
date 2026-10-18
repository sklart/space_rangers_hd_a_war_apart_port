"""Register translation and actual optional D3D9 NULLREF Set/Get; not rasterization."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCES={
 'MatrixGame/src/MatrixSkinManager.cpp':'d9d565af29522397011903bf6e3d730575092d0eb8ad540506740ff8f03f4778',
 'MatrixGame/src/MatrixMap.cpp':'dcdc7f878c186741897f158894de353e4bec190b64dd6236857179b665ce1696',
 'MatrixLib/3G/VectorObject.cpp':'11ffb4eb842b5cdf6041d0ddc3bf25a124b06bdd8ac36f090b4c2ac6d9586303',
}
REGS={'ZENABLE':7,'ZWRITEENABLE':14,'ALPHATESTENABLE':15,'SRCBLEND':19,'DESTBLEND':20,
      'ZFUNC':23,'ALPHAREF':24,'ALPHAFUNC':25,'ALPHABLENDENABLE':27,'COLORWRITEENABLE':168,'BLENDOP':171}
CONSTANTS={'TRUE':1,'FALSE':0,'D3DZB_TRUE':1,'D3DZB_FALSE':0}
CONSTANTS.update({'D3DCMP_'+n:i+1 for i,n in enumerate(('NEVER','LESS','EQUAL','LESSEQUAL','GREATER','NOTEQUAL','GREATEREQUAL','ALWAYS'))})
CONSTANTS.update({'D3DBLEND_'+n:i+1 for i,n in enumerate(('ZERO','ONE','SRCCOLOR','INVSRCCOLOR','SRCALPHA','INVSRCALPHA','DESTALPHA','INVDESTALPHA','DESTCOLOR','INVDESTCOLOR','SRCALPHASAT'))})
CONSTANTS.update({'D3DBLENDOP_'+n:i+1 for i,n in enumerate(('ADD','SUBTRACT','REVSUBTRACT','MIN','MAX'))})


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--reference-binary',type=Path);p.add_argument('--runner-binary',type=Path);p.add_argument('--qemu-sysroot',type=Path);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);operations=[];inventory={}
    for name,wanted in SOURCES.items():
        data=(ROOT/'upstream/matrixgame'/name).read_bytes().replace(b'\r\n',b'\n');assert hashlib.sha256(data).hexdigest()==wanted
        text=re.sub(r'/\*.*?\*/|//[^\n]*','',data.decode(),flags=re.S);count=0
        for reg,value in re.findall(r'SetRenderState\(\s*D3DRS_(\w+)\s*,\s*([^,)]+)\)',text):
            value=value.strip()
            if reg not in REGS:continue
            if value in CONSTANTS:n=CONSTANTS[value]
            elif re.fullmatch(r'0[xX][0-9a-fA-F]+|[0-9]+',value):n=int(value,0)
            else:continue
            operations.append((REGS[reg],n));count+=1
        inventory[name]=count
    # Each extracted constant is an input fixture, not an executed original branch.
    choices={7:(0,1),14:(0,1),15:(0,1),19:range(1,12),20:range(1,12),23:range(1,9),
             24:(0,7,8,9,191,192,193,255),25:range(1,9),27:(0,1),168:range(16),171:range(1,6)}
    for reg,values in choices.items():operations.extend((reg,value) for value in values)
    seed=0x71643891;keys=list(choices)
    for _ in range(2000):
        seed=(seed*1664525+1013904223)&0xffffffff;reg=keys[seed%len(keys)]
        seed=(seed*1664525+1013904223)&0xffffffff;values=choices[reg];operations.append((reg,values[seed%len(values)]))
    order=list(REGS.values());current=[1,1,0,5,6,4,8,7,0,15,1];expected=bytearray(struct.pack('<11I',*current))
    for reg,value in operations:current[order.index(reg)]=value;expected+=struct.pack('<11I',*current)
    manifest=out/'registers.txt';manifest.write_text(''.join(f'{r} {v}\n' for r,v in operations))
    command=[str(a.binary.resolve())]
    if a.runner_binary:command+=['-L',str(a.qemu_sysroot.resolve()),str(a.runner_binary.resolve())]
    def execute(command,label):
        result=subprocess.run(command+[str(manifest)],capture_output=True,timeout=90)
        (out/(label+'-stderr.log')).write_bytes(result.stderr)
        assert result.returncode==0,(label,result.returncode,result.stderr[-2000:])
        assert result.stdout==expected,(label,len(result.stdout),len(expected),next((i for i,(x,y) in enumerate(zip(result.stdout,expected)) if x!=y),None))
    execute(command,'native')
    if a.reference_binary:execute([str(a.reference_binary.resolve())],'d3d9')
    result=dict(scope='Raster register state; no GPU image or battle',operations=len(operations),source_constants=inventory,
        transcript_bytes=len(expected),sha256=hashlib.sha256(expected).hexdigest(),d3d9_nullref_match=bool(a.reference_binary))
    (out/'oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
