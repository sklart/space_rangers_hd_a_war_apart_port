"""Verify requested real scene frames and sampled background regions (Pillow/NumPy)."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def pixels(path):
    rgb = np.asarray(Image.open(path).convert('RGB'), dtype=np.uint16)
    return (((rgb[:, :, 0] * 31 + 127) // 255) << 11 |
            ((rgb[:, :, 1] * 63 + 127) // 255) << 5 |
            (rgb[:, :, 2] * 31 + 127) // 255).astype('<u2')


def fingerprint(data):
    value = 1469598103934665603  # Actual renderer's legacy basis.
    for byte in data.tobytes():
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f'{value:016x}'


def shade(data):
    return (((data >> 11) // 2) << 11 | (((data >> 5) & 63) // 2) << 5 |
            (data & 31) // 2).astype('<u2')


def grayscale(data):
    wide = data.astype(np.uint32)
    gray = (((wide >> 11) * 19595 + (((wide >> 5) & 63) >> 1) * 38469 +
             (wide & 31) * 7471) >> 16).astype(np.uint16)
    return (gray << 11 | gray << 6 | gray).astype('<u2')


def generation_tiles(template, target):
    # Same-coordinate 32px tiles in the scene interior, excluding the shared
    # bottom HUD. Two adjacent rows/columns give a 64x64 retained fragment.
    a, b = template[32:608, 32:1248], target[32:608, 32:1248]
    a = a.reshape(18, 32, 38, 32).transpose(0, 2, 1, 3)
    b = b.reshape(18, 32, 38, 32).transpose(0, 2, 1, 3)
    varied = a.std(axis=(2, 3)) > 512
    match = np.all(a == b, axis=(2, 3)) & varied
    square = match[:-1, :-1] & match[1:, :-1] & match[:-1, 1:] & match[1:, 1:]
    return dict(matching_varied_tiles=int(match.sum()), retained_64px_fragment=bool(square.any()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('route_report', type=Path)
    args = parser.parse_args()
    path = args.route_report.resolve(strict=True)
    if not path.is_relative_to(ROOT / 'build'):
        raise ValueError('Only isolated build reports are accepted')
    route = json.loads(path.read_text())
    stage = path.parent
    if not (stage / 'frames').is_dir():
        stage = stage / 'runs' / Path(route['stage']).name
    samples = route['scene_frames']
    images = {}
    verified = []
    for sample in samples:
        sequence = sample['sequence']
        if not isinstance(sequence, int) or not 1 <= sequence <= 256:
            raise ValueError('Invalid capture sequence')
        metadata = json.loads((stage / 'frames' / f'frame-{sequence}.json').read_text())
        assert metadata == {k: v for k, v in sample.items() if k != 'label'}
        image = pixels(stage / 'frames' / f'frame-{sequence}.ppm')
        assert image.shape == (sample['height'], sample['width']) == (720, 1280)
        assert sample['complete'] and fingerprint(image) == sample['rgb565_hash']
        images[sample['label']] = image
        verified.append(sequence)
    source = images['generation-ready']
    templates = dict(raw=source, shade=shade(source), grayscale=grayscale(source),
                     shade_grayscale=grayscale(shade(source)))
    controls = {name: generation_tiles(image, image)['retained_64px_fragment']
                for name, image in templates.items()}
    fragments = {label: {name: generation_tiles(image, target) for name, image in templates.items()}
                 for label, target in images.items() if label != 'generation-ready'}
    suspicious = [label for label, variants in fragments.items()
                  if any(item['retained_64px_fragment'] for item in variants.values())]
    # The menu's inherited image is frozen. These two side regions avoid its
    # buttons, the confirmation box, top-right minimap and bottom shared HUD.
    regions = [(0, 0, 320, 600), (1024, 160, 1280, 600)]
    restoration = []
    for cycle in range(1, route['scene_cycles'] + 1):
        before = images[f'cycle-{cycle}-menu-before-modal']
        after = images[f'cycle-{cycle}-menu-after-modal']
        changed = [int(np.count_nonzero(before[t:b, l:r] != after[t:b, l:r]))
                   for l, t, r, b in regions]
        modal = next(s for s in samples if s['label'] == f'cycle-{cycle}-exit-confirmation')
        parent = next(s for s in samples if s['label'] == f'cycle-{cycle}-menu-after-modal')
        restoration.append(dict(cycle=cycle, changed_pixels=changed,
                                nested_owner_verified=modal['depth'] == 2 and not modal['registered_owner'] and
                                parent['depth'] == 1 and parent['registered_owner']))
    passed = (route['status'] == 'PASS' and len(verified) == 4 + route['scene_cycles'] * 6 and
              all(controls.values()) and not suspicious and
              all(not any(row['changed_pixels']) and row['nested_owner_verified'] for row in restoration))
    report = dict(status='PASS' if passed else 'FAIL', route_report_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  route_binary_sha256=route['binary_sha256'], verified_frames=len(verified),
                  generation_template_controls=controls, suspicious_generation_frames=suspicious,
                  fragment_measurements=fragments, parent_regions=regions, restoration=restoration,
                  hardware_executed=False, scope='Recorded host frames only: raw/shaded/grayscale exact 64px generation fragments and two frozen menu side regions; not all screens or all possible stale layers')
    (stage / 'scene-analysis.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'fragment_measurements'}, indent=2))
    if not passed: raise SystemExit(1)


if __name__ == '__main__': main()
