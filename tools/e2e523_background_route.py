"""Read-only attribution of the first corrupted hardware scene, not a render fix."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GIT = r'C:\Program Files\Git\cmd\git.exe'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def historical(path):
    return subprocess.check_output([GIT, 'show', '03651ce:' + path], cwd=ROOT)


def main():
    path = ROOT / 'logs/2026.10.23/logs/perf515-stage.log.1'
    raw = path.read_bytes()
    lines = raw.decode('utf-8', errors='replace').splitlines()
    records = []
    for number, line in enumerate(lines, 1):
        match = re.match(r'mono_us=(\d+) ', line)
        if match:
            records.append((int(match[1]), number, line))
    # Async writer output is not guaranteed to be in monotonic timestamp order.
    records.sort()
    enters = [x for x in records if 'event=scene-enter screen=20 ' in x[2]
              and ' frame=' not in x[2]]
    assert len(enters) == 1, 'First-run assumption must be reconsidered'
    start = enters[0][0]
    end = next(x[0] for x in records if x[0] > start and
               'event=compose-after screen=20 ' in x[2])
    selected = [dict(mono_us=t, line=n, text=s) for t,n,s in records
                if start <= t <= end and any(k in s for k in [
                    '[SCENE]', 'scene canvas', 'background state', '2RCbg',
                    'geometry ready', 'bounds ready', 'activate ready'])]
    assert any('inherit=0 nested=0 aux_width=0 aux_height=0 prepared=0' in x['text']
               for x in selected)
    assert any('saved_pixels=0 saved_lines=0' in x['text'] for x in selected)
    assert any('parent=0x0 depth=0 stack=empty' in x['text'] for x in selected)
    assert any('file=scene-40986867502-023.ppm' in x['text'] for x in selected)
    inputs={str(path.relative_to(ROOT)):digest(raw)}
    source_checks={}
    kit=ROOT/'build/e2e516/hardware-e2e516-03651ce'
    mapping=json.loads((kit/'effective-source-map.json').read_text())
    hashes=json.loads((kit/'source-sha256.json').read_text())
    for name in ['upstream/cpp/src/GI_MessageLoop.cpp',
                 'upstream/cpp/src/fRuinsTalk.cpp',
                 'port/switch/platform/renderer_platform.cpp',
                 'tools/e2e_source_overrides.py']:
        if name.startswith('upstream/'):
            effective=mapping[name]
            source=(ROOT/effective).read_bytes()
            assert digest(source)==hashes[effective], 'Frozen effective source differs: '+effective
        else:
            source=historical(name)
        inputs['03651ce:'+name]=digest(source)
        source_checks[name]=source.decode('utf-8')
    loop=source_checks['upstream/cpp/src/GI_MessageLoop.cpp']
    assert 'Self->SavedBackgroundControl = nullptr;' in loop
    assert loop.count('SavedBackgroundControl = pas::construct_call') == 1
    ruins=source_checks['upstream/cpp/src/fRuinsTalk.cpp']
    assert 'ViewportRect = ClassesImports::Rect(0, 0, GR_Main::GameScreenWidth, GR_Main::GameScreenHeight);' in ruins
    renderer=source_checks['port/switch/platform/renderer_platform.cpp']
    assert 'if (inherit_background)' in renderer and 'canvas_->FillPixels16(0);' in renderer
    report={
        'source_ref':'03651ce', 'input_sha256':inputs,
        'first_RuinsTalk_scene_count':len(enters), 'events':selected,
        'conclusions':[
            'First observed RuinsTalk is independent, depth zero, inherit false, empty aux and no prepared background.',
            'Original SavedBackgroundControl starts null and is created only for a smaller viewport; RuinsTalk configures full viewport.',
            'Saved pixel/line lists are empty at the sampled first composition.',
            'These paths do not explain this first corrupted scene under recorded control flow; GI resource composition remains open.'
        ],
        'limits':[
            'Loop and RuinsTalk effective sources match frozen source-sha256 manifest; renderer and overlay generator read from historical ref. This is not a new matching-ELF audit.',
            'No source GI payload hash was recorded on hardware.',
            'No new execution or hardware test; no inference from missing diagnostic records to missing draws.',
            'The exact stale allocation and cause of historical corruption remain unproven.'
        ]}
    output=ROOT/'reports/e2e5/hardware-20261023-background-route.json'
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'events':len(selected),'output':str(output)},ensure_ascii=False))


if __name__=='__main__':
    main()
