"""Compare real x86/native/ARM64 serialization, independently attributing Load normalization."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

def cannon_normalization(raw):
    """Locate v27 place movement bytes using the original on-disk layout, not C++ sizeof."""
    position = 0
    def take(size):
        nonlocal position
        start=position; position+=size
        assert size>=0 and position<=len(raw), 'Truncated v27 graph'
        return start
    def u8(): return raw[take(1)]
    def u32(): return struct.unpack_from('<I',raw,take(4))[0]
    assert u32()==27
    zones=u32()
    for _ in range(zones):
        take(8+1+4+4+16+1+1)
        take(u8()*9)  # near-zone index, connection size, movement mask
        take(4)       # region
        take(u8()*4)  # place indices
    crotches=u32();take(crotches*17)
    roads=u32()
    for _ in range(roads):
        take(8); take(u32()*4); take(4+1)
    places=u32();expected=bytearray(raw);cannons=0;changed=[]
    for _ in range(places):
        take(8);move=take(1);take(5+5+9)
        if u8():
            cannons+=1
            if expected[move]!=31: changed.append(move)
            expected[move]=31
        take(u8()*5)
    return bytes(expected),dict(zones=zones,crotches=crotches,roads=roads,places=places,
                               cannons=cannons,changed_movement_offsets=changed)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix',type=Path,action='append',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    assert len(args.prefix)>=2,'At least two independently built executions are required'
    suffixes=('.fixture.bin','.roads.raw.bin','.roads.normalized.bin')
    samples=[]
    for prefix in args.prefix:
        samples.append({suffix:Path(str(prefix)+suffix).read_bytes() for suffix in suffixes})
    baseline=samples[0]
    assert all(sample==baseline for sample in samples),'Cross-platform serialized bytes differ'
    expected,normalization=cannon_normalization(baseline['.roads.raw.bin'])
    assert expected==baseline['.roads.normalized.bin'],'Changes outside original cannon mask normalization'
    report=dict(status='PASS',scope='actual CPU graph bytes only; not battle/renderer execution',
                executions=len(samples),cross_platform_bytes_equal=True,normalization=normalization,
                artifacts={suffix:dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
                           for suffix,data in baseline.items()},battle_executed=False,hardware_executed=False)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__': main()
