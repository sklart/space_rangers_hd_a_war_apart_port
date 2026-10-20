"""Verify requested real scene frames and sampled background regions (Pillow/NumPy)."""
import argparse
import hashlib
import json
import re
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
    parser.add_argument('--generation-reference', type=Path, help='Recorded build PPM reference for a separate copied-save planet route; not same-process generation')
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
    if route.get('interstellar_arrival_verified'):
        trace = [json.loads(line.split('|', 1)[1]) for line in
                 (stage / 'stderr.log').read_text(errors='replace').splitlines()
                 if line.startswith('E2E515_FLIGHT|') and line.endswith('}')]
        reported = route['flight_trace']
        assert trace[:len(reported)] == reported and len(reported) >= 3
        # Returning from the game menu can publish orders mode again without
        # advancing the world; accept only identical post-arrival snapshots.
        assert all(row == reported[-1] for row in trace[len(reported):])
        target = route['flight_target']['id']
        assert trace[0]['star'] == route['flight_origin'] != target
        assert any(row['target'] == target and row['order'] == 3 for row in trace)
        assert trace[-1]['star'] == target and not trace[-1]['hyperspace']
        assert trace[-1]['turn'] > trace[0]['turn']
        assert trace[0]['fuel'] - trace[-1]['fuel'] == route['flight_target']['distance']
        assert route['saved_checkpoint']['star'] == target
        assert route['saved_checkpoint']['turn'] == trace[-1]['turn']
        expected = [('flight-origin-map', 16), ('flight-galaxy-targets', 18),
                    ('flight-galaxy-selected', 18), ('flight-arrived-map', 16)]
        assert [(sample['label'], sample['screen']) for sample in samples] == expected
        assert all(sample['depth'] == 1 and sample['registered_owner'] for sample in samples)
        log = next(stage.rglob('e2e.log')).read_text(errors='replace')
        assert 'screen dispatch id=19 ' in log and re.search(r'HOST raw pad sequence=\d+ mask=1024', log)
        inventory = (stage / 'stderr.log').read_text(errors='replace')
        item_ids = re.findall(r'E2E5_SAVE_INV\|Script.PC_fem_rangers\|init\|quest_item\|vkDword\|STABLE_OBJECT_ID\|item\|(\d+)', inventory)
        assert item_ids and int(item_ids[-1]) > 0
        save = stage / 'user/config/save' / Path(route['manual_save_path']).name
        assert hashlib.sha256(save.read_bytes()).hexdigest() == route['manual_save_sha256']
        result = dict(status='PASS', route_kind='interstellar-item-save', verified_frames=len(verified),
                      star_before=trace[0]['star'], star_after=target, turn_after=trace[-1]['turn'],
                      fuel_consumed=trace[0]['fuel'] - trace[-1]['fuel'], typed_item_id=int(item_ids[-1]),
                      original_jump_screen_verified=True, raw_touch_marker_verified=True,
                      hardware_executed=False, full_world_parity=False)
        (stage / 'scene-analysis.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return
    if route.get('trade_roundtrip') and not route.get('planet_cycles'):
        records = [json.loads(line.split('|', 1)[1]) for line in
                   (stage / 'stderr.log').read_text(errors='replace').splitlines()
                   if line.startswith('E2E515_TRADE|') and line.endswith('}')]
        assert len(records) == 6 and len(route['trade_roundtrip']) == 2
        assert {k: v for k, v in records[0].items() if k != 'phase'} == {
            k: v for k, v in records[1].items() if k != 'phase'}
        for i, operation in enumerate(route['trade_roundtrip']):
            before, after = records[2 + i * 2:4 + i * 2]
            assert before == operation['before'] and after == operation['after']
            good = operation['good']
            old, new = before['goods'][good], after['goods'][good]
            direction = 1 if i == 0 else -1
            price = old['buy' if i == 0 else 'sell']
            assert new['cargo'] - old['cargo'] == direction
            assert new['stock'] - old['stock'] == -direction
            assert after['money'] - before['money'] == -direction * price
            assert after['free_space'] - before['free_space'] == -direction
            # This fixture buys into an empty entry, then sells its only unit.
            assert old['cargo'] == i and new['cargo'] == 1 - i
            assert new['cost'] == (price if i == 0 else 0)
            assert all(a == b for a, b in zip(before['goods'], after['goods']) if a['id'] != good)
        assert route['trade_roundtrip'][0]['good'] == route['trade_roundtrip'][1]['good']
        for sample in samples:
            modal = sample['label'] in ('goods-quantity-dialog', 'buy-one-unit', 'sell-one-unit')
            assert sample['screen'] == 34 and sample['depth'] == (2 if modal else 1)
            assert sample['registered_owner'] == (not modal)
        result = dict(status='PASS', route_kind='one-unit-planet-trade', verified_frames=len(verified),
                      numeric_source='actual E2E515_TRADE stderr records', cancel_unchanged=True,
                      money_cargo_stock_free_space_cost_verified=True, owner_depth_verified=True,
                      hardware_executed=False, full_world_parity=False)
        (stage / 'scene-analysis.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return
    if route.get('planet_cycles'):
        if not args.generation_reference:
            parser.error('Planet analysis requires an explicit recorded generation reference')
        reference = args.generation_reference.resolve(strict=True)
        if not reference.is_relative_to(ROOT / 'build'):
            raise ValueError('Only an isolated build reference is accepted')
        source = pixels(reference)
        templates = dict(raw=source, shade=shade(source), grayscale=grayscale(source),
                         shade_grayscale=grayscale(shade(source)))
        controls = {name: generation_tiles(image, image)['retained_64px_fragment']
                    for name, image in templates.items()}
        targets = {label: target for label, target in images.items() if label != 'load-manager'}
        fragments = {label: {name: generation_tiles(image, target) for name, image in templates.items()}
                     for label, target in targets.items()}
        suspicious = [label for label, variants in fragments.items()
                      if any(item['retained_64px_fragment'] for item in variants.values())]
        log = next(stage.rglob('e2e.log')).read_text(errors='replace')
        entries = [tuple(map(int, row)) for row in re.findall(
            r'scene canvas id=(\d+) inherit=(\d+) nested=(\d+) aux_width=(\d+) aux_height=(\d+) prepared=(\d+)', log)
                   if int(row[0]) in (4, 5, 8, 28, 34) and int(row[2]) == 0]
        required_entries = 1 + route['planet_cycles'] * 5 + (1 if route.get('trade_quantity_cancel') else 0)
        independent = len(entries) >= required_entries and all(not row[1] and not row[3] and not row[4] and not row[5] for row in entries)
        expected = [(button, screen) for _ in range(route['planet_cycles']) for button, screen in
                    [('PP_Shop', 8), ('PP_Goods', 34), ('PP_Info', 28), ('PP_Hangar', 4), ('ButClose', 5)]]
        actual = [(row['button'], row['screen']) for row in route['planet_transitions']]
        owners = all(sample['registered_owner'] and sample['depth'] == 1 for sample in samples
                     if not sample['label'].startswith('goods-'))
        trade = not route.get('trade_quantity_cancel')
        if route.get('trade_quantity_cancel'):
            trade_samples = {sample['label']: sample for sample in samples if sample['label'].startswith('goods-')}
            trade = (set(trade_samples) == {'goods-before-quantity', 'goods-quantity-dialog', 'goods-after-cancel'} and
                     trade_samples['goods-before-quantity']['depth'] == 1 and
                     trade_samples['goods-quantity-dialog']['depth'] == 2 and
                     not trade_samples['goods-quantity-dialog']['registered_owner'] and
                     trade_samples['goods-after-cancel']['depth'] == 1 and
                     trade_samples['goods-after-cancel']['registered_owner'] and
                     route['trade_native_confirms'] == 1 and route['trade_checkpoint_diff'] == {})
        nonempty = all(len(np.unique(image)) >= 256 for image in targets.values())
        passed = (route['status'] == 'PASS' and route['source_save_unchanged'] and route['source_configs_unchanged'] and
                  len(verified) == 2 + route['planet_cycles'] * 5 + (3 if route.get('trade_quantity_cancel') else 0) and
                  actual == expected and independent and owners and trade and
                  nonempty and all(controls.values()) and not suspicious)
        report = dict(status='PASS' if passed else 'FAIL', route_kind='copied-save planet panels',
                      route_report_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                      route_binary_sha256=route['binary_sha256'], verified_frames=len(verified),
                      independent_scene_entries=len(entries), independent_entry_flags_verified=independent,
                      required_independent_entries=required_entries,
                      registered_owners_verified=owners, nonempty_scene_images=nonempty,
                      quantity_cancel_verified=bool(route.get('trade_quantity_cancel')) and trade,
                      generation_reference_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
                      generation_reference_same_process=False, generation_template_controls=controls,
                      suspicious_generation_frames=suspicious, fragment_measurements=fragments,
                      transitions_verified=actual == expected, hardware_executed=False,
                      scope='Recorded planet/shop/info/hangar frames and independent canvas entry flags; generation reference is from another host process, not same-process regeneration or all possible stale layers')
        (stage / 'scene-analysis.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({k: v for k, v in report.items() if k != 'fragment_measurements'}, indent=2))
        if not passed: raise SystemExit(1)
        return
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
