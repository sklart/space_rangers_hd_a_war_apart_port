"""Compare bounded scene snapshots; CPU upload input is not GPU/display proof."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import numpy as np
from PIL import Image


def changed(a, b, left, right):
    delta = np.any(a != b, axis=2)
    h, w = delta.shape
    cursor = delta.copy()
    cursor[:] = False
    for row in (left, right):
        x = int(row.get('cursor_x', -1000)) * w // int(row['width'])
        y = int(row.get('cursor_y', -1000)) * h // int(row['height'])
        rx = max(1, 32*w//int(row['width']))
        ry = max(1, 32*h//int(row['height']))
        if 0 <= x < w and 0 <= y < h:
            cursor[max(0,y-ry):min(h,y+ry+1), max(0,x-rx):min(w,x+rx+1)] = True
    return dict(center_changed_without_cursor=int((delta & ~cursor)[h//6:5*h//6,w//4:3*w//4].sum()),
                panel_changed=int((delta & ~cursor)[9*h//10:].sum()),
                cursor_regions_changed=int((delta & cursor).sum()), all_changed=int(delta.sum()))


def analyze(log, directory):
    rows = []
    for line in log.read_text(encoding='utf-8', errors='replace').splitlines():
        if '[SCENE]' not in line:
            continue
        row = dict(re.findall(r'(\w+)=([^\s]+)', line))
        if 'event' in row and 'mono_us' in row:
            rows.append(row)
    rows.sort(key=lambda row: int(row['mono_us']))
    images = {}
    missing = []
    for row in rows:
        if 'file' not in row:
            continue
        name = row['file']
        if not re.fullmatch(r'scene-\d+-\d{3}\.ppm', name):
            raise ValueError('Unexpected snapshot filename')
        path = directory / name
        if row.get('image_ok') != '1' or not path.is_file():
            missing.append(name)
            continue
        if path.is_symlink() or path.stat().st_size > 1024*1024:
            raise ValueError('Unsafe or oversized snapshot')
        with Image.open(path) as image:
            if image.format != 'PPM' or image.size != (640,360):
                raise ValueError('Unexpected snapshot size or format')
            images[name] = np.array(image.convert('RGB'))
    previous = {}
    composed = {}
    comparisons = []
    for row in rows:
        if row.get('file') not in images:
            continue
        identity = (row.get('generation'), row.get('owner'))
        key = (*identity, row.get('frame'))
        if row['event'] == 'compose-after':
            if identity in previous:
                old = previous[identity]
                comparisons.append(dict(screen=int(row['screen']), generation=row['generation'],
                    before=old['file'], after=row['file'], mono_us=int(row['mono_us']),
                    state_changed=any(old.get(k) != row.get(k) for k in ('turn','film','order')),
                    state_fields={k: [old.get(k), row.get(k)] for k in ('turn','film','order')},
                    **changed(images[old['file']], images[row['file']], old, row),
                    switch_visible_result='HARDWARE PENDING: compare with user video'))
            previous[identity] = row
            composed[key] = row
        elif row['event'] == 'present-input' and key in composed:
            after = composed[key]
            comparisons.append(dict(screen=int(row['screen']), generation=row['generation'],
                compose=after['file'], present_input=row['file'],
                upload_matches_compose=bool(np.array_equal(images[after['file']], images[row['file']]))))
    return dict(status='ANALYZED INPUT DATA; not a Switch acceptance PASS', events=len(rows),
                snapshots=len(images), missing_images=missing,
                dropped_images=max((int(r.get('image_dropped',0)) for r in rows),default=0),
                actions=[r for r in rows if r['event'] in ('day-accepted','turn-film-start','turn-film-stop','map-state','map-state-observed','load-from-screen-failed')],
                comparisons=comparisons,
                scope='640x360 samples; fixed central rectangle and bottom panel; union of two cursor masks excluded; small/moved features can be missed',
                input_log_sha256=hashlib.sha256(log.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log',type=Path,required=True)
    parser.add_argument('--images',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args = parser.parse_args()
    result = analyze(args.log,args.images)
    args.out.mkdir(parents=True,exist_ok=False)
    (args.out/'scene-analysis.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    lines=['# Scene comparison','','CPU composition/upload samples; display result requires user video.','',
           '| Screen | State changed | Central pixels changed excluding cursor | Panel pixels changed | Cursor-region pixels changed |',
           '|---|---|---:|---:|---:|']
    for row in result['comparisons']:
        if 'state_changed' in row:
            lines.append(f"| {row['screen']} | {row['state_changed']} | {row['center_changed_without_cursor']} | {row['panel_changed']} | {row['cursor_regions_changed']} |")
    lines += ['',result['scope'],'',f"Missing images: {len(result['missing_images'])}; dropped: {result['dropped_images']}",'']
    (args.out/'scene-report.md').write_text('\n'.join(lines),encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('status','events','snapshots','missing_images','dropped_images')},indent=2))


if __name__ == '__main__':
    main()
