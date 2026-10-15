from pathlib import Path
import sys,unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from e2e5_ui_width_overrides import apply

class PayloadTests(unittest.TestCase):
    def test_both_crashing_pointer_producers_and_consumers_are_full_width(self):
        for name in ['fMods.cpp','ab_MainForm.cpp']:
            original=(ROOT/'upstream/cpp/src'/name).read_text(encoding='utf-8')
            text=apply(name,original)
            self.assertNotIn('static_cast<std::uint32_t>(Switch->UserData)',text)
            self.assertNotIn('static_cast<std::uint32_t>(Switch->UserState)',text)
            self.assertNotIn('static_cast<std::uint32_t>(Button->UserState)',text)
            for line in text.splitlines():
                if any(x in line for x in ['->UserData =','->UserState =']) and 'reinterpret_cast' in line:
                    self.assertNotIn('static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(',line)
    def test_timer_callback_abi_stays_int32(self):
        s=(ROOT/'upstream/cpp/src/types/GI_MessageLoop.hpp').read_text(encoding='utf-8')
        text=apply('types/GI_MessageLoop.hpp',s)
        self.assertEqual(text.count('std::intptr_t UserData;'),1)
        self.assertEqual(text.count('std::int32_t UserData;'),1)
        self.assertIn('std::intptr_t UserState;',text)

if __name__=='__main__':unittest.main()
