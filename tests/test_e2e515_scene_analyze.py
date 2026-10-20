import sys
import unittest
from pathlib import Path
import tempfile
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from e2e515_scene_analyze import analyze,changed

class SceneAnalysis(unittest.TestCase):
    def test_cursor_only_change_not_map_update(self):
        a=np.zeros((360,640,3),dtype=np.uint8);b=a.copy()
        b[170:180,310:320]=255
        meta=dict(width='1280',height='720',cursor_x='630',cursor_y='350')
        result=changed(a,b,meta,meta)
        self.assertEqual(result['center_changed_without_cursor'],0)
        self.assertEqual(result['cursor_regions_changed'],100)
        b[120,220]=255;b[350,20]=255
        result=changed(a,b,meta,meta)
        self.assertEqual(result['center_changed_without_cursor'],1)
        self.assertEqual(result['panel_changed'],1)

    def test_frozen_map_with_updated_state_and_matching_upload(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);log=root/'stage.log'
            pixels=np.zeros((360,640,3),dtype=np.uint8)
            lines=[]
            for seq,frame,turn,event in [(1,1,301,'compose-after'),(2,2,302,'compose-after'),(3,2,302,'present-input')]:
                name=f'scene-123-{seq:03}.ppm';Image.fromarray(pixels).save(root/name)
                lines.append(f'mono_us={seq} [SCENE] event={event} screen=16 generation=1 owner=0x100 frame={frame} width=1280 height=720 turn={turn} film=0 order=0 file={name} image_ok=1')
            log.write_text('\n'.join(lines))
            result=analyze(log,root)
            frozen=result['comparisons'][0]
            self.assertTrue(frozen['state_changed'])
            self.assertEqual(frozen['center_changed_without_cursor'],0)
            self.assertTrue(result['comparisons'][1]['upload_matches_compose'])
            self.assertIn('HARDWARE PENDING',frozen['switch_visible_result'])

    def test_missing_capture_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);log=root/'stage.log'
            log.write_text('mono_us=1 [SCENE] event=compose-after file=scene-1-001.ppm image_ok=0 image_dropped=2')
            result=analyze(log,root)
            self.assertEqual(result['missing_images'],['scene-1-001.ppm'])
            self.assertEqual(result['dropped_images'],2)

if __name__=='__main__':unittest.main()
