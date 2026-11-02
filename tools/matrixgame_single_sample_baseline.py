"""Recover the exact pre-fix source bytes, verified against the saved build."""
import json
import hashlib
from pathlib import Path
from matrixgame_single_sample_checks import ROOT,BASE


def main():
    report=json.loads((BASE/'baseline/result.json').read_text())
    cpp=ROOT/'port/matrixgame/native-engine/native_device_state.cpp'
    text=cpp.read_text().replace('    if(id==60)combiner.Factor(value);',
        '    if(id==161){if(value)throw Unsupported("native MSAA requires multisample targets/resolve pipeline");}\n    else if(id==60)combiner.Factor(value);')
    text=text.replace('void NativeRenderState::ValidateDraw(bool points,bool lines,std::uint32_t samples,std::uint32_t quality)const{\n'
        '    // D3D9\'s enable and write-mask registers have no raster effect on a\n'
        '    // single-sample target, but remain observable/restorable logical state.\n'
        '    // A true multisample attachment still needs a separate pipeline/resolve.\n'
        '    if(samples || quality)throw Unsupported("native multisample target/resolve pipeline unavailable");',
        'void NativeRenderState::ValidateDraw(bool points,bool lines)const{')
    text=text.replace('        if(Is(n,{"SLOPESCALEDEPTHBIAS","DEPTHBIAS"}))',
        '        if(n=="MULTISAMPLEMASK")require(n,v==0xffffffffu);\n        if(Is(n,{"SLOPESCALEDEPTHBIAS","DEPTHBIAS"}))')
    header=ROOT/'port/matrixgame/native-engine/native_device_state.hpp'
    old_header=header.read_text().replace('void ValidateDraw(bool points=false,bool lines=false,std::uint32_t samples=0,std::uint32_t quality=0)const;',
        'void ValidateDraw(bool points=false,bool lines=false)const;')
    output=BASE/'baseline/source';output.mkdir(parents=True,exist_ok=True)
    for source,content in ((cpp,text),(header,old_header)):
        data=content.encode();expected=report['source_sha256'][source.relative_to(ROOT).as_posix()]
        assert hashlib.sha256(data).hexdigest()==expected
        target=output/source.name
        if target.exists():assert target.read_bytes()==data
        else:
            with target.open('xb') as stream:stream.write(data)
        print(target.name,expected)


if __name__=='__main__':main()
