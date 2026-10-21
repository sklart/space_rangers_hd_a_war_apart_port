"""Check real model, dirty rectangles, ship drawing and CPU presentation ROIs.

Input comes from e2e516_map_probe.hpp in the complete effective host game.
This is not a GPU readback or a hardware acceptance test.
"""
import argparse
import csv
import json
from pathlib import Path

FIELDS = ('phase frame turn film mode id x y control left top right bottom before after dirty').split()

def analyze(path):
    records = []
    for row in csv.reader(path.open()):
        if len(row) != len(FIELDS):
            raise ValueError('Incomplete map observation')
        item = dict(zip(FIELDS, row))
        for key in FIELDS:
            if key not in ('phase', 'control', 'x', 'y'):
                item[key] = int(item[key])
        item['x'] = float(item['x']); item['y'] = float(item['y'])
        records.append(item)
    models = [r for r in records if r['phase'] == 'model-invalidation']
    turns = sorted({r['turn'] for r in models})
    endpoints = {}
    for r in models:
        # Calculation can install next-day object positions while CurrentTurn
        # still names the old day. Compare completed order modes, not this
        # transient film/calculation frame against the completed next turn.
        if r['mode']==1:endpoints[(r['turn'], r['id'])] = r
    moving = []
    if len(turns) > 1:
        for (turn, ship), before in endpoints.items():
            after = endpoints.get((turns[-1], ship))
            if turn == turns[0] and after and (before['x'], before['y']) != (after['x'], after['y']):
                moving.append({'id': ship, 'before': [before['x'], before['y']],
                               'after': [after['x'], after['y']], 'dirty_rectangles': after['dirty']})
    draws = [r for r in records if r['phase'] == 'ship-draw']
    writes = [r for r in draws if r['before'] != r['after']]
    composed = {(r['frame'], r['id']): r for r in records if r['phase'] == 'world-ui-composed'}
    present = [r for r in records if r['phase'] == 'present-input' and
               r['right']>max(0,r['left']) and r['bottom']>max(0,r['top']) and
               r['left']<1280 and r['top']<720]
    pairs = [(composed[(r['frame'], r['id'])], r) for r in present if (r['frame'], r['id']) in composed]
    exact = [(a,b) for a,b in pairs if a['after'] == b['after']]
    moving_ids = {r['id'] for r in moving}
    moving_writes = {(r['frame'],r['id']) for r in writes if r['id'] in moving_ids and r['turn'] == turns[-1]}
    moving_uploaded = {(r['frame'],r['id']) for _,r in exact if r['id'] in moving_ids and r['turn'] == turns[-1]}
    dirty = {(r['frame'],r['id']) for r in models if r['dirty']>0 and r['turn']==turns[-1]}
    # Require evidence for a real moving object across all phases, rather than
    # attributing a changed tooltip or global framebuffer hash to the map.
    witnessed = {ship for _,ship in dirty & moving_writes & moving_uploaded}
    return {'status': 'HOST_MAP_CONTENT_VERIFIED' if witnessed else 'INCONCLUSIVE',
            'hardware_executed': False, 'gpu_readback': False,
            'turns': turns, 'records': len(records), 'moving_model_ships': moving,
            'ship_draw_calls': len(draws), 'ship_draws_with_pixel_changes': len(writes),
            'composition_present_roi_pairs': len(pairs), 'exact_roi_pairs': len(exact),
            'roi_pairs_changed_after_composition': len(pairs)-len(exact),
            'moving_ships_witnessed_through_all_phases': sorted(witnessed),
            'scope': 'Real ship-control rectangles; UI can overlap final ROIs. Drawing evidence is taken directly around each ship draw. CPU upload input only.'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();result=analyze(a.input)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    return 0 if result['status']=='HOST_MAP_CONTENT_VERIFIED' else 1

if __name__=='__main__':raise SystemExit(main())
