import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('map516',Path(__file__).parents[1]/'tools/e2e516_map_analyze.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class MapEvidenceTests(unittest.TestCase):
    def analyze(self,rows):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'map.csv'
            with p.open('w',newline='') as f:csv.writer(f).writerows(rows)
            return module.analyze(p)

    def row(self,phase,turn=361,ship=5,before=0,after=42,dirty=1):
        return [phase,turn,turn,0,1,ship,turn,0,'0x123',10,20,50,60,before,after,dirty]

    def test_tooltip_or_other_ship_cannot_prove_moving_ship_rendered(self):
        rows=[self.row('model-invalidation',360),self.row('model-invalidation'),
              self.row('ship-draw',ship=6,before=1),self.row('world-ui-composed'),self.row('present-input')]
        self.assertEqual(self.analyze(rows)['status'],'INCONCLUSIVE')

    def test_model_alone_or_frozen_upload_is_insufficient(self):
        rows=[self.row('model-invalidation',360),self.row('model-invalidation'),
              self.row('ship-draw',before=1),self.row('world-ui-composed'),self.row('present-input',after=99)]
        self.assertEqual(self.analyze(rows)['status'],'INCONCLUSIVE')

    def test_same_moving_ship_requires_dirty_draw_and_matching_upload(self):
        rows=[self.row('model-invalidation',360),self.row('model-invalidation'),
              self.row('ship-draw',before=1),self.row('world-ui-composed'),self.row('present-input')]
        result=self.analyze(rows)
        self.assertEqual(result['status'],'HOST_MAP_CONTENT_VERIFIED')
        self.assertEqual(result['moving_ships_witnessed_through_all_phases'],[5])

    def test_drawing_another_frame_cannot_prove_current_frame(self):
        draw=self.row('ship-draw',before=1);draw[1]=999
        rows=[self.row('model-invalidation',360),self.row('model-invalidation'),draw,
              self.row('world-ui-composed'),self.row('present-input')]
        self.assertEqual(self.analyze(rows)['status'],'INCONCLUSIVE')

    def test_calculation_updates_positions_before_turn_number(self):
        transient=self.row('model-invalidation',360);transient[4]=2;transient[6]=361
        rows=[self.row('model-invalidation',360),transient,self.row('model-invalidation'),
              self.row('ship-draw',before=1),self.row('world-ui-composed'),self.row('present-input')]
        self.assertEqual(self.analyze(rows)['moving_ships_witnessed_through_all_phases'],[5])

if __name__=='__main__':unittest.main()
