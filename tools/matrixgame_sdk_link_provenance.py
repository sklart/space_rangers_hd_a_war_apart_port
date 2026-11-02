"""Check actual SDK archive objects, not the requested CMake library label."""
import re
def deko_link_provenance(path,expected):
    assert expected in ('deko3d','deko3dd')
    text=path.read_text(encoding='utf-8',errors='replace')
    objects={name:sorted(set(re.findall(re.escape('lib'+name+'.a')+r'\(([^)]+)\)',text))) for name in ('deko3d','deko3dd')}
    other='deko3d' if expected=='deko3dd' else 'deko3dd'
    assert objects[expected],f'no actual {expected} objects in {path}'
    assert not objects[other],f'unexpected {other} object members in {path}'
    for required in ('dk_device.o','dk_memblock.o','dk_queue.o','dk_cmdbuf.o'):
        assert required in objects[expected],(required,path)
    return {'expected_library':expected,'actual_objects':objects,'mixed_variants':False}
