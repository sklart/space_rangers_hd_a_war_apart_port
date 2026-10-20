import csv
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from e2e515_perf_analyze import analyze,sysclk,timestamp

class Analysis(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=ROOT/'build/e2e515')
        self.root=Path(self.temp.name)
        self.game=self.root/'game.csv';self.clock=self.root/'context.csv';self.log=self.root/'stage.log'
        self.epoch=1792522000000
        self.game.write_text('stage,mono_us,epoch_ms,wall_us,cpu_us,screen,a,b\n'
            f'anchor,1000000,{self.epoch},10,-1,11,0,0\n'
            'timer,1100000,-1,30000,25000,11,0,0\n'
            'frame,1100000,-1,16000,-1,11,1,16000\n'
            'frame,1116000,-1,16000,-1,11,0,16000\n',encoding='utf-8')
        self.clock.write_text('timestamp,profile,app_tid,CPU_hz,GPU_hz,MEM_hz,SoC_milliC,CPU_real_hz,power_mw\n'
            f'{self.epoch+100},Handheld,0100000000000001,1020000000,307200000,1331200000,44000,998000000,3200\n'
            f'{self.epoch+116},Docked,0100000000000002,2000000000,999000000,1600000000,55000,1990000000,5000\n',encoding='utf-8')
        self.log.write_text('mono_us=1 program_id=0100000000000001 applet_type=0\n',encoding='utf-8')
    def tearDown(self):self.temp.cleanup()
    def test_original_schema(self):
        data,bad=sysclk(self.clock)
        self.assertEqual(bad,0);self.assertEqual(data[0]['cpu_mhz'],1020)
        self.assertEqual(data[0]['cpu_real_mhz'],998);self.assertEqual(data[0]['soc_c'],44)
        self.assertEqual(data[0]['power_mw'],3200);self.assertNotIn('gpu_real_mhz',data[0])
    def test_title_and_frame_epoch(self):
        summary,frames,_,_=analyze(self.game,self.clock,self.log)
        self.assertEqual(summary['matched_frames'],2)
        self.assertEqual(frames[1]['epoch_ms'],self.epoch+116)
        self.assertEqual(frames[1]['cpu_mhz'],1020) # Other application's closer sample rejected.
        self.assertEqual(frames[1]['clock_delta_ms'],-16)
        self.assertEqual(summary['stages']['timer']['cpu_total_ms'],25)
    def test_missing_title_does_not_join(self):
        summary,_,_,_=analyze(self.game,self.clock)
        self.assertEqual(summary['matched_frames'],0)
    def test_bounded_alignment(self):
        summary,_,_,_=analyze(self.game,self.clock,self.log,max_align_ms=1)
        self.assertEqual(summary['matched_frames'],1)
    def test_clock_jump(self):
        with self.game.open('a') as f:f.write(f'anchor,2000000,{self.epoch+11000},10,-1,11,0,0\n')
        summary,_,_,_=analyze(self.game,self.clock,self.log)
        self.assertEqual(summary['matched_frames'],0)
        self.assertTrue(summary['time_alignment'].startswith('DISABLED'))
    def test_unknown_time_not_guessed(self):
        self.assertIsNone(timestamp('2026-10-20 10:30:00'))
        self.assertIsNotNone(timestamp('2026-10-20 10:30:00',180))
        self.assertIsNone(timestamp('1000'));self.assertIsNone(timestamp('nan'))
    def test_unknown_fork_units_preserved(self):
        self.clock.write_text(f'timestamp,app_tid,cpu,mystery\n{self.epoch+100},0100000000000001,1020,77\n')
        summary,_,_,clocks=analyze(self.game,self.clock,self.log)
        self.assertEqual(clocks[0]['raw']['mystery'],'77')
        self.assertNotIn('cpu_mhz',clocks[0])
        self.assertTrue(any('units unrecognized' in s for s in summary['warnings']))
    def test_measurement_errors(self):
        with self.game.open('a') as f:f.write('timer,broken\n# file_cap_reached\n')
        with self.log.open('a') as f:f.write('dropped=19 perf_failed=1 capped=1/0\n')
        summary,_,_,_=analyze(self.game,self.clock,self.log)
        self.assertGreaterEqual(len(summary['warnings']),3)
    @unittest.skipUnless(importlib.util.find_spec('matplotlib'),'matplotlib not installed')
    def test_cli_graphs_and_inputs_unchanged(self):
        before={p:p.read_bytes() for p in [self.game,self.clock,self.log]}
        out=self.root/'analysis'
        subprocess.run([sys.executable,str(ROOT/'tools/e2e515_perf_analyze.py'),'--game',str(self.game),
            '--sys-clk-csv',str(self.clock),'--stage-log',str(self.log),'--out',str(out)],check=True,stdout=subprocess.DEVNULL)
        self.assertTrue((out/'performance.png').is_file());self.assertTrue((out/'report.md').is_file())
        self.assertEqual(json.loads((out/'summary.json').read_text())['matched_frames'],2)
        for p,raw in before.items():self.assertEqual(p.read_bytes(),raw)

if __name__=='__main__':unittest.main()
