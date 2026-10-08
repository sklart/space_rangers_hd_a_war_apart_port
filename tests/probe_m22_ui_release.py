#!/usr/bin/env python3
"""Read-only M22 UI configuration inventory for a release game root."""
from __future__ import annotations
import argparse
import importlib.util
import json
import pathlib
from collections import Counter, defaultdict

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('m21_inventory', HERE / 'probe_ui_images_release.py')
inventory = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(inventory)

SUPPORTED = frozenset(('Panel', 'SimpleImage', 'TransImage', 'AlphaImage', 'Image'))
BASE = frozenset(('Style', 'Pos', 'PosZ', 'Size', 'Sme', 'Name', 'Active'))


def paths_for(decoded):
    values = defaultdict(dict)
    blocks = set()
    for path, name, value in inventory.block_entries(decoded):
        parent = path.rsplit('/', 1)[0] if '/' in path else ''
        if parent:
            blocks.add(parent)
        values[parent][name] = value
    return blocks, values


def descendants(path, controls):
    prefix = path + '/'
    return [child for child in controls if child == path or child.startswith(prefix)]


def hierarchy(blocks, controls):
    depth = max((item.count('/') + 1 for item in controls), default=0)
    child_counts = Counter()
    for item in controls:
        parent = item.rsplit('/', 1)[0] if '/' in item else ''
        child_counts[parent] += 1
    return depth, max(child_counts.values(), default=0)


def style_report(blocks, values):
    styles = sorted(path for path in blocks if path.startswith('ML/') and path.count('/') == 1)
    names = {path.rsplit('/', 1)[1] for path in styles}
    edges = {path.rsplit('/', 1)[1]: values[path].get('Style', '') for path in styles if values[path].get('Style', '') in names}
    cycles = []
    max_depth = 0
    for start in sorted(names):
        seen = []
        current = start
        while current in edges:
            if current in seen:
                cycle = tuple(seen[seen.index(current):])
                if cycle not in cycles:
                    cycles.append(cycle)
                break
            seen.append(current)
            current = edges[current]
        max_depth = max(max_depth, len(seen))
    return {'unique_style_names': len(names), 'missing_style_references': sorted({values[path].get('Style', '') for path in blocks if values[path].get('Style', '') and values[path].get('Style', '') not in names}), 'cycles': [list(cycle) for cycle in cycles], 'max_style_depth': max_depth}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('game_root', type=pathlib.Path)
    ap.add_argument('--json', type=pathlib.Path)
    ns = ap.parse_args()
    main_dat = ns.game_root / 'CFG' / 'Main.dat'
    decoded = inventory.decode_dat(main_dat)
    blocks, values = paths_for(decoded)
    controls = sorted(path for path in blocks if path.rsplit('/', 1)[-1] in inventory.KNOWN_CONTROLS)
    terminal = lambda path: path.rsplit('/', 1)[-1]
    control_counts = Counter(terminal(path) for path in controls)
    base_counts = Counter(name for props in values.values() for name in props if name in BASE)
    depth_values = values.get('ZPos', {})
    symbolic_depths = sorted({value.strip() for props in values.values() for name, value in props.items() if name == 'PosZ' and value.strip()})
    resolved_depths = sorted(name for name in symbolic_depths if name in depth_values)
    unresolved_depths = sorted(name for name in symbolic_depths if name not in depth_values)
    max_depth, max_children = hierarchy(blocks, controls)
    candidates = []
    for root in controls:
        if terminal(root) not in SUPPORTED:
            continue
        nodes = descendants(root, controls)
        kinds = {terminal(item) for item in nodes}
        visual = [item for item in nodes if terminal(item) != 'Panel']
        resources_ok = all(values[item].get('Image', '').strip() for item in visual)
        nested = max((item.count('/') - root.count('/') + 1 for item in nodes), default=0)
        if kinds <= SUPPORTED and resources_ok and nested >= 2 and len(visual) >= 3:
            candidates.append({'path': root, 'nodes': len(nodes), 'max_depth': nested, 'control_types': dict(sorted(Counter(terminal(item) for item in nodes).items())), 'resources': len(visual)})
    candidate = min(candidates, key=lambda item: item['path'].casefold()) if candidates else None
    result = {
        'source': str(main_dat), 'read_only': True,
        'total_control_blocks': len(controls), 'control_type_counts': dict(sorted(control_counts.items())),
        'supported_control_count': sum(control_counts[name] for name in SUPPORTED),
        'unsupported_control_count': len(controls) - sum(control_counts[name] for name in SUPPORTED),
        'base_property_counts': dict(sorted(base_counts.items())),
        'hierarchy': {'max_nested_control_depth': max_depth, 'max_direct_control_children': max_children},
        'styles': style_report(blocks, values),
        'depth_names': {'defined': len(depth_values), 'symbolic_used': symbolic_depths, 'resolved': resolved_depths, 'unresolved': unresolved_depths},
        'real_fully_supported_subtree': candidate if candidate else 'NOT FOUND',
        'status': 'PRESENT',
    }
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    if ns.json:
        ns.json.write_text(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
