"""Execute the original manager on real D3D9; keep undefined GPU results observational."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
MANAGER = 'MatrixGame/src/MatrixSampleStateManager.cpp'
MANAGER_SHA = '6d2562c1ba84c166023efbdff1b16b92a7e0904bd5a22474d17037f8702b9b1e'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--source-dir', type=Path, required=True, help='Snapshot used by the native reference build')
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    for source in (ROOT/'upstream/matrixgame'/MANAGER, a.source_dir/MANAGER):
        assert hashlib.sha256(source.read_bytes().replace(b'\r\n', b'\n')).hexdigest() == MANAGER_SHA, source
    binary = a.binary.resolve(); reports = []
    for mode in ('hal', 'ref'):
        r = subprocess.run([str(binary), '--'+mode], capture_output=True, timeout=90)
        (out/(mode+'-stdout.txt')).write_bytes(r.stdout)
        (out/(mode+'-stderr.txt')).write_bytes(r.stderr)
        assert r.returncode == 0, (mode, r.returncode, r.stderr[-2000:])
        text = r.stdout.decode(); error = r.stderr.decode()
        cap = re.search(r'transitions=(\d+) cap=(\d+) filter_caps=([0-9a-f]+)', error)
        assert cap, error
        transitions, maximum, caps = int(cap[1]), int(cap[2]), int(cap[3], 16)
        settings = None; count = 0
        for line in text.splitlines():
            m = re.fullmatch(r'settings requested=(-?\d+) effective=(\d+) cap=(\d+)', line)
            if m:
                requested, settings, limit = map(int, m.groups())
                assert limit == maximum and settings == min(requested & 0xffffffff, maximum)
            m = re.fullmatch(r'set reg=(\d+) input=(\d+) hr=([0-9a-f]+) stored=(\d+) max=(\d+)', line)
            if m:
                reg, value, hr, stored, degree = m.groups()
                reg, value, stored, degree = map(int, (reg, value, stored, degree))
                assert settings is not None and reg in (5, 6, 7) and int(hr, 16) == 0
                enabled = settings != 0 and value == 2
                assert stored == (3 if enabled else value) and degree == (settings if enabled else 1)
                count += 1
        assert count == transitions == 54
        draws = []
        for m in re.finditer(r'draw af=(\d+) original=([0-9a-f]+) linear_mip=([0-9a-f]+) differences=(\d+) validate_original=([0-9a-f]+) passes=(\d+)', text):
            degree, original, linear, differences, validation, passes = m.groups()
            draws.append(dict(af=int(degree), original_pixel=original, defined_mip_pixel=linear,
                              different_pixels=int(differences), validation=validation, passes=int(passes)))
        assert [d['af'] for d in draws] == [0, 1, 2, 4, 8, 16]
        assert draws[0]['validation'] == '00000000' and draws[0]['different_pixels'] == 0
        isolated = [dict(register=int(m[1]), pixel=m[2], validation=m[3], passes=int(m[4]))
                    for m in re.finditer(r'isolated reg=(\d+) pixel=([0-9a-f]+) validation=([0-9a-f]+) passes=(\d+)', text)]
        assert [s['register'] for s in isolated] == [6, 5, 7]
        if caps & 0x400:  # D3DPTFILTERCAPS_MINFANISOTROPIC
            assert isolated[0]['validation'] == '00000000'
        reports.append(dict(device=mode, manager_transitions=count, max_anisotropy=maximum,
                            filter_caps=hex(caps), draws=draws, isolated=isolated))
    report = dict(scope='Original compiled manager + actual Windows D3D9 state/draw/readback',
                  manager_source_sha256=MANAGER_SHA, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  undefined_mip_results='observations only; not a portable expected image',
                  switch_gpu_executed=False, results=reports)
    (out/'oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
