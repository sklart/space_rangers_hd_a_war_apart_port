#!/usr/bin/env python3
"""Independent structural oracle for the portable M22 UI fingerprint format."""
import struct
import zlib

OFFSET=0xcbf29ce484222325; PRIME=0x100000001b3
def fnv(data):
    value=OFFSET
    for byte in data: value=((value^byte)*PRIME)&0xffffffffffffffff
    return value
def node(kind,name,parent,local,absolute,size,origin,depth,w,active,disabled,children):
    raw=bytearray([kind]); encoded=name.encode(); raw+=struct.pack('<I',len(encoded))+encoded
    raw+=struct.pack('<i',parent)+struct.pack('<8i',*local,*absolute,*size,*origin)
    raw+=struct.pack('<Q',struct.unpack('<Q',struct.pack('<d',depth))[0])+bytes((w,active,disabled))+struct.pack('<I',children)
    return raw
def main():
    # Root Panel, then equal-depth newer child before older child: exact M22 order.
    data=node(1,'root',-1,(0,0),(0,0),(4,3),(0,0),0.0,0,1,0,2)
    data+=node(0,'new',0,(2,0),(2,0),(1,1),(0,0),5.0,0,1,0,0)
    data+=node(0,'old',0,(1,0),(1,0),(1,1),(0,0),5.0,0,1,0,0)
    crc=zlib.crc32(data)&0xffffffff; hash=fnv(data)
    assert (crc,hash,len(data))==(0x9dc4ec99,0x90e44c6a95e84c93,178)
    print(f'M22 UI TREE ORACLE PASS tree_crc32={crc:08x} tree_fnv64={hash:016x} bytes={len(data)}')
if __name__=='__main__': main()
