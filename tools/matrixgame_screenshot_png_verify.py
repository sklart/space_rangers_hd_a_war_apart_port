"""Independent PNG chunk/CRC/zlib/filter decode and RGB fixture oracle."""
import collections
import struct
import zlib
def decode(data,allow_alpha=False):
    assert data[:8]==b'\x89PNG\r\n\x1a\n';position=8;compressed=bytearray();header=None;ended=False
    while position<len(data):
        length,=struct.unpack_from('>I',data,position);kind=data[position+4:position+8];body=data[position+8:position+8+length]
        crc,=struct.unpack_from('>I',data,position+8+length);assert zlib.crc32(kind+body)&0xffffffff==crc
        position+=length+12
        if kind==b'IHDR':header=struct.unpack('>IIBBBBB',body)
        elif kind==b'IDAT':compressed+=body
        elif kind==b'IEND':ended=True;break
    assert ended and position==len(data) and header is not None
    width,height,depth,color,compression,filtering,interlace=header
    assert (depth,compression,filtering,interlace)==(8,0,0,0)
    assert color in ((2,6) if allow_alpha else (2,))
    channels=4 if color==6 else 3
    raw=zlib.decompress(compressed);stride=width*channels;assert len(raw)==height*(stride+1)
    prior=bytearray(stride);pixels=bytearray()
    for y in range(height):
        offset=y*(stride+1);mode=raw[offset];row=bytearray(raw[offset+1:offset+1+stride]);assert mode<5
        for i in range(stride):
            left=row[i-channels] if i>=channels else 0;above=prior[i];upper_left=prior[i-channels] if i>=channels else 0
            if mode==1:predictor=left
            elif mode==2:predictor=above
            elif mode==3:predictor=(left+above)//2
            elif mode==4:
                p=left+above-upper_left;distances=(abs(p-left),abs(p-above),abs(p-upper_left));predictor=(left,above,upper_left)[distances.index(min(distances))]
            else:predictor=0
            row[i]=(row[i]+predictor)&255
        pixels+=row;prior=row
    if channels==4:
        rgb=bytearray()
        for i in range(0,len(pixels),4):rgb+=pixels[i:i+3]
        pixels=rgb
    return width,height,bytes(pixels)
def expected(width,height,rgb565):
    pixels=bytearray()
    for y in range(height):
        for x in range(width):
            r=(x*19+y*37+11)&255;g=(x*73+y*13+67)&255;b=(x*31+y*83+179)&255
            if rgb565:r>>=3;g>>=2;b>>=3;r=(r<<3)|(r>>2);g=(g<<2)|(g>>4);b=(b<<3)|(b>>2)
            pixels+=bytes((r,g,b))
    return bytes(pixels)
def verify_fixture(root):
    counts=collections.Counter();files=list((root/'private/SpaceRangersHD/Screenshots').glob('Shot-*.png'))
    assert len(files)==36,len(files)
    for path in files:
        w,h,pixels=decode(path.read_bytes());assert w in (1,3,17) and h in (1,4,7)
        matches=[kind for kind in (False,True) if pixels==expected(w,h,kind)];assert len(matches)==1,path
        counts[w,h,matches[0]]+=1
    assert len(counts)==18 and set(counts.values())=={2},counts
    return {'status':'PASS; INDEPENDENT PNG RGB ORACLE','files':len(files),'layout_cases':len(counts),'chunk_CRC':True,'zlib':True,'all_scanline_filters_supported':True}
